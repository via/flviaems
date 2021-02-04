#pragma once

#include <chrono>
#include <set>
#include <vector>
#include <fstream>
#include <thread>
#include <memory>

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
