#include "MainWindow.h"

MainWindow::MainWindow()
  : MainWindowUI() {

  //  chart->autosize(1);
  chart->type(FL_LINE_CHART);
  chart->bounds(0, 10000000);
}

void MainWindow::update_connection_status(bool status) {
  m_connection_status->color(status ? FL_GREEN : FL_RED);
  m_connection_status->redraw();
}

void MainWindow::update_feed_hz(int hz) {
//  std::cerr << "hz: " << hz << std::endl;
}

void MainWindow::feed_update(viaems::FeedUpdate const &update) {
  m_status_table->feed_update(update);
  //  chart->add(std::get<uint32_t>(update.at("cputime")));
}

void MainWindow::update_config_structure(viaems::StructureNode top) {
  auto g = top.map();

  for (auto child : *g) {
    std::cerr << child.first << std::endl;
    auto parent = m_config_tree->add(child.first.c_str());
    if (child.second.is_map()) {
      auto h = child.second.map();
      for (auto ochild : *h) {
        m_config_tree->add(parent, ochild.first.c_str());
      }
    } else if (child.second.is_list()) {
      int index = 0;
      for (auto ochild : *child.second.list()) {
        m_config_tree->add(parent, std::to_string(index).c_str());
        index += 1;
      }

    }
  }

  m_config_tree->redraw();
}
