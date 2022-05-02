#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include <iostream>
#include "Log.h"

#include <zstd.h>

static constexpr std::string_view headerstr = "VIAEMSLOG1";

static constexpr std::string_view footerstr = "VIAEMSLOGFOOTER1";

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

static bool has_file_header(int fd) {
  constexpr size_t headerlen = 8 + 8 + headerstr.size();
  char headerbuf[headerlen];
  int res = pread(fd, headerbuf, headerlen, 0); 
  if (res != headerlen) {
    return false;
  }
  return std::memcmp(headerbuf + 16, headerstr.data(), headerstr.size()) == 0;
}

static void write_file_header(int fd) {
  int res = write(fd, headerstr.data(), headerstr.size());
  if (res != headerstr.size()) {
    throw std::runtime_error{"Unable to write log header"};
  }
}

static uint64_t read_footer(int fd) {
  constexpr size_t footerlen = 8 + 8 + 8 + footerstr.size();
  /* First attempt to read the footer from the end */
  auto current_position = lseek(fd, 0, SEEK_END);
  if (current_position < footerlen) {
    return 0;
  }

  uint8_t buf[footerlen];
  int res = pread(fd, buf, footerlen, current_position - footerlen);
  if (res != footerlen) {
    throw std::runtime_error{"Unable to read final metadata chunk"};
  }
  if (memcmp(&buf[24], footerstr.data(), footerstr.size()) != 0) {
    std::cerr << "Failed to match footer string" << std::endl;
    return 0;
  }
  if (read_value<uint64_t>(&buf[8]) != static_cast<uint64_t>(ChunkType::Footer)) {
    std::cerr << "Failed to match footer type" << std::endl;
    return 0;
  }
  return read_value<uint64_t>(&buf[16]);
}

static DataChunk read_datachunk(int fd, uint64_t position, uint64_t size) {
  std::vector<uint8_t> raw;
  raw.reserve(size);
  int res = pread(fd, raw.data(), size, position);
  if (res != size) {
    return {};
  }

  DataChunk result;
  uint64_t headersize = read_value<uint64_t>(raw.data() + 16);
  result.raw_header = json::from_cbor(raw.begin() + 24, raw.begin() + 24 + headersize);

  if ((result.raw_header.count("compression") > 0) &&
      (result.raw_header["compression"] == "zstd")) {
    const uint8_t *compressed = &raw[24 + headersize];
    uint64_t compressed_size = (uint64_t)(&raw[0] + size) - (uint64_t)compressed;
    auto uc_size = ZSTD_getFrameContentSize(compressed, compressed_size);
    if ((uc_size == ZSTD_CONTENTSIZE_UNKNOWN) || (uc_size == ZSTD_CONTENTSIZE_ERROR)) {
      std::cerr << "unknown size" << std::endl;
      return {}; /* TODO throw? */
    }
    result.raw_data.resize(uc_size);
    size_t const actual_size = ZSTD_decompress(result.raw_data.data(), uc_size, compressed, compressed_size);
    if (actual_size != uc_size) {
      return {}; /*TODO throw? */
      std::cerr << "failed to decomrpess" << std::endl;
    }
  } else {
    result.raw_data.insert(result.raw_data.begin(), raw.begin() + 24 + headersize, raw.begin() + size);
  }

  for (auto row : result.raw_header["columns"]) {
    result.columns.push_back({row[0], row[1]});
  }
  return result;
}

