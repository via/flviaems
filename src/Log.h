#include <chrono>
#include <set>
#include <vector>
#include <fstream>

#include <sqlite3.h>

#include "viaems.h"

struct LogPoint {
  viaems::FeedTime time;
  std::vector<viaems::FeedValue> values;
};

class Log {
  sqlite3 *db;
  std::set<std::string> keys;

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
  void Update(std::vector<viaems::FeedUpdate>);

  std::vector<LogPoint>
  GetRange(std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point end);
};
