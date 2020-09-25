#include <FL/Fl_Tree.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Button.H>


#include "MainWindow.h"

class ConfigLeafTreeWidget : public Fl_Group {
  Fl_Input output;
  Fl_Choice choices;
  viaems::ConfigNode node;
  std::string label;

  public:
  ConfigLeafTreeWidget(int X, int Y, int W, int H, viaems::ConfigNode &_node)
  : Fl_Group(X, Y, W, H),
  choices(X, Y, 100, 18),
  output(X, Y, 100, 18) {

    node = _node;
    auto id = node.path.back();
    if (std::holds_alternative<int>(id)) {
      label = std::to_string(std::get<int>(id)).c_str();
    } else {
      label = std::get<std::string>(id).c_str();
    }

    int w, h;
    fl_measure(label.c_str(), w, h);

    if (node.type == "string") {
      output.hide();
      choices.label(label.c_str());
      for (auto choice : node.choices) {
        choices.add(choice.c_str());
      }
      choices.position(X + w, Y);
    } else {
      choices.hide();
      output.label(label.c_str());
      output.resize(X + w, Y, output.w(), output.h());
      output.value("Test");
    }
    end();
  }

  void set_value(std::string val) {
    int index = choices.find_index(val.c_str());
    choices.value(index);
    redraw();
  }
  void set_value(float val) {
    output.value(std::to_string(val).c_str());
    redraw();
  }
  void set_value(uint32_t val) {
    output.value(std::to_string(val).c_str());
    redraw();
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
}

static Fl_Tree_Item *find_tree_item_by_path(Fl_Tree_Item *tree, viaems::StructurePath &path) {
  if (path.empty()) {
    return tree;
  }
  for (Fl_Tree_Item *item = tree->next(); item; item=item->next_sibling()) {
    const auto element = path.front();
    if (std::holds_alternative<std::string>(element)) {
      if (item->label() == std::get<std::string>(element)) {
        path.erase(path.begin());
        return find_tree_item_by_path(item, path);
      }
    }
    if (std::holds_alternative<int>(element)) {
      if (item->label() == std::to_string(std::get<int>(element))) {
        path.erase(path.begin());
        return find_tree_item_by_path(item, path);
      }
    }
  }
  return NULL;
}



void MainWindow::update_config_value(viaems::StructurePath &path, viaems::ConfigValue &value) {

  auto item = find_tree_item_by_path(m_config_tree->root(), path);
  auto widget = (ConfigLeafTreeWidget *)item->widget();
  if (!widget) {
    return;
  }
  if (std::holds_alternative<float>(value)) {
    widget->set_value(std::get<float>(value));
  }
  if (std::holds_alternative<uint32_t>(value)) {
    widget->set_value(std::get<uint32_t>(value));
  }
  if (std::holds_alternative<std::string>(value)) {
    widget->set_value(std::get<std::string>(value));
  }
}

static void add_config_structure_entry(Fl_Tree *tree, Fl_Tree_Item *parent, viaems::StructureNode node) {
  if (node.is_map()) {
    for (auto child : *node.map()) {
      auto item = new Fl_Tree_Item(tree->prefs());
      item->label(child.first.c_str());
      parent->add(tree->prefs(), "", item);
      if (child.second.is_leaf()) {
        item->widget(new ConfigLeafTreeWidget(0, 0, 100, 18, *child.second.leaf()));
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
