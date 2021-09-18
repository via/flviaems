#include <iostream>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <numeric>

#include "viaems.h"
#include "Log.h"

struct AnalysisPoint {
  uint64_t realtime_ns;
  double rpm;
  double ve;
  double lambda;
  double accel_enrich_percent;
  double temp_enrich_percent;
  double map;
  double ego;
};

std::vector<AnalysisPoint> get_points() {
  std::vector<std::string> keys{"realtime_ns",
                                "rpm",
                                "ve",
                                "lambda",
                                "accel_enrich_percent",
                                "temp_enrich_percent",
                                "sensor.map",
                                "sensor.ego"};

  auto log = std::make_shared<Log>("log.vlog");
  auto data = log->GetRange(keys, {}, log->EndTime());
  std::vector<AnalysisPoint> points;
  for (int index = 0; index < data.times.size(); index++) {
    AnalysisPoint p;
    p.realtime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(data.times[index].time_since_epoch()).count();
    for (const auto & [k, v] : data.values) {
      if (k == "rpm") {
        p.rpm = std::get<uint32_t>(v[index]);
      } else if (k == "ve") {
        p.ve = std::get<float>(v[index]);
      } else if (k == "lambda") {
        p.lambda = std::get<float>(v[index]);
      } else if (k == "accel_enrich_percent") {
        p.accel_enrich_percent = std::get<float>(v[index]);
      } else if (k == "temp_enrich_percent") {
        p.temp_enrich_percent = std::get<float>(v[index]);
      } else if (k == "sensor.map") {
        p.map = std::get<float>(v[index]);
      } else if (k == "sensor.ego") {
        p.ego = std::get<float>(v[index]);
      } 
    }
    points.push_back(p);
  }
  return points;
}

void add_to_window(std::vector<AnalysisPoint> &window, uint64_t window_size_ns, const AnalysisPoint point) {
  window.push_back(point);
  while((window.size() > 0) && point.realtime_ns - window[0].realtime_ns > window_size_ns) {
    window.erase(window.begin());
  }
}

AnalysisPoint average(const std::vector<AnalysisPoint> &window) {
  AnalysisPoint average{};
  for (const auto &p : window) {
    average.rpm += p.rpm;
    average.ve += p.ve;
    average.lambda += p.lambda;
    average.accel_enrich_percent += p.accel_enrich_percent;
    average.temp_enrich_percent += p.temp_enrich_percent;
    average.map += p.map;
    average.ego += p.ego;
  }
  average.rpm /= window.size();
  average.ve /= window.size();
  average.lambda /= window.size();
  average.accel_enrich_percent /= window.size();
  average.temp_enrich_percent /= window.size(); 
  average.map /= window.size();
  average.ego /= window.size();
  return average;
}
  
bool within_percent(double a, double b, double percent) {
  return (fabs((a - b) / b * 100) < percent);
}


bool window_is_stable(const std::vector<AnalysisPoint> &window) {
  auto avg = average(window);
  if (avg.temp_enrich_percent != 1.0) {
    return false;
  }
  if (avg.accel_enrich_percent != 0.0) {
    return false;
  }
  for (const auto &p : window) {
    if (!within_percent(p.rpm, avg.rpm, 5)) {
      return false;
    }
    if (!within_percent(p.ego, avg.ego, 3)) {
      return false;
    }
    if (!within_percent(p.map, avg.map, 5)) {
      return false;
    }
  }
  return true;
}

struct PointBuckets {
  std::vector<double> rows;
  std::vector<double> cols;
  double r_dist;
  double c_dist;

  std::vector<std::vector<std::vector<double>>> table;

  PointBuckets(std::vector<double> rows, std::vector<double> cols, double r_dist, double c_dist) :
    rows{rows}, cols{cols}, r_dist{r_dist}, c_dist{c_dist} {
    for (int r = 0; r < rows.size(); r++) {
      table.push_back({});
      for (int c = 0; c < cols.size(); c++) {
        table[r].push_back({});
      }
    }
  }

  bool insert(double rval, double cval, double ve) {
    for (int r = 0; r < rows.size(); r++) {
      if (fabs(rows[r] - rval) < r_dist) {
        for (int c = 0; c < cols.size(); c++) {
          if (fabs(cols[c] - cval) < c_dist) {
            table[r][c].push_back(ve);
            return true;
          }
        }
      }
    }
    return false;
  }

  void report() {
    printf("    ");
    for (auto r : rows) {
      printf("%4g ", r);
    }
    printf("\n");
    for (int c = 0; c < cols.size(); c++) {
      printf("%3g: ", cols[c]);
      for (int r = 0; r < rows.size(); r++) {
        double avg = table[r][c].size() == 0 ? 0 : std::accumulate(table[r][c].begin(), table[r][c].end(), 0.0) / table[r][c].size();
        printf("%4d ", (int)avg);
      }
      printf("\n");
    }
  }
};

int main() {
  std::vector<double> rows{500, 900, 1200, 1500, 1800, 2100, 2400, 2700, 3000, 3300, 3600, 3900, 4200, 4500, 4800, 5100, 5400, 5700, 6000, 6300, 6600};
  std::vector<double> cols{23, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 130, 135, 140, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230};
  std::cerr << "retrieving..." << std::endl;;
  auto original = get_points();
  std::cerr << "retrieved " << original.size() << " points" << std::endl;;
  std::vector<AnalysisPoint> window;
  PointBuckets buckets{rows, cols, 100, 3};
  for (const auto &point : original) {
    add_to_window(window, 250000000, point);
    if (window_is_stable(window)) {
      double new_ve = point.ego / point.lambda * point.ve;
      buckets.insert(point.rpm, point.map, new_ve);
    }
  }
  buckets.report();
}
