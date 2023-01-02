#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

#include <FL/fl_draw.H>

#include "LogView.h"

LogView::LogView(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {
  config.insert(std::make_pair("rpm", SeriesConfig{0, 6000, FL_RED}));
  series.insert(std::make_pair("rpm", std::vector<PointGroup>{}));

  config.insert(std::make_pair("sensor.map", SeriesConfig{0, 250, FL_YELLOW}));
  series.insert(std::make_pair("sensor.map", std::vector<PointGroup>{}));

  config.insert(std::make_pair("sensor.ego", SeriesConfig{0.7, 1.0, FL_GREEN}));
  series.insert(std::make_pair("sensor.ego", std::vector<PointGroup>{}));
}

struct range {
  uint64_t start_ns;
  uint64_t stop_ns;
};

void LogView::update_time_range(std::chrono::system_clock::time_point start,
                                std::chrono::system_clock::time_point stop) {
  start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                 start.time_since_epoch())
                 .count();
  stop_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                stop.time_since_epoch())
                .count();
  update_cache_time_range();
  recompute_pointgroups(0, w() - 1);
  redraw();
}

void LogView::update() {
  update_cache_time_range();
  recompute_pointgroups(0, w() - 1);
  redraw();
}

void LogView::update_cache_time_range() {

  std::vector<std::string> keys;
  for (auto i : config) {
    keys.push_back(i.first);
  }

  auto new_start =
      std::chrono::system_clock::time_point{std::chrono::nanoseconds{start_ns}};
  auto new_stop =
      std::chrono::system_clock::time_point{std::chrono::nanoseconds{stop_ns}};

  /* Is new start before potentially cached start? Determine a range to fetch
   * and fetch it (either newstart ->cachedstart or newstart -> newend. */

  auto log_locked = log.lock();
  if (!log_locked) {
    return;
  }
  if (!cache.points.size()) {
    cache = log_locked->GetRange(keys, new_start, new_stop);
  } else {
    auto cached_start = cache.points[0].time;
    if (new_start < cached_start) {
      auto updates = log_locked->GetRange(keys, new_start, cached_start);
      cache.points.insert(cache.points.begin(), updates.points.begin(),
                          updates.points.end());
    }
    auto cached_stop = cache.points[cache.points.size() - 1].time;
    if (new_stop > cached_stop) {
      auto updates = log_locked->GetRange(keys, cached_stop, new_stop);
      cache.points.insert(cache.points.end(), updates.points.begin(),
                          updates.points.end());
    }
  }

  if (!cache.points.size()) {
    return;
  }

  for (auto i = cache.points.begin(); i != cache.points.end(); i++) {
    if (i->time >= new_start) {
      cache.points.erase(cache.points.begin(), i);
      break;
    }
  }
  for (auto i = cache.points.end() - 1; i != cache.points.begin(); i--) {
    if (i->time < new_stop) {
      cache.points.erase(i + 1, cache.points.end());
      break;
    }
  }
}

/* Recompute pointgroups for pixels x1 through x1 inclusive */
void LogView::recompute_pointgroups(int x1, int x2) {
  if (stop_ns == start_ns) {
    return;
  }

  /* Ensure pointgroups exist for all pixels */
  for (auto i : config) {
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

  auto start = std::lower_bound(
      cache.points.begin(), cache.points.end(), pixel_ranges[0].start_ns,
      [](const auto &point, uint64_t time) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   point.time.time_since_epoch())
                   .count() < time;
      });
  auto stop = std::upper_bound(
      start, cache.points.end(), pixel_ranges[pixel_ranges.size() - 1].stop_ns,
      [](uint64_t time, const auto &point) {
        return time < std::chrono::duration_cast<std::chrono::nanoseconds>(
                          point.time.time_since_epoch())
                          .count();
      });

  int pixel = x1;
  for (auto i = start; i < stop; i++) {
    auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(
                 i->time.time_since_epoch())
                 .count();
    while ((t >= pixel_ranges[pixel - x1].stop_ns) && pixel < w()) {
      pixel++;
    }
    if (pixel == w()) {
      break;
    }

    for (int k = 0; k < keymap.size(); k++) {
      auto &s = keymap[k]->at(pixel);
      viaems::FeedValue fv = i->values[k];
      float v;
      if (std::holds_alternative<uint32_t>(fv)) {
        v = std::get<uint32_t>(fv);
      } else {
        v = std::get<float>(fv);
      }
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
  }
}

void LogView::draw() {
  draw_box();
  fl_push_clip(x(), y(), w(), h());
  int count = 0;
  int total_y_space = (config.size() + 2) * 15;
  int hover_text_x_offset = (mouse_x > (x() + 0.75 * w())) ? -150 : 5;
  int hover_text_y_offset =
      (mouse_y - y() + total_y_space > h()) ? -total_y_space : 20;
  for (const auto &element : config) {
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
  sprintf(ms, ".%ld", point_ms);

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
    mouse_press_x = Fl::event_x();
    mouse_press_y = Fl::event_y();
    return 1;
  }
  if (ev == FL_MOVE) {
    mouse_x = Fl::event_x();
    mouse_y = Fl::event_y();
    redraw();
  }
  if (ev == FL_DRAG) {
    int diff_x = mouse_press_x - Fl::event_x();
    auto ns_per_pixel = (stop_ns - start_ns) / w();
    uint64_t shift_ns = diff_x * ns_per_pixel;
    auto amt =
        std::chrono::system_clock::duration{std::chrono::nanoseconds{shift_ns}};
    shift(amt);
    mouse_press_x = Fl::event_x();
    mouse_press_y = Fl::event_y();
    do_callback();
  }
  if (ev == FL_MOUSEWHEEL) {
    float amt = Fl::event_dy() * 0.2; /* 20% up or down */
    float centerpoint = (Fl::event_x() - x()) / (float)w();
    zoom(amt, centerpoint);
    do_callback();
    return 1;
  }

  return 0;
}

void LogView::shift_pointgroups(int amt) {
  /* For a given shift, preserve the pointgroups that are unaffected */
  for (auto i : config) {
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
  int shifted_pixels = (ns_per_pixel == 0) ? 0 : (shift_ns / ns_per_pixel);

  if (shifted_pixels != 0) {
    update_cache_time_range();
    shift_pointgroups(shifted_pixels);
    if (shift_ns > 0) {
      recompute_pointgroups(w() - 1 - shifted_pixels, w() - 1);
    } else {
      recompute_pointgroups(0, -shifted_pixels - 1);
    }
    redraw();
  }
}

/* Zoom in by amt (0.0 means no zoom, 0.2 increases 20%, with the zoom
 * centered at centerpoint (0.0 - 1.0, 0.0 represents far left)
 */
void LogView::zoom(double amt, double centerpoint) {
  int64_t delta = (int64_t)(stop_ns - start_ns) * amt;

  start_ns -= delta * centerpoint;
  stop_ns += delta * (1.0 - centerpoint);
  update_cache_time_range();
  recompute_pointgroups(0, w() - 1);
  redraw();
}

void LogView::resize(int X, int Y, int W, int H) {
  Fl_Box::resize(X, Y, W, H);
  recompute_pointgroups(0, w() - 1);
}

void LogView::SetLog(std::weak_ptr<Log> log) {
  this->log = log;
  this->cache = viaems::LogChunk{};
};
