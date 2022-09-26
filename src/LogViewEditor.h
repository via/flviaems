#pragma once

#include <set>

#include <FL/Fl.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Value_Input.H>

#include "viaems.h"

struct OutputWidgets {
  Fl_Choice *type;
  Fl_Value_Input *angle;
  Fl_Value_Input *pin;
  Fl_Check_Button *inverted;
};

class LogViewEditor : public Fl_Table {
public:
  LogViewEditor(int X, int Y, int W, int H);

private:
  void draw_cell(TableContext c, int R, int C, int X, int Y, int W, int H);
  void place_widgets();
};
