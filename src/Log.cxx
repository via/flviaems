#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <memory>

#include <iostream>
#include "Log.h"

#include <zstd.h>

static constexpr std::string_view headerstr = "VIAEMSLOG1";

static constexpr std::string_view footerstr = "VIAEMSLOGFOOTER1";

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
    result.raw_data.resize(size - headersize - 24);
    memcpy(result.raw_data.data(), raw.data() + 24 + headersize, size - headersize - 24);
//    result.raw_data.insert(result.raw_data.begin(), raw.begin() + 24 + headersize, raw.begin() + size);
  }

  size_t offset = 8; /* Start after time */
  for (auto row : result.raw_header["columns"]) {
    result.columns.push_back({row[0], row[1], offset});
    offset += 4; /* TODO: Handle possible different type sizes */
  }
  result.rowsize = offset;
  result.count = result.raw_data.size() / offset;
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


Log::View::View(Log &log, const std::vector<std::string> keys, uint64_t start, uint64_t stop)
  : log{log}, keys{keys}, start{start}, stop{stop} {

  int n_chunks = 0;
  for (const auto &i : this->log.index) {
    if ((i.stop_time < start) || (i.start_time > stop)) {
      continue;
    }
    this->index.push_back(i);
  }
}

void Log::View::next_chunk() {

  if (this->next_chunk_idx >= this->index.size()) {
    this->current.reset();
    return;
  }
  const auto &meta = this->index[this->next_chunk_idx];
  if (this->log.cache.count(meta.offset) > 0) {
    this->current = this->log.cache[meta.offset];
  } else {
    this->current = std::make_shared<DataChunk>(read_datachunk(this->log.fd, meta.offset, meta.size));
    this->log.cache[meta.offset] = this->current;
  }
  std::cerr << "cached " << log.cache.size() << " results" << std::endl;
  
  this->next_chunk_idx += 1;
  this->next_chunk_row = 0;

  this->mapping.resize(0);
  for (const auto &k : this->keys) {
    int index = 0;
    for (const auto &c : this->current->columns) {
      if (k == c.name) {
        this->mapping.push_back(index);
      }
      index += 1;
    }
  }
}

const std::optional<LogRow> Log::View::next_row() {
  this->next_chunk_row += 1;
  if (!this->current || this->next_chunk_row >= this->current->count) {
    next_chunk();
    if (!this->current) {
      return {};
    }
  }
  auto row = LogRow{*this->current, this->next_chunk_row};
  if (row.time() > this->stop) {
    return {};
  } else {
    return row;
  }
}

Log::View Log::GetRange(std::vector<std::string> keys, uint64_t start_ns, uint64_t stop_ns) {
  View result{*this, keys, start_ns, stop_ns};
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
