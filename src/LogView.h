#pragma once

#include <chrono>
#include <deque>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>

#include "Log.h"
#include "viaems.h"


class LogView : public Fl_Box {
public:
  LogView(int X, int Y, int W, int H);
  void SetLog(Log *log) {this->log = log;};
  void update_time_range(std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point stop);

private:
  Log *log;
  std::chrono::system_clock::time_point start, stop;
  LogChunk chunk;
  std::vector<int> real_points;
  int mouse_x, mouse_y;

  int handle(int);

  void draw();
};
