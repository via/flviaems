#pragma once

#include <chrono>
#include <deque>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>

#include "Log.h"
#include "viaems.h"

struct PointGroup {
  float mean;
  float min;
  float max;
};

struct SeriesConfig {
  float min_y;
  float max_y;
};

class LogView : public Fl_Box {
public:
  LogView(int X, int Y, int W, int H);
  void SetLog(Log *log) {this->log = log;};
  void update_time_range(std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point stop);

private:
  Log *log;
  std::chrono::system_clock::time_point start, stop;
  int mouse_x, mouse_y;

  std::map<std::string, SeriesConfig> config;
  std::map<std::string, std::vector<PointGroup>> series;
  viaems::LogChunk cache;

  int handle(int);
  void draw();
};
