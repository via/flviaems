#include <chrono>

#include <FL/fl_draw.H>

#include "LogView.h"

LogView::LogView(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {
  config.insert(std::make_pair("rpm", SeriesConfig{0, 6000, FL_RED}));
  config.insert(std::make_pair("last_trigger_angle", SeriesConfig{0, 720, FL_YELLOW}));
  series.insert(std::make_pair("rpm", std::vector<PointGroup>{}));
  series.insert(std::make_pair("last_trigger_angle", std::vector<PointGroup>{}));
}


struct range {
  uint64_t start_ns;
  uint64_t stop_ns;
};

void LogView::update_pointgroups(int x0, int x1) {
  std::chrono::nanoseconds fetch_start = start_ns + (ns_per_pixel * x0);
  std::chrono::nanoseconds fetch_stop = start_ns + (ns_per_pixel * x1);

  std::vector<std::string> keys;
  for (auto i : config) {
    keys.push_back(i.first);
  }

  auto updates = log->GetRange(keys, new_start, cached_start);

void LogView::update_time_range(std::chrono::system_clock::time_point new_start,
    std::chrono::system_clock::time_point new_stop) {

  auto new_start_time_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(new_start.time_since_epoch()).count();
  auto new_stop_time_ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(new_stop.time_since_epoch()).count();

  std::vector<std::string> keys;
  for (auto i : config) {
    keys.push_back(i.first);
  }

  auto old_w = series.begin()->second.size();
  auto old_ns_per_pixel = (stop_ns - start_ns) / old_w;
  auto ns_per_pixel = (new_stop_time_ns - new_start_time_ns) / w();

  /* Is current pointgroup list usable? */
  if ((old_w == w() &&
      (old_ns_per_pixel == ns_per_pixel)) {
    /* Shift as necessary, repopulate new sections */
  } else {
    /* No, reset everything */
    }

  for (auto i : config) {
    series[i.first].clear();
    for (auto k = 0; k < w(); k++) {
      PointGroup pg{};
      series[i.first].push_back(pg);
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

  std::vector<std::vector<PointGroup> *> keymap;
  for (auto k : keys) {
    keymap.push_back(&series[k]);
  }
  int pixel = 0;
  for (auto i = cache.points.begin(); i != cache.points.end(); i++) {
    auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(i->time.time_since_epoch()).count();
    while ((t >= pixel_ranges[pixel].stop_ns) && pixel < w()) pixel++;
    if (pixel == w()) {
      break;
    }

    for (int k = 0; k < keys.size(); k++) {
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
