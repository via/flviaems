#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <numeric>
#include <unistd.h>

#include "MainWindow.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "viaems.h"

static viaems::Protocol connector{ std::cout };
MainWindow ui;
viaems::Model model(connector);

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

std::shared_ptr<viaems::Request> ping_req;

static void failed_ping_callback(void *ptr) {
  if (ping_req) {
    connector.Cancel(ping_req);
    ping_req.reset();
  }
  std::cerr << "failed ping" << std::endl;
  ui.update_connection_status(false);
}

static void interrogate_update(viaems::InterrogationState s) {
  ui.update_interrogation(s.in_progress, s.complete_nodes, s.total_nodes);
  if (!s.in_progress) {
    ui.update_model(std::shared_ptr<viaems::Model>(&model));
  }
}

static void failed_structure_callback(void *ptr) {
  auto is = model.interrogation_status();
  if (is.in_progress) {
    std::cerr << "redoing structure" << is.complete_nodes << " " << is.total_nodes << std::endl;
    model.interrogate(interrogate_update);
    Fl::add_timeout(5, failed_structure_callback);
  }
}

static void ping_callback(void *ptr) {
  static bool first_pong = true;
  Fl::remove_timeout(failed_ping_callback);
  ui.update_connection_status(true);

  if (first_pong) {
    first_pong = false;
    model.interrogate(interrogate_update);
    Fl::add_timeout(5, failed_structure_callback);
  }
}

static void pinger(void *ptr) {
  
  Fl::add_timeout(0.5, failed_ping_callback);
  ping_req = connector.Ping(ping_callback, 0);
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
