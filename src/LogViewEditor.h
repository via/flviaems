#pragma once

#include <set>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Value_Input.H>

#include "viaems.h"
#include "LogView.h"

struct LogViewFeedRow {
  std::string name;

  Fl_Value_Input *min;
  Fl_Value_Input *max;
  Fl_Check_Button *enabled;
  Fl_Button *autosize;
};

class LogViewEditor : public Fl_Table {
public:
  LogViewEditor(int X, int Y, int W, int H);
  void set_logview(LogView *);
private:
  LogView *lv;
  std::vector<LogViewFeedRow> feed;
  

  void draw_cell(TableContext c, int R, int C, int X, int Y, int W, int H);
  void place_widgets();
};

class LogViewEditorWindow : public Fl_Window {
public:
  LogViewEditorWindow(int X, int Y, int W, int H) : Fl_Window(X, Y, W, H, "Edit Log View") { 
    editor = new LogViewEditor(0, 0, w(), h());
    resizable(editor);
  }

  void set_logview(LogView *lv) {
    this->editor->set_logview(lv);
  }

  private:
    LogViewEditor *editor;
};

