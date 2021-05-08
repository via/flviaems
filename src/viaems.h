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
  std::deque<LogPoint> points;
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

struct SensorValue {};
struct OutputValue {};

typedef std::variant<uint32_t, float, std::string, TableValue, SensorValue,
                     OutputValue>
    ConfigValue;

struct StructureLeaf {
  std::string description;
  std::string type;
  std::vector<std::string> choices;
  StructurePath path;
};

struct StructureNode;
typedef std::vector<StructureNode> StructureList;
typedef std::map<std::string, StructureNode> StructureMap;

typedef std::variant<StructureList, StructureMap, StructureLeaf>
    StructureNodeTypedef;
struct StructureNode : StructureNodeTypedef {
  bool is_map() { return std::holds_alternative<StructureMap>(*this); }

  StructureMap map() { return std::get<StructureMap>(*this); }

  bool is_list() { return std::holds_alternative<StructureList>(*this); }
  StructureList list() { return std::get<StructureList>(*this); }
  bool is_leaf() { return std::holds_alternative<StructureLeaf>(*this); }
  StructureLeaf leaf() { return std::get<StructureLeaf>(*this); }
};

typedef void (*get_cb)(StructurePath path, ConfigValue val, void *ptr);
struct GetRequest {
  get_cb cb;
  StructurePath path;
  void *ptr;
};

typedef void (*structure_cb)(StructureNode top, void *ptr);
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

class Protocol {
public:
  Protocol(std::function<void(const json &)> write_cb) : write_cb{write_cb} {};

  LogChunk FeedUpdates();
  void NewData(const json &j);

  std::shared_ptr<Request> Get(get_cb, StructurePath path, void *);
  std::shared_ptr<Request> Structure(structure_cb, void *);
  std::shared_ptr<Request> Ping(ping_cb, void *);
  std::shared_ptr<Request> Set(set_cb, StructurePath, ConfigValue, void *);
  void Flash();
  void Bootloader();
  bool Cancel(std::shared_ptr<Request> req);

private:
  std::vector<std::string> m_feed_vars;
  LogChunk m_feed_updates;
  std::function<void(const json &)> write_cb;
  std::deque<std::shared_ptr<Request>> m_requests;

  std::chrono::system_clock::time_point zero_time;
  uint32_t last_feed_time = -1;

  const int max_inflight_reqs = 1;

  void handle_feed_message_from_ems(const json &m);
  void handle_description_message_from_ems(const json &m);
  void handle_response_message_from_ems(uint32_t id, const json &response);
  void ensure_sent();
};

class Model;
struct NodeModel {
  struct StructureLeaf node;
  ConfigValue value;

  bool m_pending_write;
  bool valid;
};

struct InterrogationState {
  bool in_progress;
  int total_nodes;
  int complete_nodes;
};

typedef void (*interrogation_change_cb)(InterrogationState, void *ptr);
typedef void (*value_change_cb)(StructurePath path, void *ptr);

class Model {
  Protocol &m_protocol;

  value_change_cb value_cb = nullptr;
  void *value_cb_ptr;

  std::map<StructurePath, std::shared_ptr<NodeModel>> m_model;
  StructureNode root;

  /* Interrogation members */
  interrogation_change_cb interrogate_cb;
  void *interrogate_cb_ptr;
  std::shared_ptr<Request> structure_req;
  std::vector<std::shared_ptr<Request>> get_reqs;

  void recurse_model_structure(StructureNode node);

  static void handle_model_get(StructurePath path, ConfigValue val, void *ptr);
  static void handle_model_set(StructurePath path, ConfigValue val, void *ptr);
  static void handle_model_structure(StructureNode root, void *ptr);

public:
  Model(Protocol &protocol) : m_protocol{protocol} {};

  StructureNode &structure() { return root; };

  viaems::ConfigValue get_value(StructurePath path) {
    return m_model.at(path)->value;
  }

  json to_json();
  void from_json(const json&);

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
