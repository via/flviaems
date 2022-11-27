#include "TableEditor.h"
#include <iostream>

#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

void TableEditor::cell_select_callback(Fl_Widget *w, void *ptr) {
  auto editor = static_cast<TableEditor *>(w);
  int R = editor->callback_row();
  int C = editor->callback_col();
  switch (editor->callback_context()) {
  case CONTEXT_TABLE:
  case CONTEXT_ROW_HEADER:
    break;
  case CONTEXT_COL_HEADER:
    break;
  case CONTEXT_CELL:
    switch(Fl::event()) {
    case FL_PUSH:
      editor->stop_editor();
      editor->start_editor(R, C);
      break;
    case FL_KEYBOARD:
      editor->stop_editor();
      auto x = Fl::event_key();
      if (x == FL_Enter) {
        editor->start_editor(R, C);
      } else if (x == '=') {
        editor->adjust_selection(1);
      } else if (x == '-') {
        editor->adjust_selection(-1);
      }
      break;
    }
  default:
    break;
  }
}

void TableEditor::adjust_selection(int amt) {
  int row_top, col_left, row_bot, col_right;
  this->get_selection(row_top, col_left, row_bot, col_right);

  if (this->table.axis.size() == 1) {
    for (int r = row_top; r <= row_bot; r++) {
      this->edit_changes.insert(std::make_pair(r, 0));
      this->table.one[r] += amt;
    }
  } else {
    for (int r = row_top; r <= row_bot; r++) {
      for (int c = col_left; c <= col_right; c++) {
        this->edit_changes.insert(std::make_pair(r, c));
        this->table.two[r][c] += amt;
      }
    }
  }
  this->parent()->do_callback();
}

void TableEditor::cell_value_callback(Fl_Widget *w, void *ptr) {
  auto editor = static_cast<TableEditor *>(ptr);
  float value;

  int row_top, col_left, row_bot, col_right;
  editor->get_selection(row_top, col_left, row_bot, col_right);

  std::istringstream ss{editor->input->value()};
  ss >> value;

  if (editor->table.axis.size() == 1) {
    for (int r = row_top; r <= row_bot; r++) {
      editor->edit_changes.insert(std::make_pair(r, 0));
      editor->table.one[r] = value;
    }
  } else {
    for (int r = row_top; r <= row_bot; r++) {
      for (int c = col_left; c <= col_right; c++) {
        editor->edit_changes.insert(std::make_pair(r, c));
        editor->table.two[r][c] = value;
      }
    }
  }

  editor->parent()->do_callback();
  editor->stop_editor();
}

void TableEditor::stop_editor() {
  input->hide();
  take_focus();
  window()->cursor(FL_CURSOR_DEFAULT);
}

void TableEditor::start_editor(int r, int c) {
  set_selection(r, c, r, c);

  int X, Y, W, H;
  if (r == -1) {
    /* Find Column header */
    find_cell(CONTEXT_COL_HEADER, 0, c, X, Y, W, H);
    input->resize(X, Y, W, H);
    input->value("100");
  } else if (c == -1) {
    /* Find Row header */
  } else {
    find_cell(CONTEXT_CELL, r, c, X, Y, W, H);
    input->resize(X, Y, W, H);
    auto v = cell_value(r, c);
    input->value(v.c_str());
    input->position(0, v.size());
  }
  input->show();
  input->take_focus();
  edit_r = r;
  edit_c = c;
}

TableEditor::TableEditor(int X, int Y, int W, int H) : Fl_Table(X, Y, W, H) {
  row_header(1);
  col_header(0);
  rows(0);
  cols(0);
  set_selection(0, 0, 0, 0);
  callback(cell_select_callback);
  when(FL_WHEN_NOT_CHANGED | when());

  input = new Fl_Float_Input(0, 0, 0, 0);
  input->callback(cell_value_callback, this);
  input->color(FL_YELLOW);
  input->when(FL_WHEN_ENTER_KEY_ALWAYS);
  input->hide();
  end();
}

void TableEditor::setTable(viaems::TableValue t) {
  this->table = t;
  if (this->table.axis.size() == 1) {
    rows(this->table.one.size());
    cols(1);
    col_header(0);
  } else {
    rows(this->table.two.size());
    cols(this->table.two[0].size());
    col_header(1);
  }
  col_width_all(40);
  row_height_all(25);
  edit_changes.clear();
}

std::string TableEditor::cell_value(int r, int c) {
  char buf[32];
  if (this->table.axis.size() == 2) {
    snprintf(buf, sizeof(buf), "%.3g", this->table.two[r][c]);
  } else {
    snprintf(buf, sizeof(buf), "%.3g", this->table.one[r]);
  }
  return std::string{buf};
}

void TableEditor::draw_cell(TableContext c, int R, int C, int X, int Y, int W,
                            int H) {
  char buf[32];
  switch (c) {
  case CONTEXT_ROW_HEADER:
    fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, fl_inactive(FL_WHITE));
    fl_color(FL_BLACK);
    fl_push_clip(X, Y, W, H);
    if (this->table.axis.size() == 2) {
      snprintf(buf, sizeof(buf), "%.4g", this->table.axis[1].labels[R]);
    } else {
      snprintf(buf, sizeof(buf), "%.4g", this->table.axis[0].labels[R]);
    }
    fl_draw(buf, X, Y, W, H, FL_ALIGN_CENTER);
    fl_pop_clip();
    break;
  case CONTEXT_COL_HEADER:
    fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, fl_inactive(FL_WHITE));
    fl_color(FL_BLACK);
    fl_push_clip(X, Y, W, H);
    snprintf(buf, sizeof(buf), "%.4g", this->table.axis[0].labels[C]);
    fl_draw(buf, X, Y, W, H, FL_ALIGN_CENTER);
    fl_pop_clip();
    break;
  case CONTEXT_CELL: {
    if (input->visible() && (R == edit_r) && (C == edit_c)) {
      return;
    }
    if (edit_changes.count(std::make_pair(R, C)) > 0) {
      fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H,
                  is_selected(R, C) ? FL_DARK_YELLOW : FL_RED);
    } else {
      fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H,
                  is_selected(R, C) ? FL_YELLOW : FL_WHITE);
    }
    fl_color(FL_BLACK);
    fl_push_clip(X, Y, W, H);

    buf[31] = '\0';
    fl_draw(cell_value(R, C).c_str(), X + 1, Y, W, H, FL_ALIGN_CENTER);
    fl_pop_clip();
  } break;
  case CONTEXT_RC_RESIZE: {
  }
  default:
    break;
  }
}
