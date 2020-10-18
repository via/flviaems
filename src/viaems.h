#ifndef VIAEMS_PROTOCOL_H
#define VIAEMS_PROTOCOL_H

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

typedef std::variant<uint32_t, float> FeedValue;
typedef std::map<std::string, FeedValue> FeedUpdate;

typedef std::vector<std::variant<int, std::string>> StructurePath;

struct TableNode {};
struct SensorNode {};
struct OutputNode {};

typedef std::variant<uint32_t, float, std::string, TableNode, SensorNode,
                     OutputNode>
    ConfigValue;

struct ConfigNode {
  std::string description;
  std::string type;
  std::vector<std::string> choices;
  StructurePath path;
};

struct StructureNode;
typedef std::vector<StructureNode> StructureList;
typedef std::map<std::string, StructureNode> StructureMap;

struct StructureNode {
  std::variant<std::shared_ptr<StructureList>, std::shared_ptr<StructureMap>,
               std::shared_ptr<ConfigNode>>
      contents;
  bool is_map() {
    return std::holds_alternative<std::shared_ptr<StructureMap>>(
        this->contents);
  }
  std::shared_ptr<StructureMap> map() {
    return std::get<std::shared_ptr<StructureMap>>(this->contents);
  }
  bool is_list() {
    return std::holds_alternative<std::shared_ptr<StructureList>>(
        this->contents);
  }
  std::shared_ptr<StructureList> list() {
    return std::get<std::shared_ptr<StructureList>>(this->contents);
  }
  bool is_leaf() {
    return std::holds_alternative<std::shared_ptr<ConfigNode>>(this->contents);
  }
  std::shared_ptr<ConfigNode> leaf() {
    return std::get<std::shared_ptr<ConfigNode>>(this->contents);
  }
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

struct Request {
  uint32_t id;
  std::variant<StructureRequest, GetRequest, PingRequest> request;
  bool is_sent;
  cbor repr;
};

class Protocol {
public:
  Protocol(std::ostream &out) : m_out{out} { m_log.open("log"); }

  std::vector<FeedUpdate> FeedUpdates();
  void NewData(std::string const &data);

  std::shared_ptr<Request> Get(get_cb, StructurePath path, void *);
  std::shared_ptr<Request> Structure(structure_cb, void *);
  std::shared_ptr<Request> Ping(ping_cb, void *);
  bool Cancel(std::shared_ptr<Request> req);

private:
  std::vector<std::string> m_feed_vars;
  std::vector<FeedUpdate> m_feed_updates;
  std::string m_input_buffer;
  std::ostream &m_out;
  std::deque<std::shared_ptr<Request>> m_requests;
  std::ofstream m_log;

  const int max_inflight_reqs = 1;

  void handle_message_from_ems(cbor m);
  void handle_feed_message_from_ems(cbor::array m);
  void handle_description_message_from_ems(cbor::array m);
  void handle_response_message_from_ems(uint32_t id, cbor response);
  void ensure_sent();
};

struct NodeModel {
  struct ConfigNode node;
  std::shared_ptr<ConfigValue> value;

  //  ViaemsProtocol &m_protocol;
  bool m_pending_write;

  bool is_valid() { return value != NULL; }

  bool is_waiting() { return (value == NULL) || m_pending_write; }
};

struct InterrogationState {
  bool in_progress;
  int total_nodes;
  int complete_nodes;
};

typedef void (*interrogate_cb)(InterrogationState s, void *ptr);

class Model {
  Protocol &m_protocol;
  std::map<StructurePath, std::unique_ptr<NodeModel>> m_model;
  StructureNode root;

  /* Interrogation members */
  std::shared_ptr<Request> structure_req;
  std::vector<std::shared_ptr<Request>> get_reqs;
  interrogate_cb interrogation_callback;
  void *interrogation_callback_ptr;

  void recurse_model_structure(StructureNode node);

  static void handle_model_get(StructurePath path, ConfigValue val, void *ptr);
  static void handle_model_structure(StructureNode root, void *ptr);

public:
  Model(Protocol &protocol) : m_protocol{protocol} {};

  StructureNode &structure() { return root; };
  std::shared_ptr<viaems::ConfigValue> get_value(StructurePath path) {
    return m_model.at(path)->value;
  }

  InterrogationState interrogation_status();
  void interrogate(interrogate_cb, void *ptr);
};

} // namespace viaems

#endif
