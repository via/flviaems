#include "TableEditor.h"
#include <iostream>

#include <FL/fl_draw.H>

TableEditor::TableEditor(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H) {
  rows(1);
  cols(2);
  end();
}


void TableEditor::draw_cell(TableContext c, int R, int C, int X, int Y, int W,
                            int H) {
  switch (c) {
  case CONTEXT_CELL:
  case CONTEXT_RC_RESIZE: {
                          }
  default:
    break;
  }
}
