#pragma once

#include <chrono>
#include <set>
#include <vector>
#include <fstream>
#include <thread>

#include <sqlite3.h>

#include "viaems.h"

class Log {
  sqlite3 *db;
  std::vector<std::string> keys;
  sqlite3_stmt *insert_stmt;

  bool in_transaction = false;
  int cur_transaction_size = 0;
  int max_transaction_size = 5000;

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
  void Commit();

  viaems::LogChunk
  GetRange(std::vector<std::string> keys,
      std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point end);
};
