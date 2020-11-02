#pragma once

#include <FL/Fl.H>
#include <FL/Fl_Table.H>

#include "viaems.h"

class TableEditor : public Fl_Table {
public:
  TableEditor(int X, int Y, int W, int H);

  void setTable(std::shared_ptr<viaems::NodeModel> table);
private:
  std::shared_ptr<viaems::NodeModel> m_table;
  void draw_cell(TableContext c, int R, int C, int X, int Y, int W, int H);
};
