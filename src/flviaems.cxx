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

static void set_stdin_nonblock(int fd) {
  fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL, 0));
}

class FLViaems {
  viaems::Protocol connector;
  int read_fd;
  MainWindow ui;
  viaems::Model model;
  std::shared_ptr<viaems::Request> ping_req;

  static void feed_refresh_handler(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    auto updates = v->connector.FeedUpdates();
    static std::deque<int> rates;

    /* Keep average over 1 second */
    rates.push_back(updates.size());
    if (rates.size() > 20) {
      rates.erase(rates.begin());
    }

    if (updates.size() > 0) {
      v->ui.feed_update(updates);
      v->ui.update_feed_hz(std::accumulate(rates.begin(), rates.end(), 0));
    }
    Fl::repeat_timeout(0.05, v->feed_refresh_handler, v);
  }

  static void stdin_ready_cb(int fd, void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    char buf[4096];
    auto res = read(v->read_fd, &buf[0], sizeof(buf));
    if (res < 0) {
      return;
    }
    v->connector.NewData(std::string(buf, res));
  }

  static void failed_ping_callback(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    if (v->ping_req) {
      v->connector.Cancel(v->ping_req);
      v->ping_req.reset();
    }
    std::cerr << "failed ping" << std::endl;
    v->ui.update_connection_status(false);
  }

  static void model_changed(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    auto s = v->model.interrogation_status();
    v->ui.update_interrogation(s.in_progress, s.complete_nodes, s.total_nodes);
    if (!s.in_progress) {
      v->ui.update_model(&v->model);
    }
  }

  static void failed_structure_callback(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    auto is = v->model.interrogation_status();
    if (is.in_progress) {
      std::cerr << "redoing structure" << is.complete_nodes << " "
                << is.total_nodes << std::endl;
      v->model.interrogate();
      Fl::add_timeout(5, v->failed_structure_callback, v);
    }
  }

  static void ping_callback(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    static bool first_pong = true;
    Fl::remove_timeout(v->failed_ping_callback);
    v->ui.update_connection_status(true);

    if (first_pong) {
      first_pong = false;
      v->model.interrogate();
      Fl::add_timeout(5, v->failed_structure_callback, v);
    }
  }

  static void pinger(void *ptr) {
    auto v = static_cast<FLViaems *>(ptr);

    Fl::add_timeout(0.5, v->failed_ping_callback, v);
    v->ping_req = v->connector.Ping(v->ping_callback, v);
    Fl::repeat_timeout(1, v->pinger, v);
  }

public:
  FLViaems(std::ostream &o, int infd)
      : connector{o}, model{connector, this->model_changed, this} {
    read_fd = infd;
    set_stdin_nonblock(read_fd);
    Fl::add_fd(0, FL_READ, stdin_ready_cb, this);
    Fl::add_timeout(0.05, feed_refresh_handler, this);
    Fl::add_timeout(1, pinger, this);
  };
};

int main() {
  FLViaems controller{std::cout, STDIN_FILENO};
  Fl::run();
  return 0;
}
