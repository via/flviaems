#include <chrono>
#include <cstdlib>
#include <exception>
#include <streambuf>
#include <iostream>

#include "Log.h"
#include "viaems.h"

#include <nlohmann/json.hpp>
using json = json;

using namespace viaems;

LogChunk Protocol::FeedUpdates() {
  auto updates = std::move(m_feed_updates);
  m_feed_updates = LogChunk{};
  m_feed_updates.keys = m_feed_vars;
  return updates;
}

void Protocol::handle_description_message_from_ems(const json &a) {
  m_feed_vars.clear();
  for (json i : a) {
    m_feed_vars.push_back(i);
  }
  m_feed_updates.keys = m_feed_vars;
}

static std::chrono::system_clock::time_point calculate_zero_point(uint32_t
cpu_time, std::chrono::system_clock::time_point real_time) {
  auto ns_since_zero = std::chrono::nanoseconds{(uint64_t)cpu_time * 250};
  return real_time - ns_since_zero;
}

static std::chrono::system_clock::time_point calculate_real_time(uint32_t
cpu_time, std::chrono::system_clock::time_point zero_time) {
  auto ns_since_zero = std::chrono::nanoseconds{(uint64_t)cpu_time * 250};
  return zero_time + ns_since_zero;
}

void Protocol::handle_feed_message_from_ems(const json &a) {
  if (a.size() != m_feed_vars.size()) {
    return;
  }
  LogPoint update;
  int i;
  for (i = 0; i < m_feed_vars.size(); i++) {
    if (m_feed_vars[i] == "cputime") {
      break;
    }
  }
  if (i == m_feed_vars.size()) {
    return;
  }
  auto cputime = a[i].get<uint32_t>();
  if (cputime < last_feed_time) {
    zero_time = calculate_zero_point(cputime, std::chrono::system_clock::now());
  }
  update.time = calculate_real_time(cputime, zero_time);
  last_feed_time = cputime;
  for (size_t i = 0; i < a.size(); i++) {
    const json &val = a[i];
    if (val.is_number_integer()) {
      update.values.push_back(
          FeedValue(val.get<uint32_t>()));
    } else if (val.is_number_float()) {
      update.values.push_back(
          FeedValue(val.get<float>()));
    }
  }
  m_feed_updates.points.emplace_back(update);
}

static StructureLeaf generate_config_node(const json &entry, StructurePath path) {
  std::string type = entry["_type"];
  std::string desc = "";
  if (entry.contains("description")) {
    desc = entry["description"];
  }

  std::vector<std::string> choices;
  if (entry.contains("choices")) {
    for (auto choice : entry["choices"]) {
      choices.push_back(choice);
    }
  }

  return StructureLeaf{
      .description = desc,
      .type = type,
      .choices = choices,
      .path = path,
  };
}

static StructureNode generate_structure_node_from_cbor(const json &entry,
                                                       StructurePath curpath) {
  if (entry.is_object()) {
    if (entry.contains("_type")) {
      /* This is a leaf node */
      return StructureNode{generate_config_node(entry, curpath)};
    }

    /* This is actually a map, we should descend */
    auto result = StructureMap{};
    for (const auto &map_entry : entry.items()) {
      StructurePath new_path = curpath;
      new_path.push_back(map_entry.key());

      auto child =
          generate_structure_node_from_cbor(map_entry.value(), new_path);
      result.insert(std::pair<std::string, StructureNode>(
          map_entry.key(), child));
    }
    return StructureNode{result};
  } else if (entry.is_array()) {
    auto result = StructureList{};
    int index = 0;
    for (const auto &list_entry : entry) {
      StructurePath new_path = curpath;
      new_path.push_back(index);
      index += 1;

      auto child = generate_structure_node_from_cbor(list_entry, new_path);
      result.push_back(child);
    }
    return StructureNode{result};
  }

  return StructureNode{StructureLeaf{}};
}

static TableAxis generate_table_axis_from_cbor(const json &axis) {
  TableAxis res{};
  res.name = axis["name"];
  for (const auto label : axis["values"]) {
    res.labels.push_back(label);
  }
  return res;
}

static ConfigValue generate_table_value_from_cbor(const json &map) {
  TableValue table;
  int n_axis = map["num-axis"];
  table.title = map["title"];
  table.axis.push_back(
      generate_table_axis_from_cbor(map["horizontal-axis"]));

  if (n_axis == 1) {
    for (const auto datum : map["data"]) {
      table.one.push_back(datum);
    }
  } else if (n_axis == 2) {
    table.axis.push_back(
        generate_table_axis_from_cbor(map["vertical-axis"]));
    for (const auto outter : map["data"]) {
      std::vector<float> values;
      for (const auto value : outter) {
        values.push_back(value);
      }
      table.two.push_back(values);
    }
  }
  return table;
}

