#include <chrono>

#include <FL/fl_draw.H>

#include "LogView.h"

#include <iostream>

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

void LogView::update_time_range(
    std::chrono::system_clock::time_point new_start,
    std::chrono::system_clock::time_point new_stop) {

  start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                 new_start.time_since_epoch())
                 .count();
  stop_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                new_stop.time_since_epoch())
                .count();

  std::vector<std::string> keys;
  for (auto i : config) {
    keys.push_back(i.first);
  }

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
  recompute();
  redraw();
}

void LogView::recompute() {
  if (stop_ns == start_ns) {
    return;
  }

  for (auto i : config) {
    series[i.first].clear();
    for (auto k = 0; k < w(); k++) {
      PointGroup pg{};
      series[i.first].push_back(pg);
    }
  }

  std::vector<std::vector<PointGroup> *> keymap;
  for (auto i : config) {
    auto k = i.first;
    keymap.push_back(&series[k]);
  }

  std::vector<range> pixel_ranges;
  auto ns_per_pixel = (stop_ns - start_ns) / w();
  for (auto k = 0; k < w(); k++) {
    uint64_t start = start_ns + (k * ns_per_pixel);
    uint64_t stop = start_ns + ((k + 1) * ns_per_pixel);
    pixel_ranges.push_back(range{.start_ns = start, .stop_ns = stop});
  }

  int pixel = 0;
  for (auto i = cache.points.begin(); i != cache.points.end(); i++) {
    auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(
                 i->time.time_since_epoch())
                 .count();
    if (t < pixel_ranges[pixel].start_ns) {
      continue;
    }
    while ((t >= pixel_ranges[pixel].stop_ns) && pixel < w()) {
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
  for (const auto &element : config) {
    auto name = element.first;
    auto conf = element.second;
    int cx = 0;
    int last_x = 0;
    int last_y = -1;

    fl_color(conf.color);
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
          fl_line(x() + last_x, y() + h() - last_y, x() + cx - 1,
                  y() + h() - cyfirst);
        }

        last_x = cx;
        last_y = cylast;
      }

      cx += 1;
    }

    fl_color(FL_WHITE);
    if ((mouse_x > x()) && (mouse_x < x() + w()) &&
        (series[element.first].size() == w())) {
      char txt[32];
      sprintf(txt, "%s  %.2f", name.c_str(),
              series[element.first][mouse_x - x()].max);
      fl_draw(txt, mouse_x + 5, mouse_y + (count + 1) * 15);
    }
    count += 1;
  }

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
    double amt = Fl::event_dy() * 0.2; /* 20% up or down */
    zoom(amt);
    do_callback();
    return 1;
  }

  return 0;
}

void LogView::shift(std::chrono::system_clock::duration amt) {
  uint64_t shift_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          amt).count(); 
  auto new_start = std::chrono::system_clock::time_point{
    std::chrono::nanoseconds{start_ns + shift_ns}};
  auto new_stop = std::chrono::system_clock::time_point{
    std::chrono::nanoseconds{stop_ns + shift_ns}};

  update_time_range(new_start, new_stop);
}

void LogView::zoom(double amt) {
  int64_t delta = (stop_ns - start_ns) / 2 * amt;

  auto new_start = std::chrono::system_clock::time_point{
    std::chrono::nanoseconds{start_ns - delta}};
  auto new_stop = std::chrono::system_clock::time_point{
    std::chrono::nanoseconds{stop_ns + delta}};

  update_time_range(new_start, new_stop);
}

void LogView::resize(int X, int Y, int W, int H) {
  Fl_Box::resize(X, Y, W, H);
  recompute();
}

void LogView::SetLog(std::weak_ptr<Log> log) {
  this->log = log;
  this->cache = viaems::LogChunk{};
};
