#include "OutputEditor.h"
#include <iostream>

#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

void OutputEditor::value_changed(Fl_Widget *w, void *ptr) {
  OutputEditor *oe = (OutputEditor *)ptr;
  int row = oe->find(w) / 4;

  auto val = oe->get_outputs().at(row);
  if (oe->changed_cb_func) {
    oe->changed_cb_func(row, val);
  }
}

OutputEditor::OutputEditor(int X, int Y, int W, int H) : Fl_Table(X, Y, W, H) {
  end();
}

std::vector<viaems::OutputValue> OutputEditor::get_outputs() {
  std::vector<viaems::OutputValue> result;
  for (const auto &widget : widgets) {
    viaems::OutputValue value;
    value.type = widget.type->text();
    value.pin = widget.pin->value();
    value.angle = widget.angle->value();
    value.inverted = widget.inverted->value();
    result.push_back(value);
  }
  return result;
}

void OutputEditor::set_outputs(std::vector<viaems::OutputValue> outputs) {

  clear();
  widgets.clear();
  row_header(1);
  col_header(1);
  rows(outputs.size());
  cols(4);
  set_selection(0, 0, 0, 0);

  begin();
  int index = 0;
  for (const auto &output : outputs) {
    OutputWidgets widget;

    widget.type = new Fl_Choice(x(), y(), w(), h());
    widget.type->callback(value_changed, this);
    widget.type->when(FL_WHEN_RELEASE);
    widget.type->add("disabled");
    widget.type->add("ignition");
    widget.type->add("fuel");
    if (output.type == "fuel") {
      widget.type->value(2);
    } else if (output.type == "ignition") {
      widget.type->value(1);
    } else {
      widget.type->value(0);
    }

    widget.pin = new Fl_Value_Input(x(), y(), w(), h());
    widget.pin->callback(value_changed, this);
    widget.pin->when(FL_WHEN_RELEASE);
    widget.pin->value(output.pin);

    widget.angle = new Fl_Value_Input(x(), y(), w(), h());
    widget.angle->callback(value_changed, this);
    widget.angle->when(FL_WHEN_RELEASE);
    widget.angle->value(output.angle);

    widget.inverted = new Fl_Check_Button(x(), y(), w(), h());
    widget.inverted->callback(value_changed, this);
    widget.inverted->when(FL_WHEN_RELEASE);
    widget.inverted->value(output.inverted ? 1 : 0);

    if (output.type == "disabled") {
      widget.pin->deactivate();
      widget.angle->deactivate();
      widget.inverted->deactivate();
    } else {
      widget.pin->activate();
      widget.angle->activate();
      widget.inverted->activate();
    }

    widgets.push_back(widget);
    index += 1;
  }
  place_widgets();
  end();
}

void OutputEditor::place_widgets() {
  int index = 0;
  int X, Y, W, H;
  for (auto const &w : widgets) {
    find_cell(CONTEXT_TABLE, index, 0, X, Y, W, H);
    w.type->resize(X, Y, W, H);

    find_cell(CONTEXT_TABLE, index, 1, X, Y, W, H);
    w.pin->resize(X, Y, W, H);

    find_cell(CONTEXT_TABLE, index, 2, X, Y, W, H);
    w.angle->resize(X, Y, W, H);

    find_cell(CONTEXT_TABLE, index, 3, X, Y, W, H);
    w.inverted->resize(X, Y, W, H);

    index += 1;
  }
}

static std::vector<std::string> headers = {"Type", "Pin", "Angle", "Inverted"};

void OutputEditor::draw_cell(TableContext c, int R, int C, int X, int Y, int W,
                             int H) {
  switch (c) {
  case CONTEXT_ROW_HEADER:
    fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, fl_inactive(FL_WHITE));
    fl_push_clip(X, Y, W, H);
    fl_color(FL_BLACK);
    char buf[16];
    sprintf(buf, "%d", R);
    fl_draw(buf, X, Y, W, H, FL_ALIGN_CENTER);
    fl_pop_clip();
    break;
  case CONTEXT_COL_HEADER:
    fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, fl_inactive(FL_WHITE));
    fl_push_clip(X, Y, W, H);
    fl_color(FL_BLACK);
    fl_draw(headers.at(C).c_str(), X, Y, W, H, FL_ALIGN_CENTER);
    fl_pop_clip();
    break;
  case CONTEXT_CELL:
    break;
  case CONTEXT_RC_RESIZE:
    place_widgets();
    break;
  default:
    break;
  }
}
