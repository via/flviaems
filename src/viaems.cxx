#include <chrono>
#include <cstdlib>
#include <exception>
#include <streambuf>

#include "Log.h"
#include "viaems.h"

using namespace viaems;

std::unique_ptr<LogChunk> Protocol::FeedUpdates() {
  auto updates = std::move(m_feed_updates);
  m_feed_updates = std::make_unique<LogChunk>();
  m_feed_updates->keys = m_feed_vars;
  return updates;
}

void Protocol::handle_description_message_from_ems(cbor::array a) {
  m_feed_vars.clear();
  for (cbor i : a) {
    m_feed_vars.push_back(i);
  }
  m_feed_updates->keys = m_feed_vars;
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

void Protocol::handle_feed_message_from_ems(cbor::array a) {
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
  auto cputime = a[i].to_unsigned();
  if (cputime < last_feed_time) {
    zero_time = calculate_zero_point(cputime, std::chrono::system_clock::now());
  }
  update.time = calculate_real_time(cputime, zero_time);
  last_feed_time = cputime;
  for (size_t i = 0; i < a.size(); i++) {
    cbor &val = a[i];
    if (val.is_int()) {
      update.values.push_back(
          FeedValue(static_cast<uint32_t>(val.to_unsigned())));
    } else if (val.is_float()) {
      update.values.push_back(FeedValue(static_cast<float>(val.to_float())));
    }
  }
  m_feed_updates->points.push_back(update);
}

static StructureLeaf generate_config_node(cbor::map entry, StructurePath path) {
  std::string type = entry.at("_type").to_string();
  std::string desc = "";
  if (entry.count("description")) {
    desc = entry.at("description").to_string();
  }

  std::vector<std::string> choices;
  if (entry.count("choices")) {
    for (auto choice : entry.at("choices").to_array()) {
      choices.push_back(choice.to_string());
    }
  }

  return StructureLeaf{
      .description = desc,
      .type = entry.at("_type").to_string(),
      .choices = choices,
      .path = path,
  };
}

static StructureNode generate_structure_node_from_cbor(cbor entry,
                                                       StructurePath curpath) {
  if (entry.is_map()) {
    auto map = entry.to_map();
    if (map.count("_type")) {
      /* This is a leaf node */
      return StructureNode{generate_config_node(map, curpath)};
    }

    /* This is actually a map, we should descend */
    auto result = StructureMap{};
    for (const auto &map_entry : entry.to_map()) {
      StructurePath new_path = curpath;
      new_path.push_back(map_entry.first.to_string());

      auto child =
          generate_structure_node_from_cbor(map_entry.second, new_path);
      result.insert(std::pair<std::string, StructureNode>(
          map_entry.first.to_string(), child));
    }
    return StructureNode{result};
  } else if (entry.is_array()) {
    auto result = StructureList{};
    int index = 0;
    for (const auto &list_entry : entry.to_array()) {
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

static TableAxis generate_table_axis_from_cbor(cbor::map axis) {
  TableAxis res{};
  res.name = axis.at("name").to_string();
  for (const auto label : axis.at("values").to_array()) {
    res.labels.push_back(label.to_float());
  }
  return res;
}

static ConfigValue generate_table_value_from_cbor(cbor::map map) {
  TableValue table;
  int n_axis = map.at("num-axis").to_unsigned();
  table.title = map.at("title").to_string();
  table.axis.push_back(
      generate_table_axis_from_cbor(map.at("horizontal-axis").to_map()));

  if (n_axis == 1) {
    for (const auto datum : map.at("data").to_array()) {
      table.one.push_back(datum.to_float());
    }
  } else if (n_axis == 2) {
    table.axis.push_back(
        generate_table_axis_from_cbor(map.at("vertical-axis").to_map()));
    for (const auto outter : map.at("data").to_array()) {
      std::vector<float> values;
      for (const auto value : outter.to_array()) {
        values.push_back(value.to_float());
      }
      table.two.push_back(values);
    }
  }
  return table;
}

static ConfigValue generate_node_value_from_cbor(cbor value) {

  if (value.is_int()) {
    return (uint32_t)value.to_unsigned();
  }
  if (value.is_float()) {
    return (float)value.to_float();
  }
  if (value.is_string()) {
    return value.to_string();
  }
  if (value.is_map()) {
    auto m = value.to_map();
    if (m.count("num-axis") > 0) {
      return generate_table_value_from_cbor(m);
    }
  }
  return ConfigValue{5.0f};
}

template <typename T> static cbor cbor_from_value(T v) { return cbor{v}; }

static cbor cbor_from_table_axis(TableAxis axis) {
  cbor::array labels;
  for (const auto l : axis.labels) {
    labels.push_back(l);
  }
  auto result = cbor::map{
      {"name", axis.name},
      {"values", labels},
  };
  return result;
}

static cbor cbor_from_value(const TableValue &v) {
  auto result = cbor::map{
      {"num-axis", v.axis.size()},
      {"title", v.title},
      {"horizontal-axis", cbor_from_table_axis(v.axis[0])},
  };
  if (v.axis.size() == 1) {
    auto data = cbor::array{};
    for (const auto d : v.one) {
      data.push_back(d);
    }
    result.insert(std::pair("data", data));
  } else {
    auto data = cbor::array{};
    for (const auto row : v.two) {
      auto cbor_row = cbor::array{};
      for (const auto val : row) {
        cbor_row.push_back(val);
      }
      data.push_back(cbor_row);
    }
    result.insert(std::pair("data", data));
    result.insert(std::pair("vertical-axis", cbor_from_table_axis(v.axis[1])));
  }
  return result;
}

static cbor cbor_from_value(const OutputValue &v) { return cbor{}; }

static cbor cbor_from_value(const SensorValue &v) { return cbor{}; }

void Protocol::handle_response_message_from_ems(uint32_t id, cbor response) {
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
    auto c = generate_structure_node_from_cbor(response.to_map(), {});
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

void Protocol::handle_message_from_ems(cbor msg) {
  if (!msg.is_map()) {
    return;
  }

  std::string type;
  cbor::map val = msg.to_map();
  if (val.count("type") != 1) {
    return;
  }
  if ((val.count("success") == 1) && !val.at("success").to_bool()) {
    return;
  }
  type = val.at("type").to_string();

  if (type == "feed" && (val.count("values") > 0)) {
    handle_feed_message_from_ems(val.at("values").to_array());
  } else if (type == "description" && (val.count("keys") > 0)) {
    handle_description_message_from_ems(val.at("keys").to_array());
  } else if (type == "response" && (val.count("id") > 0) &&
             (val.count("response") > 0)) {
    handle_response_message_from_ems(val.at("id"), val.at("response"));
  }
}

struct SliceReader : public std::streambuf {
  SliceReader(char *s, std::size_t n) { setg(s, s, s + n); }

  size_t bytes_read() { return gptr() - eback(); }
};

void Protocol::NewData(std::string const &data) {
  m_input_buffer.append(data);

  SliceReader reader{m_input_buffer.data(), m_input_buffer.size()};
  std::istream data_ss{&reader};
  size_t bytes_to_remove = 0;

  cbor cbordata;
  do {
    try {
      if (!cbordata.read(data_ss)) {
        break;
      }
    } catch (const std::bad_alloc &) {
      std::cerr << "Bad alloc!" << std::endl;
      break;
    } catch (const std::length_error &) {
      std::cerr << "length error" << std::endl;
      break;
    }

    cbor::array array;
    for (auto req : m_requests) {
      array.push_back(req->id);
    }
    cbor item = array;
    handle_message_from_ems(cbordata);
    /* Only remove bytes we've successfully decoded */
    bytes_to_remove = reader.bytes_read();
  } while (true);

  m_input_buffer.erase(0, bytes_to_remove);

  /* Is our buffer massive? If so, reset it */
  if (m_input_buffer.length() > 16384) {
    m_input_buffer.clear();
  }
}

std::shared_ptr<Request> Protocol::Structure(structure_cb cb, void *v) {
  uint32_t id = rand() % 1024;

  cbor wire_request = cbor::map{
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

  cbor wire_request = cbor::map{
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

cbor::array cbor_path_from_structure_path(viaems::StructurePath path) {
  cbor::array result;
  for (const auto &p : path) {
    std::visit([&](const auto &v) { result.push_back(v); }, p);
  }
  return result;
}

std::shared_ptr<Request> Protocol::Get(get_cb cb, viaems::StructurePath path,
                                       void *v) {
  uint32_t id = rand() % 1024;

  cbor wire_request = cbor::map{
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

  cbor cval = std::visit(
      [](const auto &v) -> cbor { return cbor_from_value(v); }, value);
  cbor wire_request = cbor::map{
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
  cbor wire_request = cbor::map{
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
  cbor wire_request = cbor::map{
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
  first->repr.write(m_out);
  m_out.flush();
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
