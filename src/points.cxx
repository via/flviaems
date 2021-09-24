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

std::vector<AnalysisPoint> get_points(Log& log) {
  std::vector<std::string> keys{"realtime_ns",
                                "rpm",
                                "ve",
                                "lambda",
                                "accel_enrich_percent",
                                "temp_enrich_percent",
                                "sensor.map",
                                "sensor.ego"};

  auto end = log.EndTime();
//  auto data = log.GetRange(keys, {}, end);
  auto data = log.GetRange(keys, end - std::chrono::minutes{20}, end);
  std::vector<AnalysisPoint> points;

  auto *rpms = data.valuesForKey("rpm");
  auto *lambdas = data.valuesForKey("lambda");
  auto *ves = data.valuesForKey("ve");
  auto *aeps = data.valuesForKey("accel_enrich_percent");
  auto *teps = data.valuesForKey("temp_enrich_percent");
  auto *maps = data.valuesForKey("sensor.map");
  auto *egos = data.valuesForKey("sensor.ego");

  for (int index = 0; index < data.times.size(); index++) {
    AnalysisPoint p;
    p.realtime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(data.times[index].time_since_epoch()).count();
    p.rpm = std::get<uint32_t>((*rpms)[index]);
    p.ve = std::get<float>((*ves)[index]);
    p.lambda = std::get<float>((*lambdas)[index]);
    p.accel_enrich_percent = std::get<float>((*aeps)[index]);
    p.temp_enrich_percent = std::get<float>((*teps)[index]);
    p.map = std::get<float>((*maps)[index]);
    p.ego = std::get<float>((*egos)[index]);
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

void apply(viaems::TableValue& table, const PointBuckets& buckets) {
  for (int c = 0; c < buckets.cols.size(); c++) {
    for (int r = 0; r < buckets.rows.size(); r++) {
      double avg = buckets.table[r][c].size() == 0 ? 0 : std::accumulate(buckets.table[r][c].begin(), buckets.table[r][c].end(), 0.0) / buckets.table[r][c].size();
      if (avg > 1.0) {
        table.two[c][r] = (float)avg;
      }
    }
  }
}

PointBuckets compare(viaems::TableValue& table, const PointBuckets& buckets) {
  PointBuckets ret = buckets;
  for (int c = 0; c < buckets.cols.size(); c++) {
    for (int r = 0; r < buckets.rows.size(); r++) {
      double avg = buckets.table[r][c].size() == 0 ? 0 : std::accumulate(buckets.table[r][c].begin(), buckets.table[r][c].end(), 0.0) / buckets.table[r][c].size();
      if (avg > 1.0) {
        ret.table[r][c] = {avg - table.two[c][r]};
      } else {
        ret.table[r][c] = {0};
      }

    }
  }
  return ret;
}

int main() {
  auto log = std::make_shared<Log>("log.vlog");
  auto config = log->LoadConfigs()[0];

  config.name = "log analysis";
  config.save_time = std::chrono::system_clock::now();
  auto ve = std::get<viaems::TableValue>(config.values[{"tables", "ve"}]);

  std::vector<double> rows{250, 500, 900, 1200, 1500, 1800, 2100, 2400, 2700, 3000, 3300, 3600, 3900, 4200, 4500, 4800, 5100, 5400, 5700, 6000, 6300, 6600};
  std::vector<double> cols{23, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 100, 120,  140, 160, 180, 200, 220, 240};

  std::cerr << "retrieving..." << std::endl;;
  auto original = get_points(*log);
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
  auto difference = compare(ve, buckets);
  difference.report();
  apply(ve, buckets);
  config.values[{"tables", "ve"}] = ve;
//  log->SaveConfig(config);
}
