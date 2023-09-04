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

class Log {
  sqlite3 *db;

public:
  Log(std::string path);
  Log(const Log &) = delete;
  Log &operator=(const Log &) = delete;
  ~Log() {
    if (db != nullptr) {
      auto res = sqlite3_close(db);
    }
  };

  void WriteChunk(viaems::LogChunk &&);

  viaems::LogChunk GetRange(std::vector<std::string> keys,
                            std::chrono::system_clock::time_point start,
                            std::chrono::system_clock::time_point end);
  std::vector<std::string> Keys() const;

  void SaveConfig(viaems::Configuration);
  std::vector<viaems::Configuration> LoadConfigs();

  std::chrono::system_clock::time_point EndTime();
};

class ThreadedWriteLog : public Log {
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<viaems::LogChunk> chunks;
  std::thread thread;
  std::atomic<bool> running;

  void write_loop();

public:
  ~ThreadedWriteLog() {
    std::unique_lock<std::mutex> lock(mutex);
    running = false;
    cv.notify_one();
    lock.unlock();
    thread.join();
  }

  ThreadedWriteLog(std::string path) : Log(path) {
    running = true;
    thread = std::thread([](ThreadedWriteLog *w) { w->write_loop(); }, this);
  }

  void WriteChunk(viaems::LogChunk &&);
};
