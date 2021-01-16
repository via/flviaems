#include <chrono>
#include <deque>
#include <vector>
#include <fstream>

#include "viaems.h"

struct LogPoint {
  viaems::FeedTime time;
  std::vector<viaems::FeedValue> values;
};

struct LogSection {
  uint64_t file_position;
  uint64_t section_length;

  bool ready;
  bool dirty;

  std::vector<std::string> keys;

  std::vector<LogPoint> points;
  bool full() {return points.size() > 50000;};
};

class Log {
  std::fstream logfile;
  std::vector<LogSection> sections;

  void ensure_section_usable();

public:
  Log();
  void LoadFromFile(std::istream &);
  void SetOutputFile(std::string path);
  void Update(std::vector<viaems::FeedUpdate>);

  std::vector<LogPoint>
  GetRange(std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point end);
};