bool is_valid_table_cbor(const json &value) {
 if (!value.contains("title") ||
     !value.contains("num-axis") ||
     !value.contains("horizontal-axis")) {
   return false;
 }
 if (!value["horizontal-axis"].contains("name") ||
     !value["horizontal-axis"].contains("values")) {
   return false;
 }
 if (value["num-axis"] == 2) {
   if (!value.contains("vertical-axis") ||
     !value["vertical-axis"].contains("name") ||
     !value["vertical-axis"].contains("values")) {
     return true;
   }
 }
 if (!value.contains("data")) {
   return false;
 }
 return true;
}

static ConfigValue generate_node_value_from_cbor(const json &value) {

  if (value.is_number_integer()) {
    return value.get<uint32_t>();
  }
  if (value.is_number_float()) {
    return value.get<float>();
  }
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (is_valid_table_cbor(value)) {
    return generate_table_value_from_cbor(value);
  }
  return ConfigValue{5.0f};
}

template <typename T> static json cbor_from_value(T v) { return v; }

static json cbor_from_table_axis(TableAxis axis) {
  auto result = json{
      {"name", axis.name},
      {"values", axis.labels},
  };
  return result;
}

static json cbor_from_value(const TableValue &v) {
  auto result = json{
      {"num-axis", v.axis.size()},
      {"title", v.title},
      {"horizontal-axis", cbor_from_table_axis(v.axis[0])},
  };
  if (v.axis.size() == 1) {
    result["data"] = v.one;
  } else {
    result["data"] = v.two;
    result["vertical-axis"] =  cbor_from_table_axis(v.axis[1]);
  }
  return result;
}

static json cbor_from_value(const OutputValue &v) { return json{}; }

static json cbor_from_value(const SensorValue &v) { return json{}; }

void Protocol::handle_response_message_from_ems(uint32_t id, const json &response) {
  if (m_requests.empty()) {
    return;
  }
  auto req = m_requests.front();

  if (id != req->id) {
    return;
  }

  m_requests.pop_front();
  ensure_sent();

  if (std::holds_alternative<PingRequest>(req->request)) {
    auto pingreq = std::get<PingRequest>(req->request);
    pingreq.cb(pingreq.ptr);
  } else if (std::holds_alternative<StructureRequest>(req->request)) {
    auto structurereq = std::get<StructureRequest>(req->request);
    auto c = generate_structure_node_from_cbor(response, {});
    structurereq.cb(c, structurereq.ptr);
  } else if (std::holds_alternative<GetRequest>(req->request)) {
    auto getreq = std::get<GetRequest>(req->request);
    auto val = generate_node_value_from_cbor(response);
    getreq.cb(getreq.path, val, getreq.ptr);
  } else if (std::holds_alternative<SetRequest>(req->request)) {
    auto setreq = std::get<SetRequest>(req->request);
    auto val = generate_node_value_from_cbor(response);
    setreq.cb(setreq.path, val, setreq.ptr);
  }
}

void Protocol::NewData(const json &msg) {
  if (!msg.is_object()) {
    return;
  }

  if (!msg.contains("type")) {
    return;
  }
  if ((msg.contains("success") == 1) && (msg["success"] != true)) {
    return;
  }

  std::string type = msg["type"];

  if (type == "feed" && msg.contains("values")) {
    handle_feed_message_from_ems(msg["values"]);
  } else if (type == "description" && msg.contains("keys")) {
    handle_description_message_from_ems(msg["keys"]);
  } else if (type == "response" && msg.contains("id") && msg.contains("response")) {
    handle_response_message_from_ems(msg["id"], msg["response"]);
  }
}

std::shared_ptr<Request> Protocol::Structure(structure_cb cb, void *v) {
  uint32_t id = rand() % 1024;

  auto wire_request = json{
      {"type", "request"},
      {"method", "structure"},
      {"id", id},
  };
  auto req = std::make_shared<Request>(Request{
      .id = id,
      .request = StructureRequest{cb, v},
      .repr = wire_request,
  });
  m_requests.push_back(req);
  ensure_sent();

  return req;
}

std::shared_ptr<Request> Protocol::Ping(ping_cb cb, void *v) {
  uint32_t id = rand() % 1024;

  auto wire_request = json{
      {"type", "request"},
      {"method", "ping"},
      {"id", id},
  };
  auto req = std::make_shared<Request>(Request{
      .id = id,
      .request = PingRequest{cb, v},
      .repr = wire_request,
  });
  m_requests.push_back(req);
  ensure_sent();

  return req;
}

static json cbor_path_from_structure_path(viaems::StructurePath path) {
  json result;
  for (const auto &p : path) {
    std::visit([&](const auto &v) { result.push_back(v); }, p);
  }
  return result;
}

