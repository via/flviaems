#include "LogViewEditor.h"

#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/fl_draw.H>

LogViewEditor::LogViewEditor(int X, int Y, int W, int H, LogView &lh) : Fl_Table(X, Y, W, H), lh{lh} {
  clear();
  row_header(1);
  col_header(1);
  rows(lh.config.size());
  cols(4);
  row_header_width(180);

  this->feed.reserve(lh.config.size());
  for (const auto &[name, conf] : lh.config) {
    struct LogViewFeedRow row = {
      .name = name,
      .min = new Fl_Value_Input(x(), y(), w(), h()),
      .max = new Fl_Value_Input(x(), y(), w(), h()),
      .enabled = new Fl_Check_Button(x(), y(), w(), h()),
      .autosize = new Fl_Button(x(), y(), w(), h(), "Auto"),
    };

    this->feed.push_back(row);

    row.min->value(conf.min_y);
    row.max->value(conf.max_y);
    row.enabled->value(conf.enabled ? 1 : 0);
    row.enabled->callback([](Fl_Widget *w, void *p){
      LogViewEditor *lve = (LogViewEditor *)p;
      int row = lve->find(w) / lve->cols();
      const struct LogViewFeedRow &r = lve->feed[row];

      lve->lh.config[r.name].enabled = r.enabled->value() ? true : false;
      lve->parent()->do_callback();
    }, this);

  }
  place_widgets();
  end();
}

void LogViewEditor::place_widgets() {
  int index = 0;
  int X, Y, W, H;
  for (auto const &w : feed) {
    find_cell(CONTEXT_TABLE, index, 0, X, Y, W, H);
    w.enabled->resize(X, Y, W, H);

    find_cell(CONTEXT_TABLE, index, 1, X, Y, W, H);
    w.min->resize(X, Y, W, H);

    find_cell(CONTEXT_TABLE, index, 2, X, Y, W, H);
    w.max->resize(X, Y, W, H);

    find_cell(CONTEXT_TABLE, index, 3, X, Y, W, H);
    w.autosize->resize(X, Y, W, H);

    index += 1;
  }
}

static std::vector<std::string> headers = {"Enabled", "Min", "Max", ""};

void LogViewEditor::draw_cell(TableContext c, int R, int C, int X, int Y, int W,
                             int H) {
  std::vector<std::string> keys;
  for (const auto &i : lh.config) {
    keys.push_back(i.first);
  }
  switch (c) {
  case CONTEXT_ROW_HEADER:
    fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BLACK);
    fl_push_clip(X, Y, W, H);
    fl_color(lh.config[keys[R]].color);
    char buf[32];
    sprintf(buf, "%s", keys[R].c_str());
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
