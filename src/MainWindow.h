#ifndef MainWindow_h
#define MainWindow_h

#include "viaems_protocol.h"
#include "MainWindowUI.h"

class MainWindow : public MainWindowUI {
  public:
    MainWindow();
    void feed_update(viaems::FeedUpdate const&);
    void update_connection_status(bool status);
};

#endif