std::shared_ptr<Request> Protocol::Get(get_cb cb, viaems::StructurePath path,
                                       void *v) {
  uint32_t id = rand() % 1024;

  auto wire_request = json{
      {"type", "request"},
      {"method", "get"},
      {"id", id},
      {"path", cbor_path_from_structure_path(path)},
  };

  auto req = std::make_shared<Request>(Request{
      .id = id,
      .request = GetRequest{cb, path, v},
      .repr = wire_request,
  });
  m_requests.push_back(req);
  ensure_sent();

  return req;
}

std::shared_ptr<Request> Protocol::Set(set_cb cb, viaems::StructurePath path,
                                       viaems::ConfigValue value, void *v) {
  uint32_t id = rand() % 1024;

  auto cval = std::visit(
      [](const auto &v) -> json { return cbor_from_value(v); }, value);
  auto wire_request = json{
      {"type", "request"}, {"method", "set"},
      {"id", id},          {"path", cbor_path_from_structure_path(path)},
      {"value", cval},
  };

  auto req = std::make_shared<Request>(Request{
      .id = id,
      .request = SetRequest{cb, path, value, v},
      .repr = wire_request,
  });
  m_requests.push_back(req);
  ensure_sent();

  return req;
}

void Protocol::Flash() {
  auto wire_request = json{
      {"type", "request"},
      {"method", "flash"},
  };

  auto req = std::make_shared<Request>(Request{
      .request = FlashRequest{},
      .repr = wire_request,
  });
  m_requests.push_back(req);
  ensure_sent();
  m_requests.clear();
}

void Protocol::Bootloader() {
  auto wire_request = json{
      {"type", "request"},
      {"method", "bootloader"},
  };

  auto req = std::make_shared<Request>(Request{
      .request = BootloaderRequest{},
      .repr = wire_request,
  });
  m_requests.push_back(req);
  ensure_sent();
  m_requests.clear();
}

bool Protocol::Cancel(std::shared_ptr<Request> request) {
  for (auto i = m_requests.begin(); i != m_requests.end(); i++) {
    if ((*i)->id == request->id) {
      m_requests.erase(i);
      ensure_sent();
      return true;
    }
  }
  return false;
}

void Protocol::ensure_sent() {
  if (m_requests.empty()) {
    return;
  }

  auto first = m_requests.front();
  if (first->is_sent) {
    return;
  }
  first->is_sent = true;
  this->write_cb(first->repr);
}

void Model::interrogate(interrogation_change_cb cb, void *ptr) {
  interrogate_cb = cb;
  interrogate_cb_ptr = ptr;

  /* First clear any ongoing interrogation commands */
  for (auto r = get_reqs.rbegin(); r != get_reqs.rend(); r++) {
    m_protocol.Cancel(*r);
  }
  get_reqs.clear();
  m_model.clear();

  if (structure_req) {
    m_protocol.Cancel(structure_req);
    structure_req.reset();
  }

  structure_req = m_protocol.Structure(handle_model_structure, this);
}

InterrogationState Model::interrogation_status() {
  int n_nodes = 0;
  int n_complete = 0;

  for (auto &x : m_model) {
    n_nodes++;
    if (x.second->valid) {
      n_complete++;
    }
  }
  return {
      .in_progress = n_nodes == 0 || n_nodes != n_complete,
      .total_nodes = n_nodes,
      .complete_nodes = n_complete,
  };
}

void Model::handle_model_get(StructurePath path, ConfigValue val, void *ptr) {
  Model *model = (Model *)ptr;
  model->m_model.at(path)->value = val;
  model->m_model.at(path)->valid = true;
  model->interrogate_cb(model->interrogation_status(),
                        model->interrogate_cb_ptr);
}

void Model::handle_model_set(StructurePath path, ConfigValue val, void *ptr) {
  Model *model = (Model *)ptr;
  model->m_model.at(path)->value = val;
  model->m_model.at(path)->valid = true;
  if (model->value_cb) {
    model->value_cb(path, model->value_cb_ptr);
  }
}

void Model::handle_model_structure(StructureNode root, void *ptr) {
  Model *model = (Model *)ptr;
  model->root = root;
  model->recurse_model_structure(root);
  model->interrogate_cb(model->interrogation_status(),
                        model->interrogate_cb_ptr);
}

void Model::recurse_model_structure(StructureNode node) {
  if (node.is_leaf()) {
    auto leaf = node.leaf();
    auto nodemodel = std::make_unique<NodeModel>();
    nodemodel->node = leaf;
    m_model.insert(std::make_pair(leaf.path, std::move(nodemodel)));
    get_reqs.push_back(m_protocol.Get(handle_model_get, leaf.path, this));
  } else if (node.is_map()) {
    for (auto child : node.map()) {
      recurse_model_structure(child.second);
    }
  } else if (node.is_list()) {
    for (auto child : node.list()) {
      recurse_model_structure(child);
    }
  }
}

void Model::set_value(StructurePath path, ConfigValue value) {
  m_protocol.Set(handle_model_set, path, value, this);
}

