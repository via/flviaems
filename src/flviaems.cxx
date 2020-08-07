#include <cstdio>
#include <iostream>
#include <memory>
#include <fcntl.h>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include "MainWindow.h"

#include "viaems_protocol.h"

static viaems::ViaemsProtocol connector{};
MainWindow ui;

static void feed_refresh_handler(void *_ptr) {
  auto updates = connector.FeedUpdates();
  std::cout << "num updates: " << updates->size() << std::endl;
  if (updates->size() > 0) {
    ui.feed_update(updates->at(0));
  }
  Fl::repeat_timeout(0.05, feed_refresh_handler);
}

static void stdin_ready_cb(int fd, void *ptr) {
  char buf[4096];
  auto res = read(STDIN_FILENO, &buf[0], sizeof(buf));
  connector.NewData(std::string(buf, res));
}


static void set_stdin_nonblock() {
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK | fcntl(STDIN_FILENO, F_GETFL, 0));
}

int main() {

  set_stdin_nonblock();
  Fl::add_fd(0, FL_READ, stdin_ready_cb);
  Fl::add_timeout(0.05, feed_refresh_handler);


  Fl::run();
  return 0;

}
