#pragma once

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
  Log();
  ~Log() {
    if (db != nullptr) {
      sqlite3_close(db);
    }
  };

  void SetFile(std::string path);
  void WriteChunk(viaems::LogChunk &&);

  viaems::LogChunk GetRange(std::vector<std::string> keys,
                            std::chrono::system_clock::time_point start,
                            std::chrono::system_clock::time_point end);
};

class ThreadedWriteLog : public Log {
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<viaems::LogChunk> chunks;
  std::thread thread;
  bool running;

  void write_loop();

public:
  ~ThreadedWriteLog() {
    running = false;
    cv.notify_one();
    thread.join();
  }

  ThreadedWriteLog() {
    running = true;
    thread = std::thread([](ThreadedWriteLog *w) { w->write_loop(); }, this);
  }

  void WriteChunk(viaems::LogChunk &&);
};
