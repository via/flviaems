#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>

#include "LogView.h"
#include "LogViewEditor.h"

LogView::LogView(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {
}

struct range {
  uint64_t start_ns;
  uint64_t stop_ns;
};

void LogView::update_time_range(std::chrono::system_clock::time_point start,
                                std::chrono::system_clock::time_point stop) {
  auto old_start_ns = start_ns;
  auto old_stop_ns = stop_ns;
  auto new_start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          start.time_since_epoch())
                          .count();
  auto new_stop_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         stop.time_since_epoch())
                         .count();

  if (new_stop_ns - new_start_ns == old_stop_ns - old_start_ns) {
    auto amt = std::chrono::nanoseconds{new_start_ns - old_start_ns};
    shift(amt);
  } else {
    start_ns = new_start_ns;
    stop_ns = new_stop_ns;
    recompute_pointgroups(0, w() - 1);
  }
  redraw();
}

void LogView::update() {
  recompute_pointgroups(0, w() - 1);
  redraw();
}

/* Recompute pointgroups for pixels x1 through x1 inclusive */
void LogView::recompute_pointgroups(int x1, int x2) {
  if (stop_ns == start_ns) {
    return;
  }

  /* Ensure pointgroups exist for all pixels */
  std::vector<std::string> keys;
  for (auto i : config) {
    if (i.second.enabled) {
      keys.push_back(i.first);
    }
    while (series[i.first].size() <= x2) {
      PointGroup pg{};
      series[i.first].push_back(pg);
    }
    /* Clear out the range we're touching */
    for (int j = x1; j <= x2; j++) {
      series[i.first][j] = PointGroup{};
    }
  }

  std::vector<std::vector<PointGroup> *> keymap;
  for (auto i : config) {
    if (!i.second.enabled) {
      continue;
    }
    auto k = i.first;
    keymap.push_back(&series[k]);
  }

  std::vector<range> pixel_ranges;
  auto ns_per_pixel = (stop_ns - start_ns) / w();
  for (auto k = x1; k <= x2; k++) {
    uint64_t start = start_ns + (k * ns_per_pixel);
    uint64_t stop = start_ns + ((k + 1) * ns_per_pixel);
    pixel_ranges.push_back(range{.start_ns = start, .stop_ns = stop});
  }

  auto log_locked = log.lock();
  if (!log_locked) {
    return;
  }
  auto fetch_start = std::chrono::system_clock::time_point{
      std::chrono::nanoseconds{pixel_ranges.front().start_ns}};
  auto fetch_stop = std::chrono::system_clock::time_point{
      std::chrono::nanoseconds{pixel_ranges.back().stop_ns}};
  auto before = std::chrono::system_clock::now();
  auto maybe_points = log_locked->GetLogRange(keys, fetch_start, fetch_stop);
  if (!maybe_points) {
    return;
  };

  auto points = std::move(maybe_points.value());
  auto after = std::chrono::system_clock::now();

  int pixel = x1;
  while (points.next()) {
    auto t = points.row_time();
    while ((t >= pixel_ranges[pixel - x1].stop_ns) && pixel < w()) {
      pixel++;
    }
    if (pixel == w()) {
      break;
    }

    for (int k = 0; k < keymap.size(); k++) {
      auto &s = keymap[k]->at(pixel);
      auto v = points.row_value(k);
      if (!s.set) {
        s.first = v;
        s.min = v;
        s.max = v;
      }
      if (v < s.min) {
        s.min = v;
      }
      if (v > s.max) {
        s.max = v;
      }
      s.last = v;
      s.set = true;
    }
  };
  auto after_more = std::chrono::system_clock::now();

  auto fetch_ns = std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count();
  auto calc_ns = std::chrono::duration_cast<std::chrono::milliseconds>(after_more - after).count();
  std::cerr << "fetch: " << fetch_ns << "  calc: " << calc_ns << std::endl;
}