static std::vector<ChunkMetadata> read_indexes(int fd, uint64_t position) {
  std::vector<ChunkMetadata> result;

  auto this_metachunk_offset = position;
  do {
    uint8_t szbuf[8];
    pread(fd, szbuf, 8, this_metachunk_offset);
    auto chunklen = read_value<uint64_t>(szbuf);

    std::vector<uint8_t> buffer;
    buffer.reserve(chunklen);
    pread(fd, buffer.data(), chunklen, this_metachunk_offset);

    auto count = read_value<uint64_t>(&buffer[24]);
    std::vector<ChunkMetadata> this_metachunk_list;
    this_metachunk_list.reserve(count);
    for (int i = 0; i < count; i++) {
      off_t offset = 32 + i * 40;
      ChunkMetadata this_md;
      this_md.start_time = read_value<uint64_t>(&buffer[offset]);
      this_md.stop_time = read_value<uint64_t>(&buffer[offset + 8]);
      this_md.chunk_type = static_cast<ChunkType>(read_value<uint64_t>(&buffer[offset + 16]));
      this_md.offset = read_value<uint64_t>(&buffer[offset + 24]);
      this_md.size = read_value<uint64_t>(&buffer[offset + 32]);
      this_metachunk_list.push_back(this_md);
    }

    this_metachunk_offset = read_value<uint64_t>(&buffer[16]);
    result.insert(result.begin(), this_metachunk_list.begin(), this_metachunk_list.end());
  } while (this_metachunk_offset > 0);

  return result;
}

