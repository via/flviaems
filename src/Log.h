#pragma once

#include <chrono>
#include <set>
#include <vector>
#include <fstream>
#include <thread>
#include <memory>

#include <mutex>
#include <condition_variable>

#include <sqlite3.h>

#include "viaems.h"

class Log {
  sqlite3 *db;
  std::vector<std::string> keys;
  sqlite3_stmt *insert_stmt;

  void ensure_db_schema(const viaems::LogChunk&);


public:
  Log();
  ~Log() {
    if (db != nullptr) {
      sqlite3_close(db);
    }
  };

  void LoadFromFile(std::istream &);
  void SetOutputFile(std::string path);
  void Update(const viaems::LogChunk&);

  viaems::LogChunk
  GetRange(std::vector<std::string> keys,
      std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point end);
};

struct LogWriter {
  Log log;
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<viaems::LogChunk> chunks;
  std::thread thread;

  void add_chunk(viaems::LogChunk&& chunk);
  void write_loop();
  void start() {
    thread = std::thread([](LogWriter *w) { w->write_loop(); }, this);
  }

};

