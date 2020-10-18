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
  std::shared_ptr<viaems::ConfigValue> value;
  std::string label;

  void update_value() {
    if (std::holds_alternative<std::string>(*value)) {
      int index = choices.find_index(std::get<std::string>(*value).c_str());
      choices.value(index);
    } else if (std::holds_alternative<uint32_t>(*value)) {
      output.value(std::to_string(std::get<uint32_t>(*value)).c_str());
    } else if (std::holds_alternative<float>(*value)) {
      output.value(std::to_string(std::get<float>(*value)).c_str());
    }
  }

  public:
  ConfigLeafTreeWidget(int X, int Y, int W, int H, viaems::ConfigNode &_node, std::shared_ptr<viaems::ConfigValue> val)
  : Fl_Group(X, Y, W, H),
  choices{X, Y, 100, 18},
  output{X, Y, 100, 18} {

    node = _node;
    value = val;
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
    }
    update_value();
    end();
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

static void add_config_structure_entry(Fl_Tree *tree, Fl_Tree_Item *parent, viaems::StructureNode node, viaems::Model *model) {
  if (node.is_map()) {
    for (auto child : *node.map()) {
      auto item = new Fl_Tree_Item(tree->prefs());
      item->label(child.first.c_str());
      parent->add(tree->prefs(), "", item);
      if (child.second.is_leaf()) {
        auto leaf = *child.second.leaf();
        auto val = model->get_value(leaf.path);
        auto w = new ConfigLeafTreeWidget(0, 0, 100, 18, leaf, val);
        item->widget(w);
      } else {
        add_config_structure_entry(tree, item, child.second, model);
      }
    }
  } else if (node.is_list()) {
    int index = 0;
    for (auto child : *node.list()) {
      auto item = new Fl_Tree_Item(tree->prefs());
      item->label(std::to_string(index).c_str());
      parent->add(tree->prefs(), "", item);
      add_config_structure_entry(tree, item, child, model);
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
    add_config_structure_entry(m_config_tree, item, child.second, m_model);

  }
  m_config_tree->end();

  m_config_tree->redraw();
}

void MainWindow::update_interrogation(bool in_progress, int val, int max) {
  m_interrogation_progress->maximum(max);
  m_interrogation_progress->value(val);
  if (in_progress) {
    m_interrogation_progress->show();
  } else {
    m_interrogation_progress->hide();
  }
}

void MainWindow::update_model(viaems::Model *model) {
  m_model = model;
  update_config_structure(model->structure());
}