Log::Log(std::string path) {
  int _fd = open(path.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
  if (_fd < 0) {
    perror("open: ");
    throw std::runtime_error{"open"};
  }
  lseek(_fd, 0, SEEK_END);
  this->fd = _fd;

  if (has_file_header(this->fd)) {
    std::cerr << "has file header" << std::endl;
    /* Scan for a meta index at the end and load the chain */
    this->last_meta_index_position = read_footer(this->fd);
    if (this->last_meta_index_position > 0) {
      this->index = read_indexes(this->fd, this->last_meta_index_position);
      this->num_indexes_at_open = this->index.size();
    }
  } else {
    /* Write a file header */
//    write_file_header(this->fd);
  }
}

std::vector<viaems::LogPoint> extract_points_from_chunk(const DataChunk &chunk, std::vector<std::string>
keys, uint64_t start, uint64_t stop) {
  std::vector<viaems::LogPoint> results;
  std::vector<std::pair<int, ColumnType>> mapping;
  for (auto key : keys) {
    for (int i = 0; i < chunk.columns.size(); i++) {
      if (key == chunk.columns[i].first) {
        mapping.push_back({i, chunk.columns[i].second});
        break;
      }
      /* TODO: handle if the key isn't there? */
    }
  }

  for (uint64_t current_offset = 0; current_offset < chunk.raw_data.size();
  current_offset += (8 + chunk.columns.size() * 4)) {
    viaems::LogPoint lp;
    auto time = read_value<uint64_t>(&chunk.raw_data[current_offset]);
    auto since_epoch = std::chrono::nanoseconds{time};
    lp.time = std::chrono::system_clock::time_point{since_epoch};

    if (time >= start && time <= stop) {
      for (auto col : mapping) {
        uint64_t val_offset = current_offset + 8 + 4 * col.first;
        switch (col.second) {
          case ColumnType::Uint32: {
            auto value = read_value<uint32_t>(&chunk.raw_data[val_offset]);
            lp.values.push_back(value);
            break;
          }
          case ColumnType::Float: {
            auto value = read_value<float>(&chunk.raw_data[val_offset]);
            lp.values.push_back(value);
            break;
          }
        }
      }
      results.push_back(lp);
    }
  }
  return results;
}

viaems::LogChunk Log::GetRange(std::vector<std::string> keys, viaems::FeedTime start, viaems::FeedTime end) {
  auto before = std::chrono::system_clock::now();
  viaems::LogChunk result;
  result.keys = keys;
  /* First determine what chunks we need */
  auto start_ns = time_to_ns(start);
  auto stop_ns = time_to_ns(end);
  int n_chunks = 0;
  for (const auto &i : this->index) {
    if ((i.stop_time < start_ns) || (i.start_time > stop_ns)) {
      continue;
    }
    auto chunk = read_datachunk(this->fd, i.offset, i.size);
    auto points = extract_points_from_chunk(chunk, keys, start_ns, stop_ns);
    n_chunks++;
    result.points.insert(result.points.end(), points.begin(), points.end());
  }
  auto after = std::chrono::system_clock::now();
  std::cerr << "GetRanged scanned " << result.points.size() << " points,  " << n_chunks << " chunks in " << std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() << std::endl;
  return result;
}

static std::vector<uint8_t>
generate_meta_chunk(uint64_t last_meta_offset, 
    std::vector<ChunkMetadata>::const_iterator begin, 
    std::vector<ChunkMetadata>::const_iterator end) {
  std::vector<uint8_t> buffer;

  size_t rows = end - begin;
  size_t indexbytes = 40 * rows;
  size_t chunksize = 32 + indexbytes + 8 + 4;

  buffer_write<uint64_t>(buffer, chunksize);
  buffer_write<uint64_t>(buffer, static_cast<uint64_t>(ChunkType::Meta));
  buffer_write<uint64_t>(buffer, last_meta_offset);
  buffer_write<uint64_t>(buffer, rows);
  for (auto it = begin; it != end; it++) {
    buffer_write<uint64_t>(buffer, it->start_time);
    buffer_write<uint64_t>(buffer, it->stop_time);
    buffer_write<uint64_t>(buffer, static_cast<uint64_t>(it->chunk_type));
    buffer_write<uint64_t>(buffer, it->offset);
    buffer_write<uint64_t>(buffer, it->size);
  }

  assert(buffer.size() == chunksize);
  return buffer;
}

Log::~Log() {
  if (this->fd > 0 && (this->index.size() > num_indexes_at_open)) {
    auto metachunk = generate_meta_chunk(this->last_meta_index_position,
        this->index.begin() + num_indexes_at_open,
        this->index.end());
    write(this->fd, metachunk.data(), metachunk.size());
    fsync(this->fd);
    close(this->fd);
  }
}

void Log::WriteChunk(const viaems::LogChunk &lc) {
  if (lc.points.size() == 0) {
    return;
  }
  std::vector<std::map<std::string, std::string>> columns;
  for (int i = 0; i < lc.keys.size(); i++) {
    std::string type;
    if (std::holds_alternative<uint32_t>(lc.points[0].values[i])) {
      type = "uint32";
    } else if (std::holds_alternative<float>(lc.points[0].values[i])) {
      type = "float";
    }
    columns.push_back({{"name", lc.keys[i]}, {"type", type}});
  }
  auto header_json = json{
    {"chunk_type", "data"},
    {"name", "feed"},
    {"columns", columns}
  };

  auto header_cbor = json::to_cbor(header_json);
  auto data_size = (8 + 4 * columns.size()) * lc.points.size();

  uint64_t chunksize = 8 + 1 + 8 + header_cbor.size() + data_size;
  std::vector<uint8_t> buffer(chunksize, 0);

  write_value(buffer.data(), chunksize);
  buffer[8] = static_cast<uint8_t>(ChunkType::Feed);
  write_value<uint64_t>(buffer.data() + 9, header_cbor.size());
  memcpy(buffer.data() + 17, header_cbor.data(), header_cbor.size());
  
  for (int point = 0; point < lc.points.size(); point++) {
    off_t offset = 17 + header_cbor.size() + point * (4 * columns.size() + 8);

    uint64_t time_ns = time_to_ns(lc.points[point].time);
    write_value(buffer.data() + offset, time_ns);

    for (int col = 0; col < columns.size(); col++) {
      std::visit([&](const auto &x){
          write_value(buffer.data() + offset + 8 + 4 * col, x);
        }, lc.points[point].values[col]
      );
    }
  }
  uint64_t chunkoffset = lseek(this->fd, 0, SEEK_CUR);
  write(this->fd, buffer.data(), chunksize);
  fsync(this->fd);

  /* Update indexes */
  this->index.push_back({
    .start_time = time_to_ns(lc.points.front().time),
    .stop_time = time_to_ns(lc.points.back().time),
    .chunk_type = ChunkType::Feed,
    .offset = chunkoffset,
    .size = chunksize,
  });
}

void Log::SaveConfig(viaems::Configuration c) {
}

std::vector<viaems::Configuration> Log::LoadConfigs() {
  return {};
}

std::chrono::system_clock::time_point Log::EndTime() {
  if (this->index.size() > 0) {
    auto time = this->index.back().stop_time;
    auto since_epoch = std::chrono::nanoseconds{time};
    return std::chrono::system_clock::time_point{since_epoch};
  } else {
    return std::chrono::system_clock::now();
  }
}
