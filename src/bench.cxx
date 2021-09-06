#include <iostream>
#include "viaems.h"

#include "LogView.h"


int main() {
  std::cout << "Benchmarking!" << std::endl;
  LogView lv{0, 0, 100, 100};
  auto log = std::make_shared<Log>("log.vlog");
  auto end = log->EndTime();
  auto start = end - std::chrono::seconds{120};
  lv.SetLog(std::weak_ptr{log});

  auto bstart = std::chrono::system_clock::now();
  lv.update_time_range(start, end);
  auto bend = std::chrono::system_clock::now();

  std::cout << "update_time_range: " <<
    std::chrono::duration_cast<std::chrono::microseconds>(bend - bstart).count() << std::endl;

  start -= std::chrono::seconds{120};
  end -= std::chrono::seconds{120};
  bstart = std::chrono::system_clock::now();
  lv.update_time_range(start, end);
  bend = std::chrono::system_clock::now();

  std::cout << "update_time_range: " <<
    std::chrono::duration_cast<std::chrono::microseconds>(bend - bstart).count() << std::endl;
  return 0;

}

