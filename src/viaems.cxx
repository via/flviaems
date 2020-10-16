#include <exception>
#include <streambuf>
#include <cstdlib>

#include "viaems.h"

using namespace viaems;

std::vector<FeedUpdate> Protocol::FeedUpdates() {
  auto updates = m_feed_updates;
  m_feed_updates.clear();
  return updates;
}

void Protocol::handle_description_message_from_ems(cbor::array a) {
  m_feed_vars.clear();
  for (cbor i : a) {
    m_feed_vars.push_back(i);
  }
}

void Protocol::handle_feed_message_from_ems(cbor::array a) {
  if (a.size() != m_feed_vars.size()) {
    return;
  }
  FeedUpdate update;
  for (int i = 0; i < a.size(); i++) {
    cbor &val = a[i];
    if (val.is_int()) {
      update[m_feed_vars[i]] =
        FeedValue(static_cast<uint32_t>(val.to_unsigned()));
    } else if (val.is_float()) {
      update[m_feed_vars[i]] = FeedValue(static_cast<float>(val.to_float()));
    }
  }
  m_feed_updates.push_back(update);
}

static ConfigNode generate_config_node(cbor::map entry, StructurePath path) {
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

  return ConfigNode{
    .description = desc,
    .type = entry.at("_type").to_string(),
    .choices = choices,
    .path = path,
  };
}

static StructureNode generate_structure_node_from_cbor(cbor entry,
                                                       StructurePath curpath) {
  if (entry.is_map()) {
    if (entry.to_map().count("_type")) {
      /* This is a leaf node */
      return StructureNode{ std::make_shared<ConfigNode>(
        generate_config_node(entry, curpath)) };
    }

    /* This is actually a map, we should descend */
    auto contents = std::make_shared<StructureMap>();
    for (auto map_entry : entry.to_map()) {
      StructurePath new_path = curpath;
      new_path.push_back(map_entry.first.to_string());

      auto child =
        generate_structure_node_from_cbor(map_entry.second, new_path);
      contents->insert(std::pair<std::string, StructureNode>(
        map_entry.first.to_string(), child));
    }
    return StructureNode{ contents };
  } else if (entry.is_array()) {
    auto contents = std::make_shared<StructureList>();
    int index = 0;
    for (auto list_entry : entry.to_array()) {
      StructurePath new_path = curpath;
      new_path.push_back(index);
      index += 1;

      auto child = generate_structure_node_from_cbor(list_entry, new_path);
      contents->push_back(child);
    }
    return StructureNode{ contents };
  }

  return StructureNode{ std::make_shared<ConfigNode>() };
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
  return ConfigValue{5.0f};
}

void Protocol::handle_response_message_from_ems(uint32_t id,
                                                      cbor response) {
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

  if (type == "feed") {
    handle_feed_message_from_ems(msg.to_map().at("values").to_array());
  } else if (type == "description") {
    handle_description_message_from_ems(msg.to_map().at("keys").to_array());
  } else if (type == "response") {
    handle_response_message_from_ems(msg.to_map().at("id"),
                                     msg.to_map().at("response"));
  }
}

struct SliceReader : public std::streambuf {
  SliceReader(char *s, std::size_t n) { setg(s, s, s + n); }

  size_t bytes_read() { return gptr() - eback(); }
};

void Protocol::NewData(std::string const &data) {
  m_input_buffer.append(data);

  SliceReader reader(m_input_buffer.data(), m_input_buffer.size());
  std::istream data_ss(&reader);
  size_t bytes_to_remove = 0;

  cbor cbordata;
  do {
    try {
      if (!cbordata.read(data_ss)) {
        break;
      }
    } catch(std::bad_alloc) {
      std::cerr << "Bad alloc!" << std::endl;
      break;
    } catch(std::length_error) {
      std::cerr << "length error" << std::endl;
      break;
    }

    cbordata.write(m_log);
    m_log.flush();
    cbor::array array;
    for (auto req : m_requests) {
      array.push_back(req->id);
    }
    cbor item = array;
    item.write(m_log);
    handle_message_from_ems(cbordata);
    /* Only remove bytes we've successfully decoded */
    bytes_to_remove = reader.bytes_read();
  } while(true);

  m_input_buffer.erase(0, bytes_to_remove);

  /* Is our buffer massive? If so, reset it */
  if (m_input_buffer.length() > 16384) {
    m_input_buffer.clear();
  }
}

std::shared_ptr<Request> Protocol::Structure(structure_cb cb, void *v) {
  uint32_t id = rand() % 1024;

  cbor wire_request = cbor::map{
    { "type", "request" },
    { "method", "structure" },
    { "id", id },
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
    { "type", "request" },
    { "method", "ping" },
    { "id", id },
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

std::shared_ptr<Request> Protocol::Get(get_cb cb, viaems::StructurePath path, void *v) {
  uint32_t id = rand() % 1024;

  cbor::array cbor_path;
  for (auto p : path) {
    if (std::holds_alternative<int>(p)) {
      cbor_path.push_back(std::get<int>(p));
    } else if (std::holds_alternative<std::string>(p)) {
      cbor_path.push_back(std::get<std::string>(p));
    }
  }

  cbor wire_request = cbor::map{
    { "type", "request" },
    { "method", "get" },
    { "id", id },
    { "path", cbor_path },
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
  first->repr.write(m_log);
}

void Model::interrogate(interrogate_cb cb, void *ptr) {
  interrogation_callback = cb;
  interrogation_callback_ptr = ptr;
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
    if (x.second->is_valid()) {
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
  model->m_model.at(path)->value = std::make_shared<ConfigValue>(val);
  model->interrogation_callback(model->interrogation_status(), model->interrogation_callback_ptr);
}

void Model::handle_model_structure(StructureNode root, void *ptr) {
  Model *model = (Model *)ptr;
  model->root = root;
  model->recurse_model_structure(root);
  model->interrogation_callback(model->interrogation_status(), model->interrogation_callback_ptr);
}

void Model::recurse_model_structure(StructureNode node) {
  if (node.is_leaf()) {
    auto leaf = node.leaf();
    auto nodemodel = std::make_unique<NodeModel>();
    nodemodel->node = *leaf;
    m_model.insert(std::make_pair(leaf->path, std::move(nodemodel)));
    get_reqs.push_back(m_protocol.Get(handle_model_get, leaf->path, this));
  } else if (node.is_map()) {
    for (auto child : *node.map()) {
      recurse_model_structure(child.second);
    }
  } else if (node.is_list()) {
    for (auto child : *node.list()) {
      recurse_model_structure(child);
    }
  }
}