void LogView::draw() {
  draw_box();
  fl_push_clip(x(), y(), w(), h());

  int count = 0;
  auto enabled_count = std::count_if(config.begin(), config.end(),
                                     [](auto &x) { return x.second.enabled; });
  int total_y_space = (enabled_count + 2) * 15;
  int hover_text_x_offset = (mouse_x > (x() + 0.75 * w())) ? -150 : 5;
  int hover_text_y_offset =
      (mouse_y - y() + total_y_space > h()) ? -total_y_space : 20;

  /* Draw selection box */
  if (selecting) {
    fl_draw_box(FL_FLAT_BOX, selection_x1, y(), selection_x2 - selection_x1, h(), FL_BLUE);
  }

  for (const auto &element : config) {
    if (!element.second.enabled) {
      continue;
    }
    auto name = element.first;
    auto conf = element.second;
    int cx = 0;
    int last_x = 0;
    int last_y = -1;

    fl_color(conf.color);
    PointGroup last_valid_before_x;
    for (const auto pointgroup : series[element.first]) {

      if (pointgroup.set) {
        int cymin =
            h() * ((pointgroup.min - conf.min_y) / (conf.max_y - conf.min_y));
        int cymax =
            h() * ((pointgroup.max - conf.min_y) / (conf.max_y - conf.min_y));
        int cyfirst =
            h() * ((pointgroup.first - conf.min_y) / (conf.max_y - conf.min_y));
        int cylast =
            h() * ((pointgroup.last - conf.min_y) / (conf.max_y - conf.min_y));

        fl_line(x() + cx, y() + h() - cymin, x() + cx, y() + h() - cymax);
        if (last_y >= 0) {
          fl_line(x() + last_x, y() + h() - last_y, x() + cx,
                  y() + h() - cyfirst);
        }
        /* Save this pointgroup to use with the mouse cursor */
        if (cx < mouse_x - x()) {
          last_valid_before_x = pointgroup;
        }

        last_x = cx;
        last_y = cylast;
      }

      cx += 1;
    }

    fl_color(conf.color);
    if ((mouse_x > x()) && (mouse_x < x() + w())) {
      char txt[32];
      sprintf(txt, "%s  %.3f", name.c_str(),
              last_valid_before_x.max);
      fl_draw(txt, mouse_x + hover_text_x_offset,
              mouse_y + hover_text_y_offset + (count + 2) * 15);
    }
    count += 1;
  }

  fl_color(FL_WHITE);
  char buf[64];
  int mw, mh;

  /* Printing milliseconds with strftime can't be done, so seperately extract
   * the ms components and append it to the strftime output */
  auto point_time =
      std::chrono::system_clock::time_point{std::chrono::nanoseconds{
          start_ns + (stop_ns - start_ns) * (mouse_x - x()) / w()}};
  auto point_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      point_time.time_since_epoch())
                      .count() -
                  (std::chrono::duration_cast<std::chrono::seconds>(
                       point_time.time_since_epoch())
                       .count() *
                   1000);
  char ms[32];
  sprintf(ms, ".%03ld", point_ms);

  auto point_ctime = std::chrono::system_clock::to_time_t(point_time);
  std::strftime(buf, 32, "%F %T", std::localtime(&point_ctime));
  strcat(buf, ms);
  fl_draw(buf, mouse_x + hover_text_x_offset, mouse_y + hover_text_y_offset);

  /* Print the logview start time in the bottom left corner */
  auto start_ctime = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::time_point{
          std::chrono::nanoseconds{start_ns}});
  std::strftime(buf, 32, "%F %T", std::localtime(&start_ctime));
  fl_measure(buf, mw, mh);
  fl_draw(buf, x() + 5, y() + h() - mh - 5);

  /* Print the logview stop time in the bottom right corner */
  auto stop_ctime = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::time_point{std::chrono::nanoseconds{stop_ns}});
  std::strftime(buf, 32, "%F %T", std::localtime(&stop_ctime));
  fl_measure(buf, mw, mh);
  fl_draw(buf, x() + w() - mw - 5, y() + h() - mh - 5);

  fl_color(FL_LIGHT1);
  fl_line(mouse_x, y(), mouse_x, y() + h());
  fl_pop_clip();
}

int LogView::handle(int ev) {
  if (auto ret = Fl_Box::handle(ev)) {
    return ret;
  }
  if (ev == FL_PUSH) {
    if (Fl::event_button() == FL_RIGHT_MOUSE) {
      auto *m =
          context_menu.data()->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);
      if (m) {
        m->do_callback(this, m->user_data());
      }
    } else {
      mouse_press_x = Fl::event_x();
      mouse_press_y = Fl::event_y();
      selecting = false;
    }
    return 1;
  }
  if (ev == FL_MOVE) {
    mouse_x = Fl::event_x();
    mouse_y = Fl::event_y();
    redraw();
  }
  if (ev == FL_DRAG && Fl::event_button() == FL_LEFT_MOUSE) {
    if (Fl::event_clicks() == 0) {
      /* Single left click -- drag */
      int diff_x = mouse_press_x - Fl::event_x();
      auto ns_per_pixel = (stop_ns - start_ns) / w();
      int64_t shift_ns = diff_x * ns_per_pixel;
      auto amt =
        std::chrono::system_clock::duration{std::chrono::nanoseconds{shift_ns}};
      shift(amt);
      mouse_press_x = Fl::event_x();
      mouse_press_y = Fl::event_y();
      selecting = false;
      do_callback();
    } else if (Fl::event_clicks() == 1) {
      /* Double left click means initiate selection */
      mouse_press_x = Fl::event_x();
      mouse_press_y = Fl::event_y();
      if (mouse_x < mouse_press_x) {
        selection_x1 = mouse_x;
        selection_x2 = mouse_press_x;
      } else {
        selection_x1 = mouse_press_x;
        selection_x2 = mouse_x;
      }
      selecting = true;
      redraw();
    }

  }
  if (ev == FL_MOUSEWHEEL) {
    float amt = Fl::event_dy() * 0.2; /* 20% up or down */
    float centerpoint = (Fl::event_x() - x()) / (float)w();
    selecting = false;
    zoom(amt, centerpoint);
    do_callback();
    return 1;
  }

  return 0;
}

