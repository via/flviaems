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
    cbor& val = a[i];
    if (val.is_int()) {
      update[m_feed_vars[i]] = FeedValue(static_cast<uint32_t>(val.to_unsigned()));
    } else if (val.is_float()) {
      update[m_feed_vars[i]] = FeedValue(static_cast<float>(val.to_float()));
    }
  }
  m_feed_updates.push_back(update);
}

static ConfigNode generate_config_map_from_response(cbor::map structure) {

  auto node = ConfigNode{};

  auto contents = std::make_shared<ConfigNodeMap>();
  for (auto entry : structure) {
    if (entry.second.is_map()) {
      auto child = generate_config_map_from_response(entry.second.to_map());
      contents->insert(std::pair<std::string, ConfigNode>(entry.first.to_string(), std::move(child)));
    }
  }

  node.contents = contents;
  return node;
}

void ViaemsProtocol::handle_response_message_from_ems(uint32_t id, cbor::map response) {
  auto req = m_requests.front();
  m_requests.pop_front();

  auto req_id = std::visit([](const auto& a){return a.id;}, req);
  if (req_id != id) {
    std::cerr << "Request ID does not match Response ID!" << std::endl;
    return;
  }

  if (std::holds_alternative<PingRequest>(req)) {
    auto pingreq = std::get<PingRequest>(req);
    pingreq.cb(pingreq.ptr);
  } else if (std::holds_alternative<StructureRequest>(req)) {
    auto structurereq = std::get<StructureRequest>(req);
    auto c = generate_config_map_from_response(response);
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
    handle_response_message_from_ems(msg.to_map().at("id"), msg.to_map().at("response").to_map());
  }

}

struct OneShotReadBuf : public std::streambuf
{
    OneShotReadBuf(char* s, std::size_t n)
    {
        setg(s, s, s + n);
    }

    size_t amt() {
      return gptr() - eback();
    }
};


void ViaemsProtocol::NewData(std::string const & data) {
  m_input_buffer.append(data);

  OneShotReadBuf osrb(m_input_buffer.data(), m_input_buffer.size());
//  std::istringstream data_ss(m_input_buffer);
  std::istream data_ss(&osrb);

  cbor cbordata;
  cbordata.read(data_ss);
  if (!cbordata.is_undefined()) {
    /* do stuff */
    handle_message_from_ems(cbordata);

//    m_input_buffer.erase(0, osrb.amt());
    m_input_buffer = m_input_buffer.substr(osrb.amt());
  } else {
    /* Is our buffer massive? If so, reset it */
    if (m_input_buffer.length() > 65535) {
      m_input_buffer.clear();
    }
  }
}

void ViaemsProtocol::Structure(structure_cb cb, void *v) {
  uint32_t id = 6;
  m_requests.push_back(StructureRequest{id, cb, v});

  cbor wire_request = cbor::map {
    {"type", "request"},
    {"method", "structure"},
    {"id", id},
  };
  wire_request.write(m_out);
  m_out.flush();
}

void ViaemsProtocol::Ping(ping_cb cb, void *v) {
  uint32_t id = 6;
  m_requests.push_back(PingRequest{id, cb, v});

  cbor wire_request = cbor::map {
    {"type", "request"},
    {"method", "ping"},
    {"id", id},
  };
  wire_request.write(m_out);
  m_out.flush();
}
