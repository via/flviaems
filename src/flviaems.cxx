#include <atomic>
#include <cstdio>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>

#include <pstream.h>
#include <termios.h>
#include <unistd.h>

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Window.H>

#include "fdstream.h"
#include "viaems.h"

#include <nlohmann/json.hpp>
using json = json;

struct ThreadedJsonInterface {
  std::shared_ptr<std::ostream> writer;
  std::shared_ptr<std::istream> reader;

  std::thread reader_thread;
  std::vector<json> messages;
  std::mutex in_mutex;
  Fl_Awake_Handler handler;
  void *handler_ptr;

  std::atomic<bool> running;

  static void do_reader_thread(ThreadedJsonInterface *self) {
    int i = 0;
    while (self->running) {
      try {
        auto msg = json::from_cbor(*self->reader, false);
        std::unique_lock<std::mutex> lock(self->in_mutex);
        self->messages.push_back(std::move(msg));
        lock.unlock();
      } catch (json::parse_error &e) {
      }
    }
  }

public:
  ThreadedJsonInterface(std::shared_ptr<std::istream> is,
                        std::shared_ptr<std::ostream> os,
                        Fl_Awake_Handler read_handler, void *ptr)
      : writer{os}, reader{is}, handler{read_handler}, handler_ptr{ptr} {
    running = true;
    this->reader_thread = std::thread(
        [](ThreadedJsonInterface *s) { s->do_reader_thread(s); }, this);
  }

  ~ThreadedJsonInterface() {
    running = false;
    reader_thread.join();
  }

  void Write(const json &msg) {
    json::to_cbor(msg, *writer);
    writer->flush();
  }

  std::vector<json> Read() {
    std::unique_lock<std::mutex> lock(in_mutex);
    auto msg = std::move(messages);
    messages = std::vector<json>{};
    messages.reserve(msg.size());
    return msg;
  }
};

class ExecConnection : public viaems::Connection {
  std::shared_ptr<redi::pstream> stream;
  std::unique_ptr<ThreadedJsonInterface> conn;

public:
  ExecConnection(Fl_Awake_Handler read_handler, void *ptr, std::string path) {
    stream = std::make_shared<redi::pstream>(path);
    conn = std::make_unique<ThreadedJsonInterface>(stream, stream, read_handler,
                                                   ptr);
  }

  virtual void Write(const json &msg) { conn->Write(msg); }
  virtual std::vector<json> Read() { return conn->Read(); }
};

class DevConnection : public viaems::Connection {
  std::unique_ptr<ThreadedJsonInterface> conn;
  int fd;
  std::shared_ptr<fdistream> istream;
  std::shared_ptr<fdostream> ostream;

  bool set_raw_mode() {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
      return false;
    }

    cfmakeraw(&tty);
    if (tcsetattr(fd, 0, &tty) != 0) {
      return false;
    }
    return true;
  }

public:
  DevConnection(Fl_Awake_Handler read_handler, void *ptr, std::string path) {
    fd = open(path.c_str(), O_RDWR);
    if (fd < 0) {
      throw std::runtime_error{"Failed to open device"};
    }
    set_raw_mode();
    istream = std::make_unique<fdistream>(fd);
    ostream = std::make_unique<fdostream>(fd);
    conn = std::make_unique<ThreadedJsonInterface>(istream, ostream,
                                                   read_handler, ptr);
  }

  virtual ~DevConnection() { close(fd); }
  virtual void Write(const json &msg) { conn->Write(msg); }
  virtual std::vector<json> Read() { return conn->Read(); }
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

    if (v->protocol) {
      v->protocol->NewData();
    }

    auto updates =
        v->protocol ? v->protocol->FeedUpdates() : viaems::LogChunk{};
    static std::vector<int> rates;

    /* Keep average over 1 second */
    rates.push_back(updates.points.size());
    if (rates.size() > 20) {
      rates.erase(rates.begin());
    }

    if (updates.points.size() > 0) {
      std::map<std::string, viaems::FeedValue> status;
      for (unsigned int i = 0; i < updates.keys.size(); i++) {
        status.insert(
            std::make_pair(updates.keys[i], updates.points[0].values[i]));
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

  static void initialize_simulator(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    auto conn = std::make_unique<ExecConnection>(
        v->message_available, v, "/home/via/dev/viaems/obj/hosted/viaems");
    v->protocol = std::make_unique<viaems::Protocol>(std::move(conn));
    v->model.set_protocol(v->protocol);
    v->offline = false;
  }

  static void initialize_device(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    auto conn = std::make_unique<DevConnection>(v->message_available, v,
                                                "/dev/ttyACM0");
    v->protocol = std::make_unique<viaems::Protocol>(std::move(conn));
    v->model.set_protocol(v->protocol);
    v->offline = false;
  }

  static void initialize_offline(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    v->protocol.reset();
    v->model.set_protocol(v->protocol);
    v->offline = true;
  }

  static void message_available(void *ptr) {
    FLViaems *v = static_cast<FLViaems *>(ptr);
  }

  void load_config(viaems::Configuration conf) {
    model.set_configuration(conf);
    ui.update_model(&model);

    /* Write all new config to target explicitly */
    if (offline) {
      return;
    }
    for (auto &[path, value] : conf.values) {
      model.set_value(path, value);
    }
  }

  static void export_config(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    const char *filename = fl_file_chooser("Export", "*.json", "", 0);
    if (filename == nullptr) {
      return;
    }
    auto jsonconf = v->model.configuration().to_json();
    std::ofstream dump_file{filename};
    dump_file << jsonconf.dump(4);
    dump_file.close();
  }

  static void import_config(Fl_Widget *w, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);
    const char *filename = fl_file_chooser("Import", "*.json", "", 0);
    if (filename == nullptr) {
      return;
    }
    json jsonconf;

    std::ifstream load_file{filename};
    load_file >> jsonconf;
    load_file.close();

    viaems::Configuration conf;
    conf.from_json(jsonconf);
    v->load_config(conf);
  }

public:
  FLViaems() {
    Fl::lock(); /* Necessary to enable awake() functionality */

    log_reader = std::make_shared<Log>("log.vlog");
    log_writer = std::make_shared<ThreadedWriteLog>("log.vlog");
    ui.update_log(log_reader);

    Fl::add_timeout(0.05, feed_refresh_handler, this);
    Fl::add_timeout(1, pinger, this);

    ui.m_target_flash->callback(flash, this);
    ui.m_target_bootloader->callback(bootloader, this);
    ui.m_file_open->callback(select_log, this);
    ui.m_file_export->callback(export_config, this);
    ui.m_file_import->callback(import_config, this);
    ui.m_connection_simulator->callback(initialize_simulator, this);
    ui.m_connection_device->callback(initialize_device, this);
    ui.m_connection_offline->callback(initialize_offline, this);
    ui.set_load_config_callback(
        std::bind(&FLViaems::load_config, this, std::placeholders::_1));
    model.set_value_change_callback(value_update, this);
  };

  ~FLViaems(){};
};

int main() {
  FLViaems controller{};
  Fl::run();
  return 0;
}