void LogView::shift_pointgroups(int amt) {
  /* For a given shift, preserve the pointgroups that are unaffected */
  for (auto i : config) {
    if (!i.second.enabled) {
      continue;
    }
    if (amt > 0) {
      /* We're moving points to the left, start at the beginning */
      for (int pos = amt; pos < series[i.first].size(); pos++) {
        series[i.first][pos - amt] = series[i.first][pos];
      }
    } else {
      /* We're moving existing points to the right, start at the end and count
       * backwards */
      int namt = -amt;
      for (int pos = series[i.first].size() - 1; pos >= namt; pos--) {
        series[i.first][pos] = series[i.first][pos - namt];
      }
    }
  }
}

void LogView::shift(std::chrono::system_clock::duration amt) {
  int64_t shift_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(amt).count();

  start_ns += shift_ns;
  stop_ns += shift_ns;

  int64_t ns_per_pixel = (stop_ns - start_ns) / w();
  int shifted_pixels = ns_per_pixel != 0 ? (shift_ns / ns_per_pixel) : 0;

  if (std::abs(shift_ns) >= stop_ns - start_ns) {
    recompute_pointgroups(0, w() - 1);
  } else {
    shift_pointgroups(shifted_pixels);
    if (shifted_pixels > 0) {
      recompute_pointgroups(w() - 1 - shifted_pixels, w() - 1);
    } else if (shifted_pixels < 0) {
      recompute_pointgroups(0, -shifted_pixels - 1);
    }
  }
  redraw();
}

/* Zoom in by amt (0.0 means no zoom, 0.2 increases 20%, with the zoom
 * centered at centerpoint (0.0 - 1.0, 0.0 represents far left)
 */
void LogView::zoom(double amt, double centerpoint) {
  int64_t delta = (int64_t)(stop_ns - start_ns) * amt;

  start_ns -= delta * centerpoint;
  stop_ns += delta * (1.0 - centerpoint);
  recompute_pointgroups(0, w() - 1);
  redraw();
}

void LogView::resize(int X, int Y, int W, int H) {
  Fl_Box::resize(X, Y, W, H);
  recompute_pointgroups(0, w() - 1);
}

void LogView::zoom_selection(Fl_Widget *w, void *p) {
  LogView *lv = (LogView *)w;

  uint64_t total_range = lv->stop_ns - lv->start_ns;
  double x1 = (lv->selection_x1 - lv->x()) / (double)lv->w();
  double x2 = (lv->selection_x2 - lv->x()) / (double)lv->w();
  lv->stop_ns = lv->start_ns + (x2 * total_range);
  lv->start_ns = lv->start_ns + (x1 * total_range);
  lv->selecting = false;
  lv->recompute_pointgroups(0, lv->w() - 1);
  lv->redraw();
}

void LogView::open_editor(Fl_Widget *w, void *p) {
}

void LogView::SetLog(std::weak_ptr<Log> log) {
  this->log = log;

  auto log_locked = log.lock();

  for (const auto &k : log_locked->Keys()) {
    config[k] = {0, 100, FL_WHITE, false};
  }

  /* TODO: remove when complete with dynamic setup */
  config["rpm"] = {0, 6000, FL_RED, true};
  config["sensor.map"] = {0, 250, FL_YELLOW, true};
  config["sensor.ego"] = {0.7, 1.0, FL_GREEN, true};

  context_menu.clear();
  context_menu.push_back({"Zoom to selection", 0, zoom_selection, 0, 0});
  context_menu.push_back({"Edit Viewer...", 0, open_editor, 0, 0});
  context_menu.push_back({0});
};
