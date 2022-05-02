#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include <condition_variable>
#include <mutex>

#include <sqlite3.h>

#include "viaems.h"

enum class ChunkType : uint64_t {
  Ignore = 0x00,
  Header = 0x01,
  Footer = 0x02,
  Meta = 0x03,
  Feed = 0x03,
};

enum class ColumnType {
  Unknown = 0x0, 
  Uint32 = 0x1,
  Float = 0x2,
};

struct DataChunk {
  std::vector<std::pair<std::string, ColumnType>> columns;
  json raw_header;
  std::vector<uint8_t> raw_data;
};

struct ChunkMetadata {
  uint64_t start_time;
  uint64_t stop_time;
  ChunkType chunk_type;
  uint64_t offset;
  uint64_t size;
};

class Log {
private:
  int fd;
  uint64_t last_meta_index_position;
  uint64_t num_indexes_at_open;
  std::vector<ChunkMetadata> index;

  struct std::shared_ptr<DataChunk> active_chunk;
  struct ChunkMetadata active_chunk_meta;
  std::mutex active_chunk_mutex;

public:
  Log(std::string path);
  Log(const Log &) = delete;
  Log &operator=(const Log &) = delete;
  ~Log();

  void WriteChunk(const viaems::LogChunk &);

  viaems::LogChunk GetRange(std::vector<std::string> keys,
                            viaems::FeedTime start, viaems::FeedTime end);

  void SaveConfig(viaems::Configuration);
  std::vector<viaems::Configuration> LoadConfigs();

  std::chrono::system_clock::time_point EndTime();
};

