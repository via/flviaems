#include <deque>
#include <vector>
#include <chrono>

#include "viaems.h"

class Log {
  std::fstream logfile;
  std::deque<viaems::FeedUpdate> log;

  public:
    Log();
    void LoadFromFile(std::istream&);
    void SetOutputFile(std::string path);
    void Update(std::vector<viaems::FeedUpdate>);

    std::vector<viaems::FeedUpdate> GetRange(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end);
};
