#ifndef VIAEMS_PROTOCOL_H
#define VIAEMS_PROTOCOL_H

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <variant>
#include <vector>

#include <cbor11.h>
#include <fstream>

namespace viaems {

typedef std::chrono::time_point<std::chrono::system_clock> FeedTime;
typedef std::variant<uint32_t, float> FeedValue;
typedef std::map<std::string, FeedValue> FeedUpdateTypedef;

struct FeedUpdate : public std::map<std::string, FeedValue> {
  std::chrono::system_clock::time_point time;

  FeedUpdate(std::chrono::system_clock::time_point t)
      : std::map<std::string, FeedValue>(), time(t){};

  FeedUpdate() : FeedUpdate{std::chrono::system_clock::now()} {};
};

typedef std::vector<std::variant<int, std::string>> StructurePath;

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

struct Request {
  uint32_t id;
  std::variant<StructureRequest, GetRequest, SetRequest, PingRequest> request;
  bool is_sent;
  cbor repr;
};

class Protocol {
public:
  Protocol(std::ostream &out) : m_out{out} {};

  std::vector<FeedUpdate> FeedUpdates();
  void NewData(std::string const &);

  std::shared_ptr<Request> Get(get_cb, StructurePath path, void *);
  std::shared_ptr<Request> Structure(structure_cb, void *);
  std::shared_ptr<Request> Ping(ping_cb, void *);
  std::shared_ptr<Request> Set(set_cb, StructurePath, ConfigValue, void *);
  bool Cancel(std::shared_ptr<Request> req);

private:
  std::vector<std::string> m_feed_vars;
  std::vector<FeedUpdate> m_feed_updates;
  std::string m_input_buffer;
  std::ostream &m_out;
  std::deque<std::shared_ptr<Request>> m_requests;

  const int max_inflight_reqs = 1;

  void handle_message_from_ems(cbor m);
  void handle_feed_message_from_ems(cbor::array m);
  void handle_description_message_from_ems(cbor::array m);
  void handle_response_message_from_ems(uint32_t id, cbor response);
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

typedef void (*model_change_cb)(void *ptr);

class Model {
  Protocol &m_protocol;
  model_change_cb change_callback;
  void *change_callback_ptr;

  std::map<StructurePath, std::shared_ptr<NodeModel>> m_model;
  StructureNode root;

  /* Interrogation members */
  std::shared_ptr<Request> structure_req;
  std::vector<std::shared_ptr<Request>> get_reqs;

  void recurse_model_structure(StructureNode node);

  static void handle_model_get(StructurePath path, ConfigValue val, void *ptr);
  static void handle_model_structure(StructureNode root, void *ptr);

public:
  Model(Protocol &protocol, model_change_cb cb, void *p)
      : m_protocol{protocol}, change_callback(cb), change_callback_ptr(p){};

  StructureNode &structure() { return root; };
  viaems::ConfigValue get_value(StructurePath path) {
    return m_model.at(path)->value;
  }

  InterrogationState interrogation_status();
  void interrogate();
};

} // namespace viaems

#endif
