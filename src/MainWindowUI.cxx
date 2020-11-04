// generated by Fast Light User Interface Designer (fluid) version 1.0305

#include "MainWindowUI.h"

Fl_Menu_Item MainWindowUI::menu_m_bar[] = {
 {"File", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Connect...", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Socket", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"File", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {"Save All", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Flash", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {0,0,0,0,0,0,0,0,0}
};
Fl_Menu_Item* MainWindowUI::m_file_menu = MainWindowUI::menu_m_bar + 0;
Fl_Menu_Item* MainWindowUI::m_file_connect = MainWindowUI::menu_m_bar + 1;
Fl_Menu_Item* MainWindowUI::m_file_connect_socket = MainWindowUI::menu_m_bar + 2;
Fl_Menu_Item* MainWindowUI::m_file_connect_file = MainWindowUI::menu_m_bar + 3;
Fl_Menu_Item* MainWindowUI::m_file_saveall = MainWindowUI::menu_m_bar + 5;
Fl_Menu_Item* MainWindowUI::m_file_flash = MainWindowUI::menu_m_bar + 6;

MainWindowUI::MainWindowUI() {
  { m_main_window = new Fl_Double_Window(1054, 1059, "mainWindow");
    m_main_window->box(FL_THIN_DOWN_BOX);
    m_main_window->user_data((void*)(this));
    { m_config_tree = new Fl_Tree(20, 40, 400, 440, "Configuration");
    } // Fl_Tree* m_config_tree
    { m_status_table = new StatusTable(20, 505, 400, 445, "Status");
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
    { Fl_Box* o = new Fl_Box(10, 968, 990, 24, "Status:");
      o->box(FL_DOWN_BOX);
      o->align(Fl_Align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE));
    } // Fl_Box* o
    { m_connection_status = new Fl_Box(195, 968, 40, 24, "Connection");
      m_connection_status->box(FL_UP_BOX);
      m_connection_status->color((Fl_Color)1);
      m_connection_status->align(Fl_Align(FL_ALIGN_LEFT));
    } // Fl_Box* m_connection_status
    { m_rate = new Fl_Output(298, 968, 50, 25, "Rate:");
    } // Fl_Output* m_rate
    { m_interrogation_progress = new Fl_Progress(390, 969, 135, 21, "Interrogation");
    } // Fl_Progress* m_interrogation_progress
    { m_table_editor_box = new Fl_Group(455, 35, 545, 400);
      m_table_editor_box->box(FL_DOWN_BOX);
      { m_table_title = new Fl_Input(496, 45, 119, 30, "Title");
      } // Fl_Input* m_table_title
      { m_table_editor = new TableEditor(460, 75, 530, 355);
        m_table_editor->box(FL_THIN_DOWN_FRAME);
        m_table_editor->color(FL_BACKGROUND_COLOR);
        m_table_editor->selection_color(FL_BACKGROUND_COLOR);
        m_table_editor->labeltype(FL_NORMAL_LABEL);
        m_table_editor->labelfont(0);
        m_table_editor->labelsize(14);
        m_table_editor->labelcolor(FL_FOREGROUND_COLOR);
        m_table_editor->align(Fl_Align(FL_ALIGN_TOP));
        m_table_editor->when(FL_WHEN_RELEASE);
        m_table_editor->end();
        Fl_Group::current()->resizable(m_table_editor);
      } // TableEditor* m_table_editor
      { m_table_rows = new Fl_Spinner(660, 45, 40, 25, "Rows");
      } // Fl_Spinner* m_table_rows
      { m_table_cols = new Fl_Spinner(775, 45, 40, 25, "Columns");
      } // Fl_Spinner* m_table_cols
      { m_tracing = new Fl_Check_Button(930, 55, 70, 15, "Tracing");
        m_tracing->down_box(FL_DOWN_BOX);
      } // Fl_Check_Button* m_tracing
      m_table_editor_box->end();
      Fl_Group::current()->resizable(m_table_editor_box);
    } // Fl_Group* m_table_editor_box
    { m_sensor_editor_box = new Fl_Group(455, 35, 545, 400);
      m_sensor_editor_box->box(FL_DOWN_BOX);
      m_sensor_editor_box->hide();
      { Fl_Group* o = new Fl_Group(490, 60, 155, 90, "Input");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_source = new Fl_Choice(560, 65, 72, 15, "Source");
          m_sensor_source->down_box(FL_BORDER_BOX);
        } // Fl_Choice* m_sensor_source
        { m_sensor_method = new Fl_Choice(560, 95, 72, 15, "Method");
          m_sensor_method->down_box(FL_BORDER_BOX);
        } // Fl_Choice* m_sensor_method
        { m_sensor_pin = new Fl_Input(560, 120, 70, 20, "Pin");
        } // Fl_Input* m_sensor_pin
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(795, 60, 155, 65, "Linear Mapping");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_range_min = new Fl_Input(885, 65, 50, 25, "Minimum");
        } // Fl_Input* m_sensor_range_min
        { m_sensor_range_max = new Fl_Input(885, 90, 50, 25, "Maximum");
        } // Fl_Input* m_sensor_range_max
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(795, 170, 155, 110, "Thermistor");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_therm_bias = new Fl_Input(865, 175, 55, 25, "Bias");
        } // Fl_Input* m_sensor_therm_bias
        { m_sensor_therm_A = new Fl_Input(865, 200, 55, 25, "A");
        } // Fl_Input* m_sensor_therm_A
        { m_sensor_therm_B = new Fl_Input(865, 225, 55, 25, "B");
        } // Fl_Input* m_sensor_therm_B
        { m_sensor_therm_C = new Fl_Input(865, 250, 55, 25, "C");
        } // Fl_Input* m_sensor_therm_C
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(755, 315, 195, 95, "Windowing");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_window_width = new Fl_Input(905, 325, 35, 25, "Capture Width");
        } // Fl_Input* m_sensor_window_width
        { m_sensor_window_total = new Fl_Input(905, 350, 35, 25, "Total Width");
        } // Fl_Input* m_sensor_window_total
        { m_sensor_window_offset = new Fl_Input(905, 375, 35, 25, "Window Start Offset");
        } // Fl_Input* m_sensor_window_offset
        o->end();
      } // Fl_Group* o
      { Fl_Group* o = new Fl_Group(490, 240, 155, 95, "Fault");
        o->box(FL_THIN_DOWN_BOX);
        { m_sensor_fault_min = new Fl_Input(575, 250, 50, 25, "Minimum");
        } // Fl_Input* m_sensor_fault_min
        { m_sensor_fault_max = new Fl_Input(575, 275, 50, 25, "Maximum");
        } // Fl_Input* m_sensor_fault_max
        { m_sensor_fault_value = new Fl_Input(575, 300, 50, 25, "Value");
        } // Fl_Input* m_sensor_fault_value
        o->end();
      } // Fl_Group* o
      m_sensor_editor_box->end();
    } // Fl_Group* m_sensor_editor_box
    { m_save = new Fl_Button(550, 445, 70, 20, "Save");
    } // Fl_Button* m_save
    { m_reset = new Fl_Button(630, 445, 70, 20, "Reset");
    } // Fl_Button* m_reset
    { m_bar = new Fl_Menu_Bar(0, 0, 1050, 20);
      m_bar->menu(menu_m_bar);
    } // Fl_Menu_Bar* m_bar
    { m_autosave = new Fl_Check_Button(460, 450, 70, 15, "Autosave");
      m_autosave->down_box(FL_DOWN_BOX);
      m_autosave->value(1);
    } // Fl_Check_Button* m_autosave
    m_main_window->set_non_modal();
    m_main_window->end();
  } // Fl_Double_Window* m_main_window
  m_main_window->show();
}
