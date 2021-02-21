#include <chrono>

#include <FL/fl_draw.H>

#include "LogView.h"

LogView::LogView(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {
  config.insert(std::make_pair("rpm", SeriesConfig{0, 6000, FL_RED}));
  config.insert(std::make_pair("last_trigger_angle", SeriesConfig{0, 720, FL_YELLOW}));
  series.insert(std::make_pair("rpm", std::vector<PointGroup>{}));
  series.insert(std::make_pair("last_trigger_angle", std::vector<PointGroup>{}));
}

void LogView::update_pointgroups(int x0, int x1) {
  auto fetch_start =  start_ns + (ns_per_pixel * x0);
  auto fetch_stop = start_ns + (ns_per_pixel * x1);

  std::vector<std::string> keys;
  for (auto i : config) {
    keys.push_back(i.first);
  }

  std::vector<std::vector<PointGroup> *> keymap;
  for (auto k : keys) {
    keymap.push_back(&series[k]);
  }

  auto updates = log->GetRange(keys, fetch_start, fetch_stop);

  int cur_pixel = x0;
  uint64_t end_time_of_pixel = start_ns + (ns_per_pixel * cur_pixel);
  for (auto update : updates.points) {
    auto t = std::chrono::duration_cast<std::chrono::nanoseconds>(update.time.time_since_epoch()).count();
    while ((t >= end_time_of_pixel) && cur_pixel <= x1) {
      cur_pixel++;
      end_time_of_pixel = start_ns + (ns_per_pixel * cur_pixel);
    }
    if (cur_pixel > x1) {
      break;
    }
    for (int k = 0; k < keys.size(); k++) {
      auto &s = keymap[k]->at(cur_pixel);
      viaems::FeedValue fv = update.values[k];
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

void LogView::update_time_range(uint64_t new_start, uint64_t new_stop) {


  std::vector<std::string> keys;
  for (auto i : config) {
    keys.push_back(i.first);
  }

  auto old_w = series.begin()->second.size();
  auto old_ns_per_pixel = old_w ? ((stop_ns - start_ns) / old_w) : 0;
  auto shift = new_stop - stop_ns;
  ns_per_pixel = (new_stop - new_start) / w();

  start_ns = new_start;
  stop_ns = new_stop;

  /* Is current pointgroup list usable? */
  if ((old_w == w()) &&
      (old_ns_per_pixel == ns_per_pixel)) {
    /* Shift as necessary, repopulate new sections */
    int shift_pixels = shift / ns_per_pixel;
    for (auto i : config) {
      for (auto k = 0; k < w() - shift_pixels; k++) {
        series[i.first][k] = series[i.first][k + shift_pixels];
      }
      for (auto k = w() - shift_pixels; k < w(); k++) {
        series[i.first][k] = PointGroup{};
      }
    }
    update_pointgroups(w() - 1 - shift_pixels, w() - 1);
  } else {
    for (auto i : config) {
      series[i.first].clear();
      for (auto k = 0; k < w(); k++) {
        PointGroup pg{};
        series[i.first].push_back(pg);
      }
    }
    update_pointgroups(0, w() - 1);
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
