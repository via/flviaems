#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include <condition_variable>
#include <mutex>

#include <sqlite3.h>

#include "viaems.h"

class LogRange {
  sqlite3_stmt *stmt;

  public:
    LogRange(sqlite3_stmt *stmt, std::vector<std::string> cols) : stmt{stmt} { }

    LogRange(const LogRange &) = delete;
    LogRange(LogRange &&rhs) {
      this->stmt = rhs.stmt;
      rhs.stmt = nullptr;
    }
    ~LogRange() {
      if (this->stmt) {
        sqlite3_finalize(this->stmt);
      }
    }

    uint64_t row_time() const {
      return sqlite3_column_int64(this->stmt, 0);
    }

    double row_value(int key_index) const {
      int sql_index = key_index + 1;
        return sqlite3_column_double(this->stmt, sql_index);
    }
    bool next() {
      int res = sqlite3_step(this->stmt);
      if ((res == SQLITE_DONE) || (res == SQLITE_MISUSE)) {
        return false;
      }
      return true;
    }
};

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

  std::optional<LogRange> GetLogRange(std::vector<std::string> keys,
                            std::chrono::system_clock::time_point start,
                            std::chrono::system_clock::time_point end);

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
