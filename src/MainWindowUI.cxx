// generated by Fast Light User Interface Designer (fluid) version 1.0305

#include "MainWindowUI.h"

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
    { m_table_editor_box = new Fl_Group(455, 35, 540, 440);
      m_table_editor_box->box(FL_DOWN_BOX);
      { m_table_title = new Fl_Input(496, 45, 119, 30, "Title");
      } // Fl_Input* m_table_title
      { m_table_editor = new Fl_Table(460, 85, 530, 385);
        m_table_editor->end();
      } // Fl_Table* m_table_editor
      m_table_editor_box->end();
    } // Fl_Group* m_table_editor_box
    { m_sensor_editor_box = new Fl_Group(455, 35, 540, 440);
      m_sensor_editor_box->box(FL_DOWN_BOX);
      m_sensor_editor_box->hide();
      { m_sensor_pin = new Fl_Input(490, 60, 85, 20, "Pin");
      } // Fl_Input* m_sensor_pin
      m_sensor_editor_box->end();
    } // Fl_Group* m_sensor_editor_box
    m_main_window->set_non_modal();
    m_main_window->end();
  } // Fl_Double_Window* m_main_window
  m_main_window->show();
}
