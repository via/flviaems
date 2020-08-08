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

  class ViaemsProtocol {
    public:
      std::vector<FeedUpdate> FeedUpdates();
      void NewData(std::string const & data);

    private:
      std::vector<std::string> m_feed_vars;
      std::vector<FeedUpdate> m_feed_updates;
      std::string m_input_buffer;

      void handle_message_from_ems(cbor m);
      void handle_feed_message_from_ems(cbor::array m);
      void handle_description_message_from_ems(cbor::array m);

  };
}

#endif
