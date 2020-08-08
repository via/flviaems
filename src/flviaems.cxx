#include <cstdio>
#include <iostream>
#include <memory>
#include <fcntl.h>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include "MainWindow.h"

#include "viaems_protocol.h"

static viaems::ViaemsProtocol connector{std::shared_ptr<std::ostream>(&std::cout)};
MainWindow ui;

static void feed_refresh_handler(void *_ptr) {
  auto updates = connector.FeedUpdates();
  if (updates.size() > 0) {
    ui.feed_update(updates.at(0));
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

static void failed_ping_callback(void *ptr) {
  ui.update_connection_status(false);
}

static void ping_callback(void *ptr) {
  Fl::remove_timeout(failed_ping_callback);
  ui.update_connection_status(true);
}

static void pinger(void *ptr) {
  connector.Ping(ping_callback, 0);
  Fl::repeat_timeout(1, pinger);
  Fl::add_timeout(0.5, failed_ping_callback);
}

static void structure_callback(std::unique_ptr<viaems::ConfigNode> top, void *ptr) {
  std::cerr << "Got Structure!" << std::endl;
}

int main() {

  set_stdin_nonblock();
  Fl::add_fd(0, FL_READ, stdin_ready_cb);
  Fl::add_timeout(0.05, feed_refresh_handler);

  Fl::add_timeout(1, pinger);
  connector.Structure(structure_callback, 0);

  Fl::run();
  return 0;

}
