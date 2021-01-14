#pragma once

#include <FL/Fl.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Table.H>

#include "viaems.h"

class TableEditor : public Fl_Table {
public:
  TableEditor(int X, int Y, int W, int H);

  void setTable(viaems::TableValue table);
  viaems::TableValue getTable() { return table; };

private:
  Fl_Float_Input *input;
  viaems::TableValue table;

  void draw_cell(TableContext c, int R, int C, int X, int Y, int W, int H);
  std::string cell_value(int r, int c);

  static void cell_select_callback(Fl_Widget *w, void *);
  static void cell_value_callback(Fl_Widget *w, void *);
  void start_editor(int R, int C);
  void stop_editor();

  int edit_r, edit_c;
};
