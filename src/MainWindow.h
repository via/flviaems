#ifndef MainWindow_h
#define MainWindow_h

#include "Log.h"
#include "MainWindowUI.h"
#include "viaems.h"

class MainWindow : public MainWindowUI {
  viaems::Model *m_model;
  viaems::StructurePath detail_path;
  std::optional<std::shared_ptr<Log>> log;

  std::vector<Fl_Menu_Item> prev_config_menu_items;
  std::function<void(viaems::Configuration)> load_config_callback;

  void update_config_structure(viaems::StructureNode top);
  void update_table_editor(viaems::TableValue t);

  static void select_table(Fl_Widget *w, void *p);
  static void structure_value_update_callback(Fl_Widget *w, void *p);
  static void table_value_changed_callback(Fl_Widget *w, void *ptr);
  static void select_prev_config_callback(Fl_Widget *w, void *v);

  void add_config_structure_entry(Fl_Tree_Item *, viaems::StructureNode);

public:
  MainWindow();
  void feed_update(std::map<std::string, viaems::FeedValue> status);
  void update_connection_status(bool status);
  void update_feed_hz(int hz);
  void update_model(viaems::Model *model);
  void update_interrogation(bool in_progress, int value, int max);
  void update_config_value(viaems::StructurePath path,
                           viaems::ConfigValue value);
  void update_log(std::optional<std::shared_ptr<Log>>);

  void set_load_config_callback(std::function<void(viaems::Configuration)> cb) {
    load_config_callback = cb;
  }
};

#endif
