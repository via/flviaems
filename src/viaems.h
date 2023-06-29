#ifndef VIAEMS_PROTOCOL_H
#define VIAEMS_PROTOCOL_H

#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <variant>
#include <optional>
#include <vector>
#include <thread>
#include <mutex>

#include <iostream>
#include <nlohmann/json.hpp>
#include <libusb-1.0/libusb.h>
using json = nlohmann::json;

namespace viaems {

typedef std::chrono::time_point<std::chrono::system_clock> FeedTime;
typedef std::variant<uint32_t, float> FeedValue;

typedef std::vector<std::variant<int, std::string>> StructurePath;

struct LogPoint {
  std::chrono::system_clock::time_point time;
  std::vector<viaems::FeedValue> values;
};

struct LogChunk {
  std::vector<LogPoint> points;
  std::vector<std::string> keys;
};

struct TableAxis {
  std::string name;
  std::vector<float> labels;
};

struct TableValue {
  std::string title;
  std::vector<TableAxis> axis;
  std::vector<float> one;
  std::vector<std::vector<float>> two;

  void resize(int R, int C);
};


struct SensorValue {
  std::string source;
  std::string method;

  uint32_t pin;
  float lag;

  struct {
    uint32_t max;
    uint32_t min;
    float value;
  } fault;

  float range_min;
  float range_max;
  float raw_min;
  float raw_max;

  float const_value;

  struct {
    float a;
    float b;
    float c;
    float bias;
  } therm;

  struct {
    uint32_t capture_width;
    uint32_t total_width;
    uint32_t offset;
  } window;
};

struct OutputValue {
  float angle;
  bool inverted;
  uint32_t pin;
  std::string type;
};

typedef std::variant<uint32_t, float, bool, std::string, TableValue,
                     SensorValue, OutputValue>
    ConfigValue;

struct StructureLeaf {
  std::string description;
  std::string type;
  std::vector<std::string> choices;
  StructurePath path;
};

struct StructureNode {
  using StructureMap = std::map<std::string, StructureNode>;
  using StructureList = std::vector<StructureNode>;
  using StructureType =
      std::variant<StructureList, StructureMap, StructureLeaf>;

  StructureType data;

  bool is_map() { return std::holds_alternative<StructureMap>(this->data); }
  StructureMap map() { return std::get<StructureMap>(this->data); }

  bool is_list() const {
    return std::holds_alternative<StructureList>(this->data);
  }
  StructureList list() { return std::get<StructureList>(this->data); }

  bool is_leaf() const {
    return std::holds_alternative<StructureLeaf>(this->data);
  }
  StructureLeaf leaf() { return std::get<StructureLeaf>(this->data); }
};

struct Configuration {
  std::chrono::system_clock::time_point save_time;
  std::string name;
  StructureNode structure;
  std::map<std::string, StructureNode> types;
  std::map<StructurePath, ConfigValue> values;

  std::optional<ConfigValue> get(StructurePath path) const {
    auto val = values.find(path);
    if (val == values.end()) {
      return {};
    }
    return val->second;
  }

  void from_json(const json &);
  json to_json() const;
};

typedef void (*get_cb)(StructurePath path, ConfigValue val, void *ptr);
struct GetRequest {
  get_cb cb;
  StructurePath path;
  void *ptr;
};

typedef void (*structure_cb)(StructureNode top,
                             std::map<std::string, StructureNode> types,
                             void *ptr);
struct StructureRequest {
  structure_cb cb;
  void *ptr;
};

typedef void (*ping_cb)(void *ptr);
struct PingRequest {
  ping_cb cb;
  void *ptr;
};

typedef void (*set_cb)(StructurePath path, ConfigValue val, void *ptr);
struct SetRequest {
  set_cb cb;
  StructurePath path;
  ConfigValue val;
  void *ptr;
};

struct FlashRequest {};
struct BootloaderRequest {};

struct Request {
  uint32_t id;
  std::variant<StructureRequest, GetRequest, SetRequest, PingRequest,
               FlashRequest, BootloaderRequest>
      request;
  bool is_sent;
  json repr;
};

class Connection {
public:
  virtual void Write(const json &msg) = 0;
  virtual std::optional<json> Read() = 0;
  virtual ~Connection() {}
};

class UsbConnection : public Connection {

public:
  UsbConnection() {
  }
  virtual ~UsbConnection() {
    stopped = true;
    read_thread.join();
    if (devh) {
      libusb_close(devh);
    }
    libusb_exit(NULL);
  }

  bool Connect(uint16_t vid = USB_VID, uint16_t pid = USB_PID) {
    int rc = libusb_init(NULL);
    if (rc < 0) {
      return false;
    }
    devh = libusb_open_device_with_vid_pid(NULL, vid, pid);
    if (!devh) {
      return false;
    }

    for (int if_num = 0; if_num < 2; if_num++) {
        if (libusb_kernel_driver_active(devh, if_num)) {
            libusb_detach_kernel_driver(devh, if_num);
        }
        rc = libusb_claim_interface(devh, if_num);
        if (rc < 0) {
            return false;
        }
    }
    /* Start configuring the device:
     * - set line state
     */
    constexpr uint32_t ACM_CTRL_DTR = 0x01;
    constexpr uint32_t ACM_CTRL_RTS = 0x02;
    rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS,
                                0, NULL, 0, 0);
    if (rc < 0) {
      return false;
    }

