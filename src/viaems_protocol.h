#ifndef VIAEMS_PROTOCOL_H
#define VIAEMS_PROTOCOL_H

#include <memory>
#include <vector>
#include <deque>
#include <map>
#include <variant>
#include <cstdint>
#include <sstream>

#include <cbor11.h>

namespace viaems {

  typedef std::variant<uint32_t, float> FeedValue;
  typedef std::map<std::string, FeedValue> FeedUpdate;

  typedef std::vector<std::variant<int, std::string>> StructurePath;

  struct TableNode {
  };

  typedef std::variant<uint32_t, float, std::string, TableNode> ConfigValue;

  struct ConfigNode {
    std::string description;
    std::string type;
    StructurePath path;
  };


  struct StructureNode;
  typedef std::vector<StructureNode> StructureList;
  typedef std::map<std::string, StructureNode> StructureMap;


  struct StructureNode {
    std::variant<
      std::shared_ptr<StructureList>,
      std::shared_ptr<StructureMap>, 
      std::shared_ptr<ConfigNode>> contents;
  };

  typedef void (*structure_cb)(StructureNode top, void *ptr);
  struct StructureRequest {
    uint32_t id;
    structure_cb cb;
    void *ptr;
  };

  typedef void (*ping_cb)(void *ptr);
  struct PingRequest {
    uint32_t id;
    ping_cb cb;
    void *ptr;
  };

  typedef std::variant<StructureRequest, PingRequest> Request;

  class ViaemsProtocol {
    public:
      ViaemsProtocol(std::ostream& out) : m_out(out) {}

      std::vector<FeedUpdate> FeedUpdates();
      void NewData(std::string const & data);

      void Structure(structure_cb, void *);
      void Ping(ping_cb, void *);

    private:
      std::vector<std::string> m_feed_vars;
      std::vector<FeedUpdate> m_feed_updates;
      std::string m_input_buffer;
      std::ostream& m_out;
      std::deque<Request> m_requests;

      void handle_message_from_ems(cbor m);
      void handle_feed_message_from_ems(cbor::array m);
      void handle_description_message_from_ems(cbor::array m);
      void handle_response_message_from_ems(uint32_t id, cbor::map response);

  };
}

#endif
