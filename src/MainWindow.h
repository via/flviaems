#ifndef MainWindow_h
#define MainWindow_h

#include <mutex>
#include <condition_variable>

#include "Log.h"
#include "MainWindowUI.h"
#include "viaems.h"

struct LogWriter {
  Log log;
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<viaems::LogChunk> chunks;
  std::thread thread;

  void add_chunk(viaems::LogChunk&& chunk) {
    std::unique_lock<std::mutex> lock(mutex);
    chunks.push_back(std::move(chunk));
    cv.notify_one();
  }

  void write_loop() {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex);
      if (chunks.empty()) {
        cv.wait(lock);
      }

      auto first = std::move(chunks.front());
      chunks.erase(chunks.begin());

      lock.unlock();

      log.Update(first);
    }
  }

  void start() {
    thread = std::thread([](LogWriter *w) { w->write_loop(); }, this);
  }

};


class MainWindow : public MainWindowUI {
  viaems::Model *m_model;
  Log log;
  LogWriter logwriter;
  viaems::StructurePath detail_path;

  void update_config_structure(viaems::StructureNode top);

  void update_table_editor(viaems::TableValue t);

  static void select_table(Fl_Widget *w, void *p);
  static void structure_value_update_callback(Fl_Widget *w, void *p);
  static void table_value_changed_callback(Fl_Widget *w, void *ptr);

  void add_config_structure_entry(Fl_Tree_Item *, viaems::StructureNode);

public:
  MainWindow();
  void feed_update(viaems::LogChunk&&);
  void update_connection_status(bool status);
  void update_feed_hz(int hz);
  void update_model(viaems::Model *model);
  void update_interrogation(bool in_progress, int value, int max);
  void update_config_value(viaems::StructurePath path,
                           viaems::ConfigValue value);
};

#endif
