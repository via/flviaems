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
