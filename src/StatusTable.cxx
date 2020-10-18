#include "StatusTable.h"
#include <iostream>

#include <FL/fl_draw.H>

StatusTable::StatusTable(int X, int Y, int W, int H, const char *L = 0)
    : Fl_Table(X, Y, W, H, L) {
  rows(5);
  cols(2);
  col_width(0, 120);
  end();
}

void StatusTable::feed_update(viaems::FeedUpdate const &update) {
  rows(update.size());

  m_current_values.clear();
  std::string longest;
  for (auto i : update) {
    m_current_values.push_back(i);
    if (i.first.size() > longest.size()) {
      longest = i.first;
    }
  }

  int w = 0, h = 0;
  fl_measure(longest.c_str(), w, h, 0);
  col_width(0, w + 2);
  row_height_all(h + 1);

  this->redraw();
}

void draw_value_cell(viaems::FeedValue val, int X, int Y, int W, int H) {
  fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_WHITE);
  fl_color(FL_BLACK);
  fl_push_clip(X, Y, W, H);

  std::ostringstream o;
  std::visit([&](const auto &v) { o << v; }, val);
  fl_draw(o.str().c_str(), X, Y, W, H, FL_ALIGN_CENTER);

  fl_pop_clip();
}

void draw_key_cell(std::string key, int X, int Y, int W, int H) {
  fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_WHITE);
  fl_color(FL_BLACK);
  fl_push_clip(X, Y, W, H);

  fl_draw(key.c_str(), X + 1, Y, W, H, FL_ALIGN_LEFT);

  fl_pop_clip();
}

void StatusTable::draw_cell(TableContext c, int R, int C, int X, int Y, int W,
                            int H) {
  switch (c) {
  case CONTEXT_CELL:
    if (R >= m_current_values.size()) {
      return;
    }
    if (C == 1) {
      draw_value_cell(m_current_values.at(R).second, X, Y, W, H);
    } else {
      draw_key_cell(m_current_values.at(R).first, X, Y, W, H);
    }
    break;
  case CONTEXT_RC_RESIZE: {
    auto col0_w = col_width(0);
    col_width(1, std::max(0, w() - col0_w));
    break;
  }
  default:
    break;
  }
}
