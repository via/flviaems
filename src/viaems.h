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
#include <vector>

#include <nlohmann/json.hpp>
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

typedef std::variant<uint32_t, float, std::string, TableValue, SensorValue,
                     OutputValue>
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

class Protocol {
public:
  Protocol(std::unique_ptr<Connection> conn) : connection{std::move(conn)} {};

  LogChunk FeedUpdates();
  void NewData();

  std::shared_ptr<Request> Get(get_cb, StructurePath path, void *);
  std::shared_ptr<Request> Structure(structure_cb, void *);
  std::shared_ptr<Request> Ping(ping_cb, void *);
  std::shared_ptr<Request> Set(set_cb, StructurePath, ConfigValue, void *);
  void Flash();
  void Bootloader();
  bool Cancel(std::shared_ptr<Request> req);

private:
  std::unique_ptr<Connection> connection;

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
