#pragma once

#include <FL/Fl.H>
#include <FL/Fl_Table.H>

#include "viaems.h"

class StatusTable : public Fl_Table {
public:
  StatusTable(int X, int Y, int W, int H, const char *L);

  void feed_update(std::map<std::string, viaems::FeedValue>);

private:
  std::vector<std::pair<std::string, viaems::FeedValue>> m_current_values;
  void draw_cell(TableContext c, int R, int C, int X, int Y, int W, int H);
};
