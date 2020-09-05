#include <variant>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Button.H>

#include "MainWindow.h"

class My_Group : public Fl_Group {
  Fl_Output output;
  viaems::StructureNode node;
  std::string label;

  public:
  My_Group(int X, int Y, int W, int H, viaems::StructureNode &_node)
  : Fl_Group(X, Y, W, H), 
  output(X, Y, W, H) {

    node = _node;
    if (node.is_leaf()) {
      output.value("Test");
      auto id = node.leaf()->path.back();
      if (std::holds_alternative<int>(id)) {
        label = std::to_string(std::get<int>(id)).c_str();
      } else {
        label = std::get<std::string>(id).c_str();
      }
      output.label(label.c_str());
    }
    end();
  }
  void draw() {
    output.resize(220, y(), w(), h());
    draw_child(output);
    output.draw_label(x(), y(), 180, h(), FL_ALIGN_LEFT);
    fl_line(200, y(), 200, y() + h());
  }
};

MainWindow::MainWindow()
  : MainWindowUI() {

}

void MainWindow::update_connection_status(bool status) {
  m_connection_status->color(status ? FL_GREEN : FL_RED);
  m_connection_status->redraw();
}

void MainWindow::update_feed_hz(int hz) {
  m_rate->value(std::to_string(hz).c_str());
  m_rate->redraw();
}

void MainWindow::feed_update(viaems::FeedUpdate const &update) {
  m_status_table->feed_update(update);
  //  chart->add(std::get<uint32_t>(update.at("cputime")));
}

static void add_config_structure_entry(Fl_Tree *tree, Fl_Tree_Item *parent, viaems::StructureNode node) {
  if (node.is_map()) {
    for (auto child : *node.map()) {
      auto item = new Fl_Tree_Item(tree->prefs());
      item->label(child.first.c_str());
      parent->add(tree->prefs(), "", item);
      if (child.second.is_leaf()) {
        auto leaf = child.second.leaf();
        item->widget(new My_Group(0, 0, 100, 18, child.second));
      } else {
        add_config_structure_entry(tree, item, child.second);
      }
    }
  } else if (node.is_list()) {
    int index = 0;
    for (auto child : *node.list()) {
      auto item = new Fl_Tree_Item(tree->prefs());
      item->label(std::to_string(index).c_str());
      parent->add(tree->prefs(), "", item);
      add_config_structure_entry(tree, item, child);
      index += 1;
    }
  }
}

void MainWindow::update_config_structure(viaems::StructureNode top) {
  m_config_tree->showroot(0);
  auto root = m_config_tree->root();
  m_config_tree->begin();
  for (auto child : *top.map()) {
    auto item = new Fl_Tree_Item(m_config_tree->prefs());
    item->label(child.first.c_str());
    root->add(m_config_tree->prefs(), "", item);
    add_config_structure_entry(m_config_tree, item, child.second);
  }
  m_config_tree->end();

  m_config_tree->redraw();
}
