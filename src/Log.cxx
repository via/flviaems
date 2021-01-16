
#include "Log.h"

Log::Log() {}

void Log::ensure_section_usable() {
  if (sections.empty()) {
    sections.push_back(LogSection{.ready = true});
  } else if (sections[sections.size() - 1].full()) {
    auto& section = sections[sections.size() - 1];
    uint64_t pos = 0;
    if (section.dirty) {
      /* Write to file, update section_length, free memory */
      section.points.clear();
    }
    sections.push_back(LogSection{
      .file_position = pos,
      .ready = true,
    });
  }
}

void Log::Update(std::vector<viaems::FeedUpdate> list) {
  ensure_section_usable();

  auto& section = sections[sections.size() - 1];
  for (auto &entry : list) {
    LogPoint point{};
    point.time = entry.time;
    for (auto &x : entry) {
      point.values.push_back(x.second);
    }
    section.points.push_back(point);
    section.dirty = true;
  }
}

void Log::SetOutputFile(std::string path) {
  logfile.open(path);
}
