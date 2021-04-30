#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <numeric>
#include <functional>

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "viaems.h"

#include <nlohmann/json.hpp>
using json = json;

class Connection {
  std::istream &in;
  std::ostream &out;

  std::thread reader_thread;
  std::deque<json> in_messages;
  std::mutex in_mutex;
  Fl_Awake_Handler handler;
  void *handler_ptr;

  bool running;

  static void do_reader_thread(Connection *self) {
    while (self->running) {
      try {
        auto msg = json::from_cbor(self->in, false);
        std::unique_lock<std::mutex> lock(self->in_mutex);
        self->in_messages.push_back(msg);
        lock.unlock();
        Fl::awake(self->handler, self->handler_ptr);
      } catch (json::parse_error &e) {
      }

    }
  }
  public:
    Connection(std::istream &i, std::ostream &o, Fl_Awake_Handler read_handler, void *ptr)
    : in{i}, out{o}, handler{read_handler}, handler_ptr{ptr}, running{true} {
      this->reader_thread = std::thread([](Connection *s) {
        s->do_reader_thread(s); }, this);
    }
    ~Connection() {
      running = false;
      reader_thread.join();
    }

    void Write(const json &msg) {
      json::to_cbor(msg, out);
      out.flush();
    }

    std::optional<json> Read() {
      std::unique_lock<std::mutex> lock(in_mutex);
      if (in_messages.empty()) {
        return {};
      }
      auto msg = std::move(in_messages[0]);
      in_messages.pop_front();
      return msg;
    }
};


class FLViaems {
  MainWindow ui;
  viaems::Protocol protocol;
  viaems::Model model;

  std::unique_ptr<Connection> connector;
  std::shared_ptr<viaems::Request> ping_req;


  static void feed_refresh_handler(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    auto updates = v->protocol.FeedUpdates();
    static std::deque<int> rates;

    /* Keep average over 1 second */
    rates.push_back(updates.points.size());
    if (rates.size() > 20) {
      rates.erase(rates.begin());
    }

    if (updates.points.size() > 0) {
      v->ui.feed_update(std::move(updates));
      v->ui.update_feed_hz(std::accumulate(rates.begin(), rates.end(), 0));
    }
    Fl::repeat_timeout(0.05, v->feed_refresh_handler, v);
  }

  static void failed_ping_callback(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    if (v->ping_req) {
      v->protocol.Cancel(v->ping_req);
      v->ping_req.reset();
    }
    std::cerr << "failed ping" << std::endl;
    v->ui.update_connection_status(false);
  }

  static void interrogate_update(viaems::InterrogationState s, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    v->ui.update_interrogation(s.in_progress, s.complete_nodes, s.total_nodes);
    if (!s.in_progress) {
      v->ui.update_model(&v->model);
    }
  }

  void start_interrogation() {
    model.interrogate(interrogate_update, this);
    Fl::add_timeout(60, failed_structure_callback, this);
  }

  static void value_update(viaems::StructurePath path, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    auto value = v->model.get_value(path);
    v->ui.update_config_value(path, value);
  }

  static void failed_structure_callback(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    auto is = v->model.interrogation_status();
    if (is.in_progress) {
      std::cerr << "redoing structure" << is.complete_nodes << " "
                << is.total_nodes << std::endl;
      v->start_interrogation();
    }
  }

  static void ping_callback(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    static bool first_pong = true;
    Fl::remove_timeout(v->failed_ping_callback);
    v->ui.update_connection_status(true);

    if (first_pong) {
      first_pong = false;
      v->start_interrogation();
    }
  }

  static void pinger(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    Fl::add_timeout(0.5, v->failed_ping_callback, v);
    v->ping_req = v->protocol.Ping(v->ping_callback, v);
    Fl::repeat_timeout(1, v->pinger, v);
  }

  static void flash(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    v->protocol.Flash();
    v->start_interrogation();
  }

  static void bootloader(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    v->protocol.Bootloader();
    v->start_interrogation();
  }

  void initialize_connection() {
    this->connector = std::make_unique<Connection>(std::cin, std::cout,
    read_message, this);
  }

  static void read_message(void *ptr) {
    FLViaems *v = static_cast<FLViaems *>(ptr);
    do {
      auto msg = v->connector->Read();
      if (!msg) {
        break;
      }
      v->protocol.NewData(msg.value());
    } while (true);
  }

  void write_message(const json& msg) {
    if (this->connector != nullptr) {
      this->connector->Write(msg);
    }
  }

public:
  FLViaems() : protocol{std::bind(&FLViaems::write_message, this,
  std::placeholders::_1)}, model{protocol} {
    Fl::lock(); /* Necessary to enable awake() functionality */
    model.set_value_change_callback(value_update, this);
    ui.m_file_flash->callback(flash, this);
    ui.m_file_bootloader->callback(bootloader, this);
    Fl::add_timeout(0.05, feed_refresh_handler, this);
    Fl::add_timeout(1, pinger, this);
    initialize_connection();
  };

  ~FLViaems() {
  };
};

int main() {
  FLViaems controller{};
  Fl::run();
  return 0;
}
