#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <numeric>
#include <unistd.h>

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "viaems_protocol.h"

static viaems::ViaemsProtocol connector{ std::cout };
MainWindow ui;

static void feed_refresh_handler(void *_ptr) {
  auto updates = connector.FeedUpdates();
  static std::deque<int> rates;

  /* Keep average over 1 second */
  rates.push_back(updates.size());
  if (rates.size() > 20) {
    rates.erase(rates.begin());
  }

  if (updates.size() > 0) {
    ui.feed_update(updates.at(0));
    ui.update_feed_hz(std::accumulate(rates.begin(), rates.end(), 0));
  }
  Fl::repeat_timeout(0.05, feed_refresh_handler);
}

static void stdin_ready_cb(int fd, void *ptr) {
  char buf[4096];
  auto res = read(STDIN_FILENO, &buf[0], sizeof(buf));
  if (res < 0) {
    return;
  }
  connector.NewData(std::string(buf, res));
}

static void set_stdin_nonblock() {
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK | fcntl(STDIN_FILENO, F_GETFL, 0));
}

static void debug_thing(viaems::StructureNode c) {
  if (c.is_leaf()) {
    std::cerr << " (leaf@";
    for (auto p : c.leaf()->path) {
      std::visit([](const auto&z) {std::cerr << z;}, p);
    std::cerr << " ";
    }
    std::cerr << ") ";
  } else if (c.is_map()) {
    std::cerr << "{";
    for (auto entry : *c.map()) {
      std::cerr << "'" << entry.first << "': ";
      debug_thing(entry.second);
      std::cerr << ", ";
    }
    std::cerr << "} ";
  } else if (c.is_list()) {
    std::cerr << "[";
    for (auto entry : *c.list()) {
      debug_thing(entry);
      std::cerr << ", ";
    }
    std::cerr << "] ";
  }
}

static void structure_callback(viaems::StructureNode top, void *ptr) {
  debug_thing(top);
  ui.update_config_structure(top);
}

static void failed_ping_callback(void *ptr) {
  std::cerr << "failed ping" << std::endl;
  ui.update_connection_status(false);
}

static void ping_callback(void *ptr) {
  static bool first_pong = true;
  Fl::remove_timeout(failed_ping_callback);
  ui.update_connection_status(true);

  if (first_pong) {
    first_pong = false;
    connector.Structure(structure_callback, 0);
  }
}

static void pinger(void *ptr) {
  
  Fl::add_timeout(0.5, failed_ping_callback);
  connector.Ping(ping_callback, 0);
  Fl::repeat_timeout(1, pinger);
}

int main() {

  set_stdin_nonblock();
  Fl::add_fd(0, FL_READ, stdin_ready_cb);
  Fl::add_timeout(0.05, feed_refresh_handler);

  Fl::add_timeout(1, pinger);

  Fl::run();
  return 0;
}
