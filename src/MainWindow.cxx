#include "MainWindow.h"

MainWindow::MainWindow() : MainWindowUI() {

//  chart->autosize(1);
  chart->type(FL_LINE_CHART);
  chart->bounds(0, 10000000);
}

void MainWindow::update_connection_status(bool status) {
  m_connection_status->color(status ? FL_GREEN: FL_RED);
  m_connection_status->redraw();
}

void MainWindow::feed_update(viaems::FeedUpdate const& update) {
  m_status_table->feed_update(update);
//  chart->add(std::get<uint32_t>(update.at("cputime")));
}

void MainWindow::update_config_structure(viaems::ConfigNode top) {
  auto g = std::get<std::shared_ptr<viaems::ConfigNodeMap>>(top.contents);

  for (auto child : *g) {
    auto parent = m_config_tree->add(child.first.c_str());
    if (std::holds_alternative<std::shared_ptr<viaems::ConfigNodeMap>>(child.second.contents)) {
      auto h = std::get<std::shared_ptr<viaems::ConfigNodeMap>>(child.second.contents);
      for (auto ochild : *h) {
        m_config_tree->add(parent, ochild.first.c_str());
      }
    }


  }

  m_config_tree->redraw();
}
