#include <exception>
#include <streambuf>

#include "viaems_protocol.h"

using namespace viaems;

std::vector<FeedUpdate> ViaemsProtocol::FeedUpdates() {
  auto updates = m_feed_updates;
  m_feed_updates.clear();
  return updates;
}

void ViaemsProtocol::handle_description_message_from_ems(cbor::array a) {
  m_feed_vars.clear();
  for (cbor i : a) {
    m_feed_vars.push_back(i);
  }
}

void ViaemsProtocol::handle_feed_message_from_ems(cbor::array a) {
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
  return ConfigNode{
    .description = desc,
    .type = entry.at("_type").to_string(),
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

void ViaemsProtocol::handle_response_message_from_ems(uint32_t id,
                                                      cbor::map response) {
  auto req = m_requests.front();
  m_requests.pop_front();

  auto req_id = std::visit([](const auto &a) { return a.id; }, req);
  if (req_id != id) {
    std::cerr << "Request ID does not match Response ID!" << std::endl;
    return;
  }

  if (std::holds_alternative<PingRequest>(req)) {
    auto pingreq = std::get<PingRequest>(req);
    pingreq.cb(pingreq.ptr);
  } else if (std::holds_alternative<StructureRequest>(req)) {
    auto structurereq = std::get<StructureRequest>(req);
    auto c = generate_structure_node_from_cbor(response, {});
    structurereq.cb(c, structurereq.ptr);
  }
}

void ViaemsProtocol::handle_message_from_ems(cbor msg) {
  if (!msg.is_map()) {
    return;
  }

  std::string type;
  type = msg.to_map().at("type").to_string();

  if (type == "feed") {
    handle_feed_message_from_ems(msg.to_map().at("values").to_array());
  } else if (type == "description") {
    handle_description_message_from_ems(msg.to_map().at("keys").to_array());
  } else if (type == "response") {
    handle_response_message_from_ems(msg.to_map().at("id"),
                                     msg.to_map().at("response").to_map());
  }
}

struct SliceReader : public std::streambuf {
  SliceReader(char *s, std::size_t n) { setg(s, s, s + n); }

  size_t bytes_read() { return gptr() - eback(); }
};

void ViaemsProtocol::NewData(std::string const &data) {
  m_input_buffer.append(data);

  SliceReader reader(m_input_buffer.data(), m_input_buffer.size());
  std::istream data_ss(&reader);
  size_t bytes_to_remove = 0;

  cbor cbordata;
  do {
    if (!cbordata.read(data_ss)) {
      break;
    }
    if (!cbordata.is_map()) {
      break;
    }
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

void ViaemsProtocol::Structure(structure_cb cb, void *v) {
  uint32_t id = 6;
  m_requests.push_back(StructureRequest{ id, cb, v });

  cbor wire_request = cbor::map{
    { "type", "request" },
    { "method", "structure" },
    { "id", id },
  };
  wire_request.write(m_out);
  m_out.flush();
}

void ViaemsProtocol::Ping(ping_cb cb, void *v) {
  uint32_t id = 6;
  m_requests.push_back(PingRequest{ id, cb, v });

  cbor wire_request = cbor::map{
    { "type", "request" },
    { "method", "ping" },
    { "id", id },
  };
  wire_request.write(m_out);
  m_out.flush();
}
