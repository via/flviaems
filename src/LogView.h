#pragma once

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
  void SetLog(Log *log) {this->log = log;};
  void update_time_range(uint64_t start, uint64_t stop);

private:
  Log *log;
  uint64_t start_ns, stop_ns, ns_per_pixel;
  int mouse_x, mouse_y;

  std::map<std::string, SeriesConfig> config;
  std::map<std::string, std::vector<PointGroup>> series;
  viaems::LogChunk cache;

  void update_pointgroups(int x0, int x1);
  int handle(int);
  void draw();
};
