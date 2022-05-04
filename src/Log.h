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

template<typename T>
static T read_value(const uint8_t *data) {
  T result;
  memcpy(&result, data, sizeof(T));
  return result;
}

template<typename T>
static void write_value(uint8_t *data, T value) {
  memcpy(data, &value, sizeof(T));
}

template<typename T>
static void buffer_write(std::vector<uint8_t> &buffer, T value) {
  uint8_t bytes[sizeof(T)];
  memcpy(bytes, &value, sizeof(T));
  buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

template<>
void buffer_write<std::string>(std::vector<uint8_t> &buffer, std::string value) {
  buffer.insert(buffer.end(), value.data(), value.data() + value.size());
}

uint64_t time_to_ns(std::chrono::system_clock::time_point tp) {
  uint64_t time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      tp.time_since_epoch()).count();
  return time_ns;
}

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

using LogValue = std::optional<std::variant<uint32_t, float>>;

struct Column {
  std::string name;
  ColumnType type;
  size_t offset;
};

struct DataChunk {
  std::vector<Column> columns;
  json raw_header;
  size_t count;
  size_t rowsize;
  std::vector<uint8_t> raw_data;

  LogValue get(size_t row, size_t index) const {
    const auto &col = columns[index];
    const uint8_t *val = raw_data.data() + (rowsize * row) + col.offset;
    if (col.type == ColumnType::Float) {
      return read_value<float>(val);
    } else if (col.type == ColumnType::Uint32) {
      return read_value<uint32_t>(val);
    } else {
      return {};
    }
  }

  uint64_t time(size_t row) const {
    const uint8_t *start = &raw_data[rowsize * row];
    return read_value<uint64_t>(start);
  }
};

struct ChunkMetadata {
  uint64_t start_time;
  uint64_t stop_time;
  ChunkType chunk_type;
  uint64_t offset;
  uint64_t size;
};

struct LogRow {
  const DataChunk &chunk;
  int row;

  uint64_t time() {
    return chunk.time(row);
  }
  LogValue get(size_t index) {
    return chunk.get(row, index);
  }
};

class Log {

  struct View {
    View(Log &log, const std::vector<std::string> keys, uint64_t start, uint64_t stop);

    Log& log;
    const std::vector<std::string> keys;
    const uint64_t start;
    const uint64_t stop;

    std::vector<ChunkMetadata> index;
    std::shared_ptr<DataChunk> current;
    int next_chunk_idx;

    const DataChunk & next_chunk();
  };

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

  View GetRange(std::vector<std::string> keys,
                            viaems::FeedTime start, viaems::FeedTime end);

  void SaveConfig(viaems::Configuration);
  std::vector<viaems::Configuration> LoadConfigs();

  std::chrono::system_clock::time_point EndTime();
};

