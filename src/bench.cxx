#include <iostream>
#include "viaems.h"

#include "LogView.h"

static std::vector<std::string> keys = {"rpm", "sensor.map", "sensor.iat",
    "sensor.ego", "t0_count", "t1_count"};
static std::vector<viaems::FeedValue> values = {(uint32_t)1000, (float)80.0,
  (float)25.0, (float)0.95,
    (uint32_t)1, (uint32_t)5};

std::unique_ptr<viaems::LogChunk> generate_chunk(uint64_t start_time, uint64_t
    stop_time, int count) {
  auto chunk = std::make_unique<viaems::LogChunk>();
  chunk->keys = keys;
  int step = (stop_time - start_time) / count;
  if (step == 0) {
    step = 1;
  }
  for (uint64_t time = start_time; time <= stop_time; time += step) {
    viaems::LogPoint p = {
      .time = std::chrono::system_clock::time_point{std::chrono::nanoseconds{time}},
      .values = values,
    };
    chunk->points.push_back(p);
  }
  return chunk;
}


int main() {
  LogView lv{0, 0, 100, 100};
  auto log = std::make_shared<Log>("bench.vlog");

  std::chrono::system_clock::duration generate_time;
  std::chrono::system_clock::duration write_time;

  int count = 0;
  for (uint64_t start = 1000000000; start < 60000000000; start += 1000000000, count++) {

    auto bstart = std::chrono::system_clock::now();
    auto chunk = generate_chunk(start, start + 1000000000, 5000);
    auto bend = std::chrono::system_clock::now();
    generate_time += (bend - bstart);

    bstart = std::chrono::system_clock::now();
    log->WriteChunk(std::move(*chunk));
    bend = std::chrono::system_clock::now();
    write_time += (bend - bstart);
  }

  generate_time /= count;
  write_time /= count;

  std::cout << "generate 60 chunks with 5000 entries each" << std::endl;
  std::cout << "generate time per chunk (us): " << std::chrono::duration_cast<std::chrono::microseconds>(generate_time).count() << std::endl;
  std::cout << "write time per chunk (us): " << std::chrono::duration_cast<std::chrono::microseconds>(write_time).count() << std::endl;

  auto end = log->EndTime();
  auto start = end - std::chrono::seconds{20};
  lv.SetLog(std::weak_ptr{log});

  auto bstart = std::chrono::system_clock::now();
  lv.update_time_range(start, end);
  auto bend = std::chrono::system_clock::now();

  std::cout << "update_time_range: " <<
    std::chrono::duration_cast<std::chrono::microseconds>(bend - bstart).count() << std::endl;

  start -= std::chrono::seconds{20};
  end -= std::chrono::seconds{20};
  bstart = std::chrono::system_clock::now();
  lv.update_time_range(start, end);
  bend = std::chrono::system_clock::now();

  std::cout << "update_time_range: " <<
    std::chrono::duration_cast<std::chrono::microseconds>(bend - bstart).count() << std::endl;
  return 0;

}

