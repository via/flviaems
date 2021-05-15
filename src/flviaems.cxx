#include <cstdio>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <atomic>
#include <exception>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_File_Chooser.H>

#include "viaems.h"

#include <nlohmann/json.hpp>
using json = json;

struct ThreadedJsonInterface {
  FILE *write_file;
  FILE *read_file;

  std::thread reader_thread;
  std::deque<json> in_messages;
  std::mutex in_mutex;
  Fl_Awake_Handler handler;
  void *handler_ptr;

  std::atomic<bool> running;

  static void do_reader_thread(ThreadedJsonInterface *self) {
    while (self->running) {
      try {
        auto msg = json::from_cbor(self->read_file, false);
        std::unique_lock<std::mutex> lock(self->in_mutex);
        self->in_messages.push_back(std::move(msg));
        lock.unlock();
        Fl::awake(self->handler, self->handler_ptr);
      } catch (json::parse_error &e) {
      }
    }
  }

public:
  ThreadedJsonInterface(FILE *read, FILE *write, Fl_Awake_Handler read_handler, void *ptr)
    : write_file{write}, read_file{read}, handler{read_handler}, handler_ptr{ptr} {
      running = true;
      this->reader_thread =
        std::thread([](ThreadedJsonInterface *s) { s->do_reader_thread(s); }, this);
    }

  ~ThreadedJsonInterface() {
    running = false;
    reader_thread.detach();
  }

  void Write(const json &msg) {
    auto encoded = json::to_cbor(msg);
    fwrite(encoded.data(), encoded.size(), 1, write_file);
    fflush(write_file);
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

class ExecConnection : public viaems::Connection {
  int pid;
  std::unique_ptr<ThreadedJsonInterface> conn;
  
public:
  ExecConnection(Fl_Awake_Handler read_handler, void *ptr, std::string path) {
    int outfds[2];
    int infds[2];
    pipe(outfds);
    pipe(infds);

    if ((pid = fork()) == 0) {
      dup2(outfds[0], STDIN_FILENO);
      dup2(infds[1], STDOUT_FILENO);
      char *new_argv[] = {strdup(path.c_str()), NULL};
      char *new_envp[] = {NULL};
      if (execve(path.c_str(), new_argv, new_envp) < 0) {
        throw std::runtime_error{strerror(errno)};
      }
    } else {
      FILE *write_file = fdopen(outfds[1], "wb");
      FILE *read_file = fdopen(infds[0], "rb");
      if (!write_file || !read_file) {
        throw std::runtime_error{"Unable to open pipes to viaems"};
      }
      conn = std::make_unique<ThreadedJsonInterface>(read_file, write_file, read_handler, ptr);
    }

  }

  virtual ~ExecConnection() {
    kill(pid, SIGTERM);
  }
  virtual void Write(const json &msg) {conn->Write(msg);}
  virtual std::optional<json> Read() {return conn->Read();}
};



class FLViaems {
  MainWindow ui;
  viaems::Model model;

  std::shared_ptr<viaems::Protocol> protocol;
  std::shared_ptr<Log> log_reader;
  std::shared_ptr<ThreadedWriteLog> log_writer;
  std::shared_ptr<viaems::Request> ping_req;

  bool offline = true;

  static void feed_refresh_handler(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    auto updates = v->protocol ? v->protocol->FeedUpdates() : viaems::LogChunk{};
    static std::deque<int> rates;

    /* Keep average over 1 second */
    rates.push_back(updates.points.size());
    if (rates.size() > 20) {
      rates.erase(rates.begin());
    }

    if (updates.points.size() > 0) {
      std::map<std::string, viaems::FeedValue> status;
      for (unsigned int i = 0; i < updates.keys.size(); i++) {
        status.insert(std::make_pair(updates.keys[i], updates.points[0].values[i]));
      }

      v->ui.feed_update(status);
      v->ui.update_feed_hz(std::accumulate(rates.begin(), rates.end(), 0));
      v->log_writer->WriteChunk(std::move(updates));
    }
    Fl::repeat_timeout(0.05, v->feed_refresh_handler, v);
  }

  static void failed_ping_callback(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    if (v->ping_req) {
      if (v->protocol) {
        v->protocol->Cancel(v->ping_req);
      }
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
      v->log_writer->SaveConfig(v->model.configuration());
    }
  }

  void start_interrogation() {
    model.interrogate(interrogate_update, this);
    Fl::add_timeout(60, failed_structure_callback, this);
  }

  static void value_update(viaems::StructurePath path, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    auto value = v->model.configuration().get(path).value_or(uint32_t{0});
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
    static bool done = false;

    Fl::add_timeout(0.5, v->failed_ping_callback, v);
    if (v->protocol) {
      v->ping_req = v->protocol->Ping(v->ping_callback, v);
    }
    Fl::repeat_timeout(1, v->pinger, v);
  }

  static void flash(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    v->protocol->Flash();
    v->start_interrogation();
  }

  static void bootloader(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    v->protocol->Bootloader();
    v->start_interrogation();
  }

  static void select_log(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    const char *filename = fl_file_chooser("Select Log", "*.vlog", "", 0);
    if (filename == nullptr) {
      return;
    }
    v->log_reader = std::make_shared<Log>(filename);
    v->log_writer = std::make_shared<ThreadedWriteLog>(filename);
    v->ui.update_log(v->log_reader);
  }

  static void initialize_simulator(Fl_Widget *w,  void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    auto conn = std::make_unique<ExecConnection>(v->message_available, v, "/home/via/dev/viaems/obj/hosted/viaems");
    v->protocol = std::make_unique<viaems::Protocol>(std::move(conn));
    v->model.set_protocol(v->protocol);
    v->offline = false;
  }

  static void initialize_offline(Fl_Widget *w,  void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    v->protocol.reset();
    v->model.set_protocol(v->protocol);
    v->offline = false;
  }

  static void message_available(void *ptr) {
    FLViaems *v = static_cast<FLViaems *>(ptr);
    if (v->protocol) {
      v->protocol->NewData();
    }
  }

public:
  FLViaems() {
    Fl::lock(); /* Necessary to enable awake() functionality */

    log_reader = std::make_shared<Log>("log.vlog");
    log_writer = std::make_shared<ThreadedWriteLog>("log.vlog");
    ui.update_log(log_reader);

    Fl::add_timeout(0.05, feed_refresh_handler, this);
    Fl::add_timeout(1, pinger, this);

    model.set_value_change_callback(value_update, this);
    ui.m_file_flash->callback(flash, this);
    ui.m_file_bootloader->callback(bootloader, this);
    ui.m_log_select->callback(select_log, this);
    ui.m_connection_simulator->callback(initialize_simulator, this);
    ui.m_connection_offline->callback(initialize_offline, this);

  };

  ~FLViaems(){};
};

int main() {
  FLViaems controller{};
  Fl::run();
  return 0;
}
