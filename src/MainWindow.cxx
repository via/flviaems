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

static void add_config_structure_entry(Fl_Tree *tree, viaems::StructureNode node, Fl_Tree_Item *parent) {
  if (node.is_map()) {
    for (auto child : *node.map()) {
      auto entry = tree->add(parent, child.first.c_str());
      add_config_structure_entry(tree, child.second, entry);
    }
  } else if (node.is_list()) {
    int index = 0;
    for (auto child : *node.list()) {
      auto entry = tree->add(parent, std::to_string(index).c_str());
      add_config_structure_entry(tree, child, entry);
      index += 1;
    }
  }
}

void MainWindow::update_config_structure(viaems::StructureNode top) {
  m_config_tree->showroot(0);
  for (auto child : *top.map()) {
    auto entry = m_config_tree->add(child.first.c_str());
    add_config_structure_entry(m_config_tree, child.second, entry);
  }

  m_config_tree->redraw();
}
