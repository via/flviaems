#pragma once

#include <chrono>
#include <deque>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>

#include "Log.h"
#include "viaems.h"

struct PointGroup {
  float first;
  float last;
  float min;
  float max;
  bool set;
};

struct SeriesConfig {
  float min_y;
  float max_y;
  Fl_Color color;
};

class LogView : public Fl_Box {
public:
  LogView(int X, int Y, int W, int H);
  void SetLog(std::weak_ptr<Log> log);
  void update_time_range(std::chrono::system_clock::time_point start,
                         std::chrono::system_clock::time_point stop);

private:
  std::weak_ptr<Log> log;
  uint64_t start_ns, stop_ns;
  int mouse_x, mouse_y;

  std::map<std::string, SeriesConfig> config;
  std::map<std::string, std::vector<PointGroup>> series;
  viaems::LogChunk cache;

  int handle(int);
  void resize(int, int, int, int);
  void recompute();
  void draw();
};
