#include "LogViewEditor.h"

#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

LogViewEditor::LogViewEditor(int X, int Y, int W, int H) : Fl_Table(X, Y, W, H) {
  clear();
  row_header(1);
  col_header(1);
  rows(0);
  cols(4);
  set_selection(0, 0, 0, 0);
  end();
}

void LogViewEditor::place_widgets() {
  int index = 0;
  int X, Y, W, H;
}

static std::vector<std::string> headers = {"Name", "Show", "Min", "Max"};

void LogViewEditor::draw_cell(TableContext c, int R, int C, int X, int Y, int W,
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
