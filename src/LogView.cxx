#include <chrono>

#include <FL/fl_draw.H>

#include "LogView.h"

LogView::LogView(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {
}

void LogView::update_time_range(std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point stop) {

  if (data.size() == 0) {
    auto newdata = log->GetRange({"rpm"}, start, stop);
    data.insert(data.end(), newdata.begin(), newdata.end());
  } else {
    auto newdata = log->GetRange({"rpm"}, data[data.size() - 1].time, stop);
    data.insert(data.end(), newdata.begin(), newdata.end());
  }
  for (auto i = data.begin(); i != data.end(); i++) {
    if (i->time >= start) {
      data.erase(data.begin(), i);
      break;
    }
  }
  
  auto start_time_ns =
  std::chrono::duration_cast<std::chrono::nanoseconds>(data[0].time.time_since_epoch()).count();
  auto stop_time_ns =
  std::chrono::duration_cast<std::chrono::nanoseconds>(data[data.size() - 1].time.time_since_epoch()).count();
  real_points.clear();
  for (int i = 0; i < w(); i++) {
    real_points.push_back(0);
  }

  for (int i = 0; i < data.size(); i++) {
    auto p_time_ns =
  std::chrono::duration_cast<std::chrono::nanoseconds>(data[i].time.time_since_epoch()).count();
    double x_ratio = (p_time_ns - start_time_ns) / (double)(stop_time_ns - start_time_ns);

    double y_ratio = std::get<uint32_t>(data[i]["rpm"]) / 6000.0;

    int x = w() * x_ratio;
    int y = h() * y_ratio;
    real_points[x] = y;
  }

  redraw();
}


void LogView::draw() {
  draw_box();
  if (log == nullptr) {
    return;
  }
  if (data.size() < 2) {
    return;
  }
  fl_color(FL_WHITE);
  for (int i = 0; i < w(); i++) {
    fl_point(x() + i, y() + real_points[i]);
  }
  fl_color(FL_RED);
  if ((mouse_x > x()) && (mouse_x < x() + w())) {
    char txt[32];
    sprintf(txt, "rpm   %u", 2000);
    fl_draw(txt, mouse_x + 5, mouse_y + 30);
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

