#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include "Log.h"

static constexpr std::string_view headerstr = "VIAEMSLOG1";
static constexpr auto headerlen = headerstr.size();

template<typename T>
static uint64_t read_value(uint8_t *data) {
  T result;
  memcpy(&result, data, sizeof(T));
  return result;
}

template<typename T>
static void write_value(uint8_t *data, T value) {
  memcpy(data, &value, sizeof(T));
}

static bool has_file_header(int fd) {
  char headerbuf[headerlen];
  int res = pread(fd, headerbuf, headerlen, 0); 
  if (res != headerlen) {
    return false;
  }
  return std::memcmp(headerbuf, headerstr.data(), headerlen) == 0;
}

static void write_file_header(int fd) {
  int res = write(fd, headerstr.data(), headerlen);
  if (res != headerlen) {
    throw std::runtime_error{"Unable to write log header"};
  }
}

static std::vector<ChunkMetadata> read_indexes(int fd) {
  /* First attempt to read the last 12 bytes of the file, which would be an
   * 8-byte size + "meta" */
  auto current_position = lseek(fd, 0, SEEK_CUR);
  if (current_position < 12) {
    return {};
  }

  constexpr size_t readamt = 12;
  uint8_t buf[readamt];
  int res = pread(fd, buf, readamt, current_position - readamt);
  if (res != readamt) {
    throw std::runtime_error{"Unable to read final metadata chunk"};
  }
  if (memcmp(&buf[8], "meta", 4) != 0) {
    return {};
  }

  auto metachunk_size = read_value<uint64_t>(buf);
  auto this_metachunk_offset = current_position - metachunk_size; 
  std::vector<ChunkMetadata> result;

  do {
    uint8_t szbuf[8];
    pread(fd, szbuf, 8, this_metachunk_offset);
    auto chunklen = read_value<uint64_t>(szbuf);

    std::vector<uint8_t> buffer;
    buffer.reserve(chunklen);
    pread(fd, buffer.data(), chunklen, this_metachunk_offset + 8);

    auto count = read_value<uint64_t>(&buffer[17]);
    std::vector<ChunkMetadata> this_metachunk_list;
    this_metachunk_list.reserve(count);
    for (int i = 0; i < count; i++) {
      off_t offset = 25 + i * 33;
      ChunkMetadata this_md;
      this_md.start_time = read_value<uint64_t>(&buffer[offset]);
      this_md.stop_time = read_value<uint64_t>(&buffer[offset + 8]);
      this_md.chunk_type = static_cast<ChunkType>(buffer[offset + 16]);
      this_md.offset = read_value<uint64_t>(&buffer[offset + 17]);
      this_md.size = read_value<uint64_t>(&buffer[offset + 25]);
      this_metachunk_list.push_back(this_md);
    }

    this_metachunk_offset = read_value<uint64_t>(&buffer[9]);
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
  this->fd = _fd;

  if (has_file_header(this->fd)) {
    /* Scan for a meta index at the end and load the chain */
    this->index = read_indexes(this->fd);
    this->num_indexes_at_open = this->index.size();
  } else {
    /* Write a file header */
    write_file_header(this->fd);
  }
}

Log::~Log() {
  close(this->fd);
}

viaems::LogChunk Log::GetRange(std::vector<std::string> keys, viaems::FeedTime start,
viaems::FeedTime end) {
  return {};
}

void Log::WriteChunk(const viaems::LogChunk &lc) {
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
  buffer[8] = static_cast<uint8_t>(ChunkType::Data);
  write_value<uint64_t>(buffer.data() + 9, header_cbor.size());
  memcpy(buffer.data() + 17, header_cbor.data(), header_cbor.size());
  
  for (int point = 0; point < lc.points.size(); point++) {
    off_t offset = 17 + header_cbor.size() + point * (4 * columns.size() + 8);
    uint64_t time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           lc.points[point].time.time_since_epoch())
                           .count();

    write_value(buffer.data() + offset, time_ns);
    for (int col = 0; col < columns.size(); col++) {
      std::visit([&](const auto &x){
          write_value(buffer.data() + offset + 8 + 4 * col, x);
        }, lc.points[point].values[col]
      );
    }
  }
  write(this->fd, buffer.data(), chunksize);
  fsync(this->fd);
}

void Log::SaveConfig(viaems::Configuration c) {
}

std::vector<viaems::Configuration> Log::LoadConfigs() {
  return {};
}

std::chrono::system_clock::time_point Log::EndTime() {
  return {};
}
