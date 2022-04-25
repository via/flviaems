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

enum class ChunkType : uint8_t {
  Ignore = 0x00,
  Data = 0x01,
  Aggr = 0x02,
  Meta = 0x03,
};

enum class ColumnType {
  Uint32,
  Float,
};

struct DataChunk {
  std::string name;
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

