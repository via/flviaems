#ifndef MainWindow_h
#define MainWindow_h

#include "MainWindowUI.h"
#include "viaems.h"

class MainWindow : public MainWindowUI
{
  viaems::Model *m_model;

  void update_config_structure(viaems::StructureNode top);
  void update_config_value(viaems::StructurePath &path, viaems::ConfigValue &value);
public:
  MainWindow();
  void feed_update(viaems::FeedUpdate const &);
  void update_connection_status(bool status);
  void update_feed_hz(int hz);
  void update_model(viaems::Model *model);
  void update_interrogation(bool in_progress, int value, int max);
};

#endif
