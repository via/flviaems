#include <chrono>

#include <FL/fl_draw.H>

#include "LogView.h"

LogView::LogView(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {
  config.insert(std::make_pair("rpm", SeriesConfig{0, 6000}));
  config.insert(std::make_pair("last_trigger_angle", SeriesConfig{0, 720}));
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

  for (auto i = cache.points.begin(); i != cache.points.end(); i++) {
    auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(i->time.time_since_epoch()).count();
    int x = w() * ((double)(t - start_time_ns) / (stop_time_ns - start_time_ns));
    if ((x < 0) || (x >= w())) {
      continue;
    }
    for (int k = 0; k < keys.size(); k++) {
      std::visit([&](const auto v) { series[keys[k]].at(x).mean = v; }, i->values[k]);
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
    fl_color(FL_WHITE);
    int cx = 0;
    for (const auto pointgroup : series[element.first]) {
      int cy = h() * ((pointgroup.mean - conf.min_y) / (conf.max_y - conf.min_y));
      fl_point(x() + cx, y() + cy);
      cx += 1;
    }

    fl_color(FL_RED);
    if ((mouse_x > x()) && (mouse_x < x() + w())) {
      char txt[32];
      sprintf(txt, "%s   %f", name.c_str(), series[element.first][mouse_x - x()].mean);
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

