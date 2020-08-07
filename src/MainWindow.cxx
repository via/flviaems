#include "MainWindow.h"

MainWindow::MainWindow() : MainWindowUI() {

//  chart->autosize(1);
  chart->type(FL_LINE_CHART);
  chart->bounds(0, 10000000);
}

void MainWindow::feed_update(viaems::FeedUpdate const& update) {
  m_status_table->feed_update(update);
//  chart->add(std::get<uint32_t>(update.at("cputime")));
}
