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

struct ResultColumn {
  int column;
  bool is_real;
};

class LogRange {
  sqlite3_stmt *stmt;
  std::vector<std::string> column_names;

  public:
    LogRange(sqlite3_stmt *stmt, std::vector<std::string> cols) : stmt{stmt}, column_names{cols} { }

    LogRange(const LogRange &) = delete;
    LogRange(const LogRange &&rhs) {
      this->stmt = rhs.stmt;
      this->column_names = std::move(rhs.column_names);
    }
    ~LogRange() {
      sqlite3_finalize(this->stmt);
    }

    ResultColumn Column(int index) const {
      auto coltype = sqlite3_column_type(this->stmt, index);
      if (coltype == SQLITE_FLOAT) {
        return ResultColumn{index, true};
      } else {
        return ResultColumn{index, false};
      }
    }
      
    uint64_t row_time() const {
      return sqlite3_column_int64(this->stmt, 0);
    }

    viaems::FeedValue row_value(const ResultColumn &c) const {
      if (c.is_real) {
        return viaems::FeedValue((float)sqlite3_column_double(this->stmt, c.column));
      } else {
        return viaems::FeedValue((uint32_t)sqlite3_column_int64(this->stmt, c.column));
      }
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

  LogRange GetLogRange(std::vector<std::string> keys,
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
