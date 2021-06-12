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

class OutputEditor : public Fl_Table {
public:
  OutputEditor(int X, int Y, int W, int H);

  void set_outputs(std::vector<viaems::OutputValue> outputs);
  std::vector<viaems::OutputValue> get_outputs();
  void changed_callback(std::function<void(int, viaems::OutputValue)> cb) {
    changed_cb_func = cb;
  }

private:
  std::vector<OutputWidgets> widgets;
  std::function<void(int, viaems::OutputValue)> changed_cb_func;
  void draw_cell(TableContext c, int R, int C, int X, int Y, int W, int H);
  static void value_changed(Fl_Widget *, void *);
  void place_widgets();
};
