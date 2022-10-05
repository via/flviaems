// generated by Fast Light User Interface Designer (fluid) version 1.0308

#include "MainWindowUI.h"

Fl_Menu_Item MainWindowUI::menu_m_bar[] = {
 {"File", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Open", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Load Config", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Export Config", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Import Config", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {"Target", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Flash", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Reset to bootloader", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {"Connection", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Device", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Simulator", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Offline", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {0,0,0,0,0,0,0,0,0}
};
Fl_Menu_Item* MainWindowUI::m_file_menu = MainWindowUI::menu_m_bar + 0;
Fl_Menu_Item* MainWindowUI::m_file_open = MainWindowUI::menu_m_bar + 1;
Fl_Menu_Item* MainWindowUI::m_file_loadconfig = MainWindowUI::menu_m_bar + 2;
Fl_Menu_Item* MainWindowUI::m_file_export = MainWindowUI::menu_m_bar + 3;
Fl_Menu_Item* MainWindowUI::m_file_import = MainWindowUI::menu_m_bar + 4;
Fl_Menu_Item* MainWindowUI::m_target_menu = MainWindowUI::menu_m_bar + 6;
Fl_Menu_Item* MainWindowUI::m_target_flash = MainWindowUI::menu_m_bar + 7;
Fl_Menu_Item* MainWindowUI::m_target_bootloader = MainWindowUI::menu_m_bar + 8;
Fl_Menu_Item* MainWindowUI::m_connection_menu = MainWindowUI::menu_m_bar + 10;
Fl_Menu_Item* MainWindowUI::m_connection_device = MainWindowUI::menu_m_bar + 11;
Fl_Menu_Item* MainWindowUI::m_connection_simulator = MainWindowUI::menu_m_bar + 12;
Fl_Menu_Item* MainWindowUI::m_connection_offline = MainWindowUI::menu_m_bar + 13;

MainWindowUI::MainWindowUI() {
  { m_main_window = new Fl_Double_Window(1020, 750, "mainWindow");
    m_main_window->box(FL_THIN_DOWN_BOX);
    m_main_window->user_data((void*)(this));
    { m_status_table = new StatusTable(10, 415, 430, 295, "Status");
      m_status_table->box(FL_THIN_DOWN_FRAME);
      m_status_table->color(FL_BACKGROUND_COLOR);
      m_status_table->selection_color(FL_BACKGROUND_COLOR);
      m_status_table->labeltype(FL_NORMAL_LABEL);
      m_status_table->labelfont(0);
      m_status_table->labelsize(14);
      m_status_table->labelcolor(FL_FOREGROUND_COLOR);
      m_status_table->align(Fl_Align(FL_ALIGN_TOP));
      m_status_table->when(FL_WHEN_RELEASE);
      m_status_table->end();
    } // StatusTable* m_status_table
    { Fl_Box* o = new Fl_Box(5, 721, 1010, 24, "Status:");
      o->box(FL_DOWN_BOX);
      o->align(Fl_Align(68|FL_ALIGN_INSIDE));
    } // Fl_Box* o
    { m_connection_status = new Fl_Box(170, 721, 40, 24, "Connection");
      m_connection_status->box(FL_UP_BOX);
      m_connection_status->color((Fl_Color)1);
      m_connection_status->align(Fl_Align(FL_ALIGN_LEFT));
    } // Fl_Box* m_connection_status
    { m_rate = new Fl_Output(385, 720, 50, 25, "Rate:");
    } // Fl_Output* m_rate
    { m_interrogation_progress = new Fl_Progress(875, 724, 135, 21, "Interrogation");
    } // Fl_Progress* m_interrogation_progress
    { m_table_editor_box = new Fl_Group(455, 40, 525, 345);
      m_table_editor_box->box(FL_DOWN_BOX);
      m_table_editor_box->hide();
      { m_table_title = new Fl_Input(496, 45, 119, 30, "Title");
      } // Fl_Input* m_table_title
      { m_table_editor = new TableEditor(460, 75, 510, 305);
        m_table_editor->box(FL_THIN_DOWN_FRAME);
        m_table_editor->color(FL_BACKGROUND_COLOR);
        m_table_editor->selection_color(FL_BACKGROUND_COLOR);
        m_table_editor->labeltype(FL_NORMAL_LABEL);
        m_table_editor->labelfont(0);
        m_table_editor->labelsize(14);
        m_table_editor->labelcolor(FL_FOREGROUND_COLOR);
        m_table_editor->align(Fl_Align(FL_ALIGN_TOP));
        m_table_editor->when(FL_WHEN_RELEASE);
        m_table_editor->when(FL_WHEN_RELEASE | FL_WHEN_NOT_CHANGED);
        m_table_editor->end();
        Fl_Group::current()->resizable(m_table_editor);
      } // TableEditor* m_table_editor
      { m_table_rows = new Fl_Spinner(660, 45, 40, 25, "Rows");
      } // Fl_Spinner* m_table_rows
      { m_table_cols = new Fl_Spinner(775, 45, 40, 25, "Columns");
      } // Fl_Spinner* m_table_cols
      { m_tracing = new Fl_Check_Button(880, 50, 50, 15, "Tracing");
        m_tracing->down_box(FL_DOWN_BOX);
      } // Fl_Check_Button* m_tracing
      m_table_editor_box->end();
    } // Fl_Group* m_table_editor_box
    { m_sensor_editor_box = new Fl_Scroll(455, 40, 545, 345);
      m_sensor_editor_box->box(FL_DOWN_BOX);
      m_sensor_editor_box->hide();
      { Fl_Group* o = new Fl_Group(490, 60, 260, 100, "Input");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_source = new Fl_Choice(645, 65, 95, 20, "Source");
          m_sensor_source->down_box(FL_BORDER_BOX);
        } // Fl_Choice* m_sensor_source
        { m_sensor_method = new Fl_Choice(645, 88, 95, 20, "Method");
          m_sensor_method->down_box(FL_BORDER_BOX);
        } // Fl_Choice* m_sensor_method
        { m_sensor_pin = new Fl_Value_Input(645, 112, 95, 20, "Pin");
          m_sensor_pin->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_pin
        { m_sensor_lag = new Fl_Value_Input(645, 135, 95, 20, "Lag Filter");
          m_sensor_lag->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_lag
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(785, 60, 195, 55, "Linear Mapping");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_range_min = new Fl_Value_Input(875, 65, 95, 20, "Minimum");
          m_sensor_range_min->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_range_min
        { m_sensor_range_max = new Fl_Value_Input(875, 88, 95, 20, "Maximum");
          m_sensor_range_max->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_range_max
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(785, 140, 195, 105, "Thermistor");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_therm_bias = new Fl_Value_Input(875, 145, 95, 20, "Bias");
          m_sensor_therm_bias->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_therm_bias
        { m_sensor_therm_A = new Fl_Value_Input(875, 170, 95, 20, "A");
          m_sensor_therm_A->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_therm_A
        { m_sensor_therm_B = new Fl_Value_Input(875, 195, 95, 20, "B");
          m_sensor_therm_B->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_therm_B
        { m_sensor_therm_C = new Fl_Value_Input(875, 220, 95, 20, "C");
          m_sensor_therm_C->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_therm_C
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(490, 280, 260, 80, "Windowing");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_window_width = new Fl_Value_Input(645, 285, 95, 20, "Capture Width");
          m_sensor_window_width->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_window_width
        { m_sensor_window_total = new Fl_Value_Input(645, 308, 95, 20, "Total Width");
          m_sensor_window_total->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_window_total
        { m_sensor_window_offset = new Fl_Value_Input(645, 333, 95, 20, "Window Start Offset");
          m_sensor_window_offset->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_window_offset
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(490, 180, 260, 80, "Fault");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_fault_min = new Fl_Value_Input(645, 185, 95, 20, "Minimum");
          m_sensor_fault_min->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_fault_min
        { m_sensor_fault_max = new Fl_Value_Input(645, 209, 95, 20, "Maximum");
          m_sensor_fault_max->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_fault_max
        { m_sensor_fault_value = new Fl_Value_Input(645, 233, 95, 20, "Value");
          m_sensor_fault_value->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_fault_value
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(785, 265, 195, 30, "Constant");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_const = new Fl_Value_Input(875, 270, 95, 20, "Value");
          m_sensor_const->when(FL_WHEN_RELEASE);
        } // Fl_Value_Input* m_sensor_const
        o->end();
      } // Fl_Group* o
      m_sensor_editor_box->end();
    } // Fl_Scroll* m_sensor_editor_box
    { m_bar = new Fl_Menu_Bar(0, 0, 1050, 20);
      m_bar->menu(menu_m_bar);
    } // Fl_Menu_Bar* m_bar
    { m_status_text = new Fl_Output(450, 720, 415, 25);
      m_status_text->box(FL_NO_BOX);
    } // Fl_Output* m_status_text
    { m_logview = new LogView(460, 423, 525, 271);
      m_logview->box(FL_DOWN_BOX);
      m_logview->color(FL_FOREGROUND_COLOR);
      m_logview->selection_color(FL_BACKGROUND_COLOR);
      m_logview->labeltype(FL_NORMAL_LABEL);
      m_logview->labelfont(0);
      m_logview->labelsize(14);
      m_logview->labelcolor(FL_FOREGROUND_COLOR);
      m_logview->align(Fl_Align(FL_ALIGN_CENTER));
      m_logview->when(FL_WHEN_RELEASE);
    } // LogView* m_logview
    { m_offline = new Fl_Output(215, 720, 80, 25);
      m_offline->box(FL_NO_BOX);
    } // Fl_Output* m_offline
    { m_logview_follow = new Fl_Button(810, 695, 25, 20, "@>|");
    } // Fl_Button* m_logview_follow
    { m_output_editor_box = new Fl_Group(455, 40, 525, 345);
      m_output_editor_box->box(FL_DOWN_BOX);
      m_output_editor_box->hide();
      { m_output_editor = new OutputEditor(460, 75, 510, 305);
        m_output_editor->box(FL_THIN_DOWN_FRAME);
        m_output_editor->color(FL_BACKGROUND_COLOR);
        m_output_editor->selection_color(FL_BACKGROUND_COLOR);
        m_output_editor->labeltype(FL_NORMAL_LABEL);
        m_output_editor->labelfont(0);
        m_output_editor->labelsize(14);
        m_output_editor->labelcolor(FL_FOREGROUND_COLOR);
        m_output_editor->align(Fl_Align(FL_ALIGN_TOP));
        m_output_editor->when(FL_WHEN_RELEASE);
        m_output_editor->end();
        Fl_Group::current()->resizable(m_output_editor);
      } // OutputEditor* m_output_editor
      m_output_editor_box->end();
      Fl_Group::current()->resizable(m_output_editor_box);
    } // Fl_Group* m_output_editor_box
    { m_logview_forward = new Fl_Button(785, 695, 25, 20, "@>>");
    } // Fl_Button* m_logview_forward
    { m_logview_pause = new Fl_Button(760, 695, 25, 20, "@||");
    } // Fl_Button* m_logview_pause
    { m_logview_back = new Fl_Button(735, 695, 25, 20, "@<<");
    } // Fl_Button* m_logview_back
    { m_logview_in = new Fl_Button(695, 695, 25, 20, "@8UpArrow");
    } // Fl_Button* m_logview_in
    { m_logview_out = new Fl_Button(670, 695, 25, 20, "@2UpArrow");
    } // Fl_Button* m_logview_out
    { Fl_Tabs* o = new Fl_Tabs(10, 25, 430, 365);
      { Fl_Group* o = new Fl_Group(10, 45, 430, 345, "Target");
        o->hide();
        { m_config_tree = new Fl_Tree(10, 45, 430, 345);
        } // Fl_Tree* m_config_tree
        o->end();
        Fl_Group::current()->resizable(o);
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(10, 45, 430, 345, "Log View");
        { m_logview_editor = new LogViewEditor(10, 70, 430, 320);
          m_logview_editor->box(FL_THIN_DOWN_FRAME);
          m_logview_editor->color(FL_BACKGROUND_COLOR);
          m_logview_editor->selection_color(FL_BACKGROUND_COLOR);
          m_logview_editor->labeltype(FL_NORMAL_LABEL);
          m_logview_editor->labelfont(0);
          m_logview_editor->labelsize(14);
          m_logview_editor->labelcolor(FL_FOREGROUND_COLOR);
          m_logview_editor->align(Fl_Align(FL_ALIGN_TOP));
          m_logview_editor->when(FL_WHEN_RELEASE);
          m_logview_editor->end();
        } // LogViewEditor* m_logview_editor
        o->end();
      } // Fl_Group* o
      o->end();
    } // Fl_Tabs* o
    m_main_window->set_non_modal();
    m_main_window->end();
  } // Fl_Double_Window* m_main_window
  m_main_window->show();
}