    /* - set line encoding: here 9600 8N1
     * 9600 = 0x2580 ~> 0x80, 0x25 in little endian
     */
    unsigned char encoding[] = { 0x80, 0x25, 0x00, 0x00, 0x00, 0x00, 0x08 };
    rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding,
                                sizeof(encoding), 0);
    if (rc < 0) {
      return false;
    }

    read_thread = std::thread(read_thread_entrypoint, this);
    return true;
  }

  virtual void Write(const json &msg) {
    auto payload = json::to_cbor(msg);
    int actual_length;
    if (libusb_bulk_transfer(devh, 0x1, payload.data(), payload.size(),
                             &actual_length, 0) < 0) {
        fprintf(stderr, "Error while sending char\n");
    }
  }

  virtual std::optional<json> Read() { 
    if (outgoing.size() == 0) {
      std::lock_guard<std::mutex> l{incoming_lock};
      if (incoming.size() == 0) {
        return nullptr;
      }
      outgoing = std::move(incoming);
    }

    auto res = std::move(outgoing[0]);
    outgoing.erase(outgoing.begin()); // TODO: do better
    return res;
  }


private:
   std::thread read_thread;
   std::mutex incoming_lock;
   bool stopped;
   std::vector<json> incoming;
   std::vector<json> outgoing;
   struct libusb_device_handle *devh;
   static constexpr uint16_t USB_VID = 0x1209;
   static constexpr uint16_t USB_PID = 0x2041;

   static void read_callback(struct libusb_transfer *xfer) {
     const uint8_t *rxbuf = xfer->buffer;
     const size_t length = xfer->actual_length;
     UsbConnection *self = (UsbConnection *)xfer->user_data;

     try {
       json obj = json::from_cbor(rxbuf, rxbuf + length, false);
       std::lock_guard<std::mutex> l{self->incoming_lock};
       self->incoming.push_back(obj);
     } catch (...) {
       std::cerr << "exception reading" << std::endl;
     }

     libusb_fill_bulk_transfer(xfer, self->devh, 0x81, xfer->buffer, xfer->length, read_callback, self, 1000);

     int rc = libusb_submit_transfer(xfer);

   }

   static void read_thread_entrypoint(UsbConnection *self) {
     struct {
       struct libusb_transfer *xfer;
       uint8_t buf[16384];
     } xfers[4];
       

     for (auto &x : xfers) {
       x.xfer = libusb_alloc_transfer(0);
       if (!x.xfer) {
         return;
       }
       libusb_fill_bulk_transfer(x.xfer, self->devh, 0x81, x.buf, sizeof(x.buf), read_callback, self, 1000);

       int rc = libusb_submit_transfer(x.xfer);
       if (rc != 0) {
         return;
       }
     }

     while (!self->stopped) {
       libusb_handle_events(NULL);
     }
   }
};

class Protocol {
public:
  Protocol(std::unique_ptr<Connection> conn) : connection{std::move(conn)} {};

  LogChunk FeedUpdates();
  void NewData();

  void SetTrace(int level) {
    this->trace = level;
  }

  std::shared_ptr<Request> Get(get_cb, StructurePath path, void *);
  std::shared_ptr<Request> Structure(structure_cb, void *);
  std::shared_ptr<Request> Ping(ping_cb, void *);
  std::shared_ptr<Request> Set(set_cb, StructurePath, ConfigValue, void *);
  void Flash();
  void Bootloader();
  bool Cancel(std::shared_ptr<Request> req);

private:
  std::unique_ptr<Connection> connection;
  int trace = 0;

  std::vector<std::string> m_feed_vars;
  LogChunk m_feed_updates;
  std::function<void(const json &)> write_cb;
  std::deque<std::shared_ptr<Request>> m_requests;

  std::chrono::system_clock::time_point zero_time;
  uint32_t last_feed_time = -1;

  const int max_inflight_reqs = 1;

  void handle_feed_message_from_ems(const json &m);
  void handle_description_message_from_ems(const json &m);
  void handle_response_message_from_ems(const json &msg);
  void ensure_sent();
};

struct InterrogationState {
  bool in_progress;
  int total_nodes;
  int complete_nodes;
};

typedef void (*interrogation_change_cb)(InterrogationState, void *ptr);
typedef void (*value_change_cb)(StructurePath path, void *ptr);

class Model {
  std::shared_ptr<Protocol> protocol;

  value_change_cb value_cb = nullptr;
  void *value_cb_ptr;

  Configuration config;

  /* Interrogation members */
  InterrogationState interrogation_state;
  interrogation_change_cb interrogate_cb = nullptr;
  void *interrogate_cb_ptr;
  std::shared_ptr<Request> structure_req;
  std::vector<std::shared_ptr<Request>> get_reqs;

  static void handle_model_get(StructurePath path, ConfigValue val, void *ptr);
  static void handle_model_set(StructurePath path, ConfigValue val, void *ptr);
  static void handle_model_structure(StructureNode root,
                                     std::map<std::string, StructureNode> types,
                                     void *ptr);

public:
  const Configuration &configuration() const { return config; };
  void set_configuration(const Configuration &c);
  void set_protocol(std::shared_ptr<Protocol>);

  void set_value_change_callback(value_change_cb cb, void *ptr) {
    value_cb = cb;
    value_cb_ptr = ptr;
  }

  InterrogationState interrogation_status();
  void interrogate(interrogation_change_cb cb, void *ptr);
  void set_value(StructurePath path, ConfigValue value);
};

} // namespace viaems

#endif
