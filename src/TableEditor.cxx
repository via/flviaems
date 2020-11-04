#include "TableEditor.h"
#include <iostream>

#include <FL/fl_draw.H>

TableEditor::TableEditor(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H) {
  rows(0);
  cols(0);
  end();
}

void TableEditor::setTable(viaems::TableValue t) {
  this->table = t;
  if (this->table.axis.size() == 1) {
    rows(this->table.one.size());
    cols(1);
  } else {
    rows(this->table.two.size());
    cols(this->table.two[0].size());
  }
  col_width_all(40);
  row_height_all(25);
} 

void TableEditor::draw_cell(TableContext c, int R, int C, int X, int Y, int W,
                            int H) {
  switch (c) {
  case CONTEXT_CELL:
    {
    fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_WHITE);
    fl_color(FL_BLACK);
    fl_push_clip(X, Y, W, H);

    char buf[32];
    if (this->table.axis.size() == 2) {
      snprintf(buf, sizeof(buf), "%.3g", this->table.two[R][C]);
    } else {
      snprintf(buf, sizeof(buf), "%.3g", this->table.one[R]);
    }
    buf[31] = '\0';
    fl_draw(buf, X + 1, Y, W, H, FL_ALIGN_CENTER);
    fl_pop_clip();
    }
    break;
  case CONTEXT_RC_RESIZE: {
                          }
  default:
    break;
  }
}
