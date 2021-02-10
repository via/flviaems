#include <chrono>

#include <FL/fl_draw.H>

#include "LogView.h"

LogView::LogView(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {
  config.insert(std::make_pair("rpm", SeriesConfig{0, 6000, FL_RED}));
  config.insert(std::make_pair("last_trigger_angle", SeriesConfig{0, 720, FL_YELLOW}));
  series.insert(std::make_pair("rpm", std::vector<PointGroup>{}));
  series.insert(std::make_pair("last_trigger_angle", std::vector<PointGroup>{}));
}

void LogView::update_time_range(std::chrono::system_clock::time_point new_start,
    std::chrono::system_clock::time_point new_stop) {

  std::vector<std::string> keys;
  for (auto i : config) {
    keys.push_back(i.first);
    series[i.first].clear();
    for (auto k = 0; k < w(); k++) {
      series[i.first].push_back({});
    }
  }

/* Is new start before potentially cached start? Determine a range to fetch and
 * fetch it (either newstart ->cachedstart or newstart -> newend. */

  if (!cache.points.size()) {
    cache = log->GetRange(keys, new_start, new_stop);
  } else {
    auto cached_start = cache.points[0].time;
    if (new_start < cached_start) {
      auto updates = log->GetRange(keys, new_start, cached_start);
      cache.points.insert(cache.points.begin(), updates.points.begin(),
      updates.points.end());
    }
    auto cached_stop = cache.points[cache.points.size() - 1].time;
    if (new_stop > cached_stop) {
      auto updates = log->GetRange(keys, cached_stop, new_stop);
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

  auto start_time_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(new_start.time_since_epoch()).count();
  auto stop_time_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(new_stop.time_since_epoch()).count();

  std::vector<std::vector<PointGroup> *> keymap;
  for (auto k : keys) {
    keymap.push_back(&series[k]);
  }
  for (auto i = cache.points.begin(); i != cache.points.end(); i++) {
    auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(i->time.time_since_epoch()).count();
    int x = w() * ((double)(t - start_time_ns) / (stop_time_ns - start_time_ns));
    if ((x < 0) || (x >= w())) {
      continue;
    }
    for (int k = 0; k < keys.size(); k++) {
      auto &s = keymap[k]->at(x);
      std::visit([&](const auto v) { 
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
        }, i->values[k]);
    }
  }
  redraw();
}


void LogView::draw() {
  draw_box();
  if (log == nullptr) {
    return;
  }
  int count = 0;
  for (const auto element : config) {
    auto name = element.first;
    auto conf = element.second;
    int cx = 0;
    int last_x = 0;
    int last_y = -1;

    fl_color(conf.color);
    for (const auto pointgroup : series[element.first]) {

      if (pointgroup.set) {
        int cymin = h() * ((pointgroup.min - conf.min_y) / (conf.max_y - conf.min_y));
        int cymax = h() * ((pointgroup.max - conf.min_y) / (conf.max_y - conf.min_y));
        int cyfirst = h() * ((pointgroup.first - conf.min_y) / (conf.max_y - conf.min_y));
        int cylast = h() * ((pointgroup.last - conf.min_y) / (conf.max_y - conf.min_y));

        fl_line(x() + cx, y() + cymin, x() + cx, y() + cymax);
        if (last_y >= 0) {
          fl_line(x() + last_x, y() + last_y, x() + cx - 1, y() + cyfirst);
        }

        last_x = cx;
        last_y = cylast;
      }

      cx += 1;
    }

    fl_color(FL_WHITE);
    if ((mouse_x > x()) && (mouse_x < x() + w()) && (series[element.first].size() == w())) {
      char txt[32];
      sprintf(txt, "%s  %.2f", name.c_str(), series[element.first][mouse_x - x()].max);
      fl_draw(txt, mouse_x + 5, mouse_y + (count + 1) * 15);
    }
    count += 1;
  }

  fl_color(FL_LIGHT1);
  fl_line(mouse_x, y(), mouse_x, y() + h());
}

int LogView::handle(int ev) {
  if (auto ret = Fl_Box::handle(ev)) {
    return ret;
  }
  if (ev == FL_MOVE) {
    mouse_x = Fl::event_x();
    mouse_y = Fl::event_y();
  }

  return 0;
}

