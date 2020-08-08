#ifndef VIAEMS_PROTOCOL_H
#define VIAEMS_PROTOCOL_H

#include <memory>
#include <vector>
#include <map>
#include <variant>
#include <cstdint>
#include <sstream>

#include <cbor11.h>

namespace viaems {

  typedef std::variant<uint32_t, float> FeedValue;
  typedef std::map<std::string, FeedValue> FeedUpdate;

  class ConfigNode;
  typedef std::vector<ConfigNode> ConfigNodeList;
  typedef std::map<std::string, ConfigNode> ConfigNodeMap;

  class ConfigNode {
    std::variant<uint32_t, float, std::unique_ptr<ConfigNodeList>, std::unique_ptr<ConfigNodeMap>> contents;
  };

  typedef void (*structure_cb)(ConfigNode node, void *ptr);

  struct StructureRequest {
    uint32_t id;
    structure_cb cb;
    void *v;
  };

  typedef std::variant<StructureRequest> Request;

  class ViaemsProtocol {
    public:
      ViaemsProtocol(std::shared_ptr<std::ostream>);

      std::vector<FeedUpdate> FeedUpdates();
      void NewData(std::string const & data);

      void Structure(structure_cb, void *);

    private:
      std::vector<std::string> m_feed_vars;
      std::vector<FeedUpdate> m_feed_updates;
      std::string m_input_buffer;
      std::vector<Request> m_requests;
      std::shared_ptr<std::ostream> m_out;

      void handle_message_from_ems(cbor m);
      void handle_feed_message_from_ems(cbor::array m);
      void handle_description_message_from_ems(cbor::array m);
      void handle_response_message_from_ems(uint32_t id, cbor::map response);

  };
}

#endif
