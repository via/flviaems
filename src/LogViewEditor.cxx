#include "LogViewEditor.h"

#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/fl_draw.H>

LogViewEditor::LogViewEditor(int X, int Y, int W, int H) : Fl_Table(X, Y, W, H) {
  clear();
  row_header(1);
  col_header(1);
  cols(4);
  row_header_width(180);
  col_width_all(50);

  end();
}

void LogViewEditor::set_logview(LogView *lv) {
  this->lv = lv;

  rows(lv->config.size());
  for (const auto &[name, conf] : lv->config) {
    struct LogViewFeedRow row = {
      .name = name,
      .min = new Fl_Value_Input(x(), y(), w(), h()),
      .max = new Fl_Value_Input(x(), y(), w(), h()),
      .enabled = new Fl_Check_Button(x(), y(), w(), h()),
      .autosize = new Fl_Button(x(), y(), w(), h(), "Auto"),
    };

    this->feed.push_back(row);

    row.min->value(conf.min_y);
    row.min->callback([](Fl_Widget *w, void *p){
      LogViewEditor *lve = (LogViewEditor *)p;
      int row = lve->find(w) / lve->cols();
      const struct LogViewFeedRow &r = lve->feed[row];

      lve->lv->config[r.name].min_y = r.min->value();
      lve->lv->update();
    }, this);

    row.max->value(conf.max_y);
    row.max->callback([](Fl_Widget *w, void *p){
      LogViewEditor *lve = (LogViewEditor *)p;
      int row = lve->find(w) / lve->cols();
      const struct LogViewFeedRow &r = lve->feed[row];

      lve->lv->config[r.name].max_y = r.max->value();
      lve->lv->update();
    }, this);

    row.enabled->value(conf.enabled ? 1 : 0);
    row.enabled->callback([](Fl_Widget *w, void *p){
      LogViewEditor *lve = (LogViewEditor *)p;
      int row = lve->find(w) / lve->cols();
      const struct LogViewFeedRow &r = lve->feed[row];

      lve->lv->config[r.name].enabled = r.enabled->value() ? true : false;
      lve->lv->update();
    }, this);

    add(row.enabled);
    add(row.min);
    add(row.max);
    add(row.autosize);

  }
  place_widgets();
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


  switch (c) {
  case CONTEXT_ROW_HEADER: {
    auto rowname = this->feed[R].name;
    fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BLACK);
    fl_push_clip(X, Y, W, H);
    fl_color(lv->config[rowname].color);
    char buf[32];
    sprintf(buf, "%s", rowname.c_str());
    fl_draw(buf, X, Y, W, H, FL_ALIGN_CENTER);
    fl_pop_clip();
    break;
  }
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
