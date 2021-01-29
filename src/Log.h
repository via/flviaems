#pragma once

#include <chrono>
#include <set>
#include <vector>
#include <fstream>

#include <sqlite3.h>

#include "viaems.h"

struct LogPoint {
  std::chrono::system_clock::time_point time;
  std::array<viaems::FeedValue, 2> values;
};

struct LogChunk {
  std::deque<LogPoint> points;
  std::vector<std::string> keys;
};

class Log {
  sqlite3 *db;
  std::set<std::string> keys;
  sqlite3_stmt *insert_stmt;

  bool in_transaction = false;
  int cur_transaction_size = 0;
  int max_transaction_size = 5000;

  void ensure_db_schema(const viaems::FeedUpdate&);


public:
  Log();
  ~Log() {
    if (db != nullptr) {
      sqlite3_close(db);
    }
  };

  void LoadFromFile(std::istream &);
  void SetOutputFile(std::string path);
  void Update(const std::vector<viaems::FeedUpdate>&);

#if 0
  std::vector<viaems::FeedUpdate>
  GetRange(std::vector<std::string> keys,
      std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point end);
#endif
  LogChunk
  GetRange(std::vector<std::string> keys,
      std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point end);
};
