#include <exception>

#include "viaems_protocol.h"

using namespace viaems;

ViaemsProtocol::ViaemsProtocol(std::shared_ptr<std::ostream> o) {
  m_out = o;
}

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
    cbor val = a[i];
    if (val.is_int()) {
      update[m_feed_vars[i]] = FeedValue(static_cast<uint32_t>(val.to_unsigned()));
    } else if (val.is_float()) {
      update[m_feed_vars[i]] = FeedValue(static_cast<float>(val.to_float()));
    }
  }
  m_feed_updates.push_back(update);
}
void ViaemsProtocol::handle_response_message_from_ems(uint32_t id, cbor::map response) {
  auto sreq = std::get<StructureRequest>(m_requests.front());
  auto req = m_requests.erase(m_requests.begin());
  if (id != sreq.id) {
    std::cerr << "id did not match!" << std::endl;
    return;
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
    handle_response_message_from_ems(msg.to_map().at("id"), msg.to_map().at("response"));
  }

}

void ViaemsProtocol::NewData(std::string const & data) {
  m_input_buffer.append(data);

  std::istringstream data_ss(std::move(m_input_buffer));
  m_input_buffer = std::string{};

  cbor cbordata;
  cbordata.read(data_ss);
  if (!cbordata.is_undefined()) {
    /* do stuff */
    handle_message_from_ems(cbordata);
    size_t bytes_read = data_ss.tellg();
    m_input_buffer.erase(0, bytes_read);
  } else {
    /* Is our buffer massive? If so, reset it */
    if (m_input_buffer.length() > 65535) {
      m_input_buffer.clear();
    }
  }
}

void ViaemsProtocol::Structure(structure_cb cb, void *v) {
  uint32_t id = 6;
  m_requests.push_back(StructureRequest{6, cb, v});

  cbor wire_request = cbor::map {
    {"type", "request"},
    {"method", "structure"},
    {"id", id},
  };
  wire_request.write(*m_out);
  m_out->flush();
}

