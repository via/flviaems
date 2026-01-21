#include <chrono>
#include <cstdlib>
#include <exception>
#include <streambuf>

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

static std::chrono::system_clock::time_point
calculate_zero_point(uint32_t cpu_time,
                     std::chrono::system_clock::time_point real_time) {
  auto ns_since_zero = std::chrono::nanoseconds{(uint64_t)cpu_time * 250};
  return real_time - ns_since_zero;
}

static std::chrono::system_clock::time_point
calculate_real_time(uint32_t cpu_time,
                    std::chrono::system_clock::time_point zero_time) {
  auto ns_since_zero = std::chrono::nanoseconds{(uint64_t)cpu_time * 250};
  return zero_time + ns_since_zero;
}

void Protocol::handle_feed_message_from_ems(const json &a) {
  if (a.size() != m_feed_vars.size()) {
    return;
  }
  LogPoint update;
  unsigned int i;
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
      update.values.push_back(FeedValue(val.get<uint32_t>()));
    } else if (val.is_boolean()) {
      update.values.push_back(FeedValue((uint32_t)val.get<bool>()));
    } else if (val.is_number_float()) {
      update.values.push_back(FeedValue(val.get<float>()));
    }
  }
  m_feed_updates.points.emplace_back(update);
}

static StructureLeaf generate_config_node(const json &entry,
                                          StructurePath path) {
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
    auto result = StructureNode::StructureMap{};
    for (const auto &map_entry : entry.items()) {
      StructurePath new_path = curpath;
      new_path.push_back(map_entry.key());

      auto child =
          generate_structure_node_from_cbor(map_entry.value(), new_path);
      result.insert(
          std::pair<std::string, StructureNode>(map_entry.key(), child));
    }
    return StructureNode{result};
  } else if (entry.is_array()) {
    auto result = StructureNode::StructureList{};
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

static std::map<std::string, StructureNode>
generate_types_from_cbor(const json &entry) {
  if (!entry.is_object()) {
    return {};
  }

  std::map<std::string, StructureNode> types;
  for (auto &[name, t] : entry.items()) {
    types[name] = generate_structure_node_from_cbor(t, {});
  }
  return types;
}

void TableValue::resize(int R, int C) {
  if (axis.size() == 2) {
    int oldC = axis[0].labels.size();
    int oldR = axis[1].labels.size();

    while (R > oldR) {
      axis[1].labels.push_back(axis[1].labels.back());
      two.push_back(two.back());
      oldR++;
    }
    while (R < oldR) {
      axis[1].labels.pop_back();
      two.pop_back();
      oldR--;
    }

    while (C > oldC) {
      axis[0].labels.push_back(axis[0].labels.back());
      for (auto &row : two) {
        row.push_back(row.back());
      }
      oldC++;
    }
    while (C < oldC) {
      axis[0].labels.pop_back();
      for (auto &row : two) {
        row.pop_back();
      }
      oldC--;
    }
  } else {
    int oldR = axis[0].labels.size();
    while (R > oldR) {
      axis[0].labels.push_back(axis[0].labels.back());
      one.push_back(one.back());
      oldR++;
    }
    while (R < oldR) {
      axis[0].labels.pop_back();
      one.pop_back();
      oldR--;
    }
  }
}

static TableAxis generate_table_axis_from_cbor(const json &axis) {
  TableAxis res{};
  res.name = axis["name"];
  for (const auto &label : axis["values"]) {
    res.labels.push_back(label);
  }
  return res;
}

static ConfigValue generate_table_value_from_cbor(const json &map) {
  TableValue table;
  table.title = map["title"];

  /* Handle backwards compat for when tables are one type differentiated by
   * "num-axis". Otherwise assume a lack of vertical axis means one axis */
  if (((map.count("num-axis") > 0) && map["num-axis"] == 1) ||
      map.count("vertical-axis") == 0) {
    table.axis.push_back(generate_table_axis_from_cbor(map["horizontal-axis"]));
    for (const auto &datum : map["data"]) {
      table.one.push_back(datum);
    }
  } else {
    table.axis.push_back(generate_table_axis_from_cbor(map["horizontal-axis"]));
    table.axis.push_back(generate_table_axis_from_cbor(map["vertical-axis"]));
    for (const auto &outter : map["data"]) {
      std::vector<float> values;
      for (const auto &value : outter) {
        values.push_back(value);
      }
      table.two.push_back(values);
    }
  }
  return table;
}

static ConfigValue generate_output_value_from_cbor(const json &map) {
  OutputValue v;
  v.angle = map.at("angle");
  auto inverted = map.at("inverted");
  if (inverted.is_boolean()) {
    v.inverted = inverted;
  } else {
    v.inverted = inverted == 1 ? true : false;
  }
  v.pin = map.at("pin");
  v.type = map.at("type");
  return v;
}

static ConfigValue generate_sensor_value_from_cbor(const json &map) {
  SensorValue v;
  v.source = map.at("source");
  v.method = map.at("method");
  v.pin = map.at("pin");
  v.lag = map.at("lag");

  v.fault.min = map.at("fault-min");
  v.fault.max = map.at("fault-max");
  v.fault.value = map.at("fault-value");

  if (v.source == "const") {
    v.const_value = map.at("fixed-value");
  } else {
    if (v.method == "therm") {
      v.therm.a = map.at("therm-a");
      v.therm.b = map.at("therm-b");
      v.therm.c = map.at("therm-c");
      v.therm.bias = map.at("therm-bias");
    } else if (v.method == "linear") {
      v.range_min = map.at("range-min");
      v.range_max = map.at("range-max");
      v.raw_min = map.at("raw-min");
      v.raw_max = map.at("raw-max");
    }
  }

  if (v.method == "linear-window") {
    v.window.offset = map.at("window-offset");
    v.window.opening = map.at("window-capture-opening");
    v.window.windows_per_cycle = map.at("window-count");
  }

  return v;
}

bool is_valid_table_cbor(const json &value) {
  if (!value.contains("title") || !value.contains("horizontal-axis")) {
    return false;
  }
  if (!value["horizontal-axis"].contains("name") ||
      !value["horizontal-axis"].contains("values")) {
    return false;
  }
  if (value.contains("vertical-axis") &&
      (!value["vertical-axis"].contains("name") ||
       !value["vertical-axis"].contains("values"))) {
    return false;
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
  if (value.is_boolean()) {
    return value.get<bool>();
  }
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (is_valid_table_cbor(value)) {
    return generate_table_value_from_cbor(value);
  }
  if (value.contains("angle")) {
    return generate_output_value_from_cbor(value);
  }
  if (value.contains("source")) {
    return generate_sensor_value_from_cbor(value);
  }
  return uint32_t{0};
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
  };
  if (v.axis.size() == 1) {
    result["data"] = v.one;
    result["horizontal-axis"] = cbor_from_table_axis(v.axis[0]);
  } else {
    result["data"] = v.two;
    result["horizontal-axis"] = cbor_from_table_axis(v.axis[0]);
    result["vertical-axis"] = cbor_from_table_axis(v.axis[1]);
  }
  return result;
}

static json cbor_from_value(const OutputValue &v) {
  return json{
      {"type", v.type},
      {"angle", v.angle},
      {"pin", v.pin},
      {"inverted", v.inverted},
  };
}

static json cbor_from_value(const SensorValue &v) {
  auto j = json{
      {"fault-min", v.fault.min},
      {"fault-max", v.fault.max},
      {"fault-value", v.fault.value},
      {"lag", v.lag},
      {"method", v.method},
      {"pin", v.pin},
      {"source", v.source},
      {"window-capture-opening", v.window.opening},
      {"window-offset", v.window.offset},
      {"window-count", v.window.windows_per_cycle},
  };

  if (v.source == "const") {
    j["fixed-value"] = v.const_value;
  } else {
    if (v.method == "therm") {
      j["therm-a"] = v.therm.a;
      j["therm-b"] = v.therm.b;
      j["therm-c"] = v.therm.c;
      j["therm-bias"] = v.therm.bias;
    } else if (v.method == "linear") {
      j["range-max"] = v.range_max;
      j["range-min"] = v.range_min;
      j["raw-max"] = v.raw_max;
      j["raw-min"] = v.raw_min;
    }
  }
  return j;
}

void Protocol::handle_response_message_from_ems(const json &msg) {
  const auto &response = msg["response"];
  int id = msg["id"];

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
    const auto &types = msg["types"];
    auto t = generate_types_from_cbor(types);
    structurereq.cb(c, t, structurereq.ptr);
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

void Protocol::NewData() {
  while (const auto &maybe_msg = connection->Read()) {
    const auto &msg = maybe_msg.value();
    if (!msg.is_object()) {
      return;
    }

    if (!msg.contains("type")) {
      return;
    }
    std::string type = msg["type"];
    if ((this->trace > 1) ||
        ((this->trace > 0) && (type != "feed" && type != "description"))) {
      std::cerr << "recv: " << msg.dump() << std::endl;
    }

#if 0
    if ((msg.contains("success") == 1) && (msg["success"] != true)) {
      std::cerr << "repeating request" << std::endl;
      m_requests.front()->is_sent = false;
      ensure_sent();
    }
#endif

    if (type == "feed" && msg.contains("values")) {
      handle_feed_message_from_ems(msg["values"]);
    } else if (type == "description" && msg.contains("keys")) {
      handle_description_message_from_ems(msg["keys"]);
    } else if (type == "response" && msg.contains("id") &&
               msg.contains("response")) {
      handle_response_message_from_ems(msg);
    }
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

  if (this->trace > 0) {
    std::cerr << "send: " << first->repr << std::endl;
  }
  this->connection->Write(first->repr);
}

void Model::interrogate(interrogation_change_cb cb, void *ptr) {
  if (!protocol) {
    return;
  }
  interrogate_cb = cb;
  interrogate_cb_ptr = ptr;

  /* First clear any ongoing interrogation commands */
  for (auto r = get_reqs.rbegin(); r != get_reqs.rend(); r++) {
    protocol->Cancel(*r);
  }
  get_reqs.clear();
  config = Configuration{.save_time = std::chrono::system_clock::now(),
                         .name = "autosave"};
  interrogation_state = InterrogationState{.in_progress = true};

  if (structure_req) {
    protocol->Cancel(structure_req);
    structure_req.reset();
  }

  structure_req = protocol->Structure(handle_model_structure, this);
}

InterrogationState Model::interrogation_status() { return interrogation_state; }

void Model::handle_model_get(StructurePath path, ConfigValue val, void *ptr) {
  Model *model = (Model *)ptr;
  model->config.values.insert_or_assign(path, val);
  model->interrogation_state.complete_nodes += 1;
  if (model->interrogation_state.total_nodes ==
      model->interrogation_state.complete_nodes) {
    model->interrogation_state.in_progress = false;
  }
  if (model->interrogate_cb != nullptr) {
    model->interrogate_cb(model->interrogation_status(),
                          model->interrogate_cb_ptr);
  }
}

void Model::handle_model_set(StructurePath path, ConfigValue val, void *ptr) {
  Model *model = (Model *)ptr;
  model->config.values.insert_or_assign(path, val);
  if (model->value_cb) {
    model->value_cb(path, model->value_cb_ptr);
  }
}

static std::vector<StructurePath>
enumerate_structure_paths(StructureNode node) {
  std::vector<StructurePath> paths;
  if (node.is_leaf()) {
    auto leaf = node.leaf();
    paths.push_back(leaf.path);
  } else if (node.is_map()) {
    for (auto child : node.map()) {
      auto subs = enumerate_structure_paths(child.second);
      paths.insert(paths.end(), subs.begin(), subs.end());
    }

  } else if (node.is_list()) {
    for (auto child : node.list()) {
      auto subs = enumerate_structure_paths(child);
      paths.insert(paths.end(), subs.begin(), subs.end());
    }
  }
  return paths;
}

void Model::handle_model_structure(StructureNode root,
                                   std::map<std::string, StructureNode> types,
                                   void *ptr) {
  Model *model = (Model *)ptr;
  model->config.structure = root;
  model->config.types = types;
  auto paths = enumerate_structure_paths(root);
  model->interrogation_state.total_nodes = paths.size();
  for (const auto &path : paths) {
    model->get_reqs.push_back(
        model->protocol->Get(handle_model_get, path, model));
  }
  if (model->interrogate_cb != nullptr) {
    model->interrogate_cb(model->interrogation_status(),
                          model->interrogate_cb_ptr);
  }
}

void Model::set_value(StructurePath path, ConfigValue value) {
  if (!protocol) {
    return;
  }
  protocol->Set(handle_model_set, path, value, this);
}

void Model::set_configuration(const Configuration &conf) {
  config = conf;
  if (interrogate_cb != nullptr) {
    interrogate_cb(interrogation_status(), interrogate_cb_ptr);
  }
}

void Model::set_protocol(std::shared_ptr<Protocol> proto) { protocol = proto; }

static json json_config_from_structure(StructureNode n,
                                       const Configuration &conf) {
  json j{};
  if (n.is_list()) {
    for (const auto &v : n.list()) {
      j.push_back(json_config_from_structure(v, conf));
    }
    return j;
  } else if (n.is_map()) {
    for (const auto &v : n.map()) {
      j[v.first] = json_config_from_structure(v.second, conf);
    }
    return j;
  } else {
    const auto &val = conf.get(n.leaf().path).value_or(uint32_t{0});
    return std::visit([&](const auto &v) { return cbor_from_value(v); }, val);
  }
}

static json json_structure_from_structure(StructureNode n) {
  json j{};
  if (n.is_list()) {
    for (const auto &v : n.list()) {
      j.push_back(json_structure_from_structure(v));
    }
    return j;
  } else if (n.is_map()) {
    for (const auto &v : n.map()) {
      j[v.first] = json_structure_from_structure(v.second);
    }
    return j;
  } else {
    auto leaf = n.leaf();
    return {
        {"_type", leaf.type},
        {"description", leaf.description},
        {"choices", leaf.choices},
    };
  }
}

json Configuration::to_json() const {
  auto config = json_config_from_structure(structure, *this);
  auto s = json_structure_from_structure(structure);
  json jsontypes;
  for (auto &[name, type] : types) {
    jsontypes[name] = json_structure_from_structure(type);
  }

  return {
      {"structure", s},
      {"config", config},
      {"types", jsontypes},
  };
}

void Configuration::from_json(const json &j) {
  auto structure = generate_structure_node_from_cbor(j.at("structure"), {});
  this->structure = structure;
  values.clear();

  types.clear();
  for (auto &[name, jsontype] : j.at("types").items()) {
    auto type = generate_structure_node_from_cbor(jsontype, {});
    types[name] = type;
  }

  auto paths = enumerate_structure_paths(structure);
  for (const auto &path : paths) {
    json val = j["config"];
    for (const auto &p : path) {
      if (std::holds_alternative<std::string>(p)) {
        val = val.at(std::get<std::string>(p));
      } else {
        val = val.at(std::get<int>(p));
      }
    }
    try {
      auto value = generate_node_value_from_cbor(val);
      values.insert_or_assign(path, value);
    } catch (nlohmann::detail::out_of_range &e) {
      std::cerr << "Unable to parse config value: " << val << std::endl;
    }
  }
}
