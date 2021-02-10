#include <sstream>

#include <memory>

#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Tree.H>

#include "MainWindow.h"

class ConfigLeafTreeWidget;
typedef void (*Value_Change_Callback)(ConfigLeafTreeWidget *w,
                                      std::string value);

class SelectableTreeWidget : public Fl_Group {
protected:
  Fl_Box *id_box;

  Fl_Callback *select_cb = nullptr;
  void *select_cb_userdata;

  int handle(int e) {
    if (e == FL_PUSH) {
      if (Fl::event_inside(id_box)) {
        if (select_cb) {
          select_cb(this, select_cb_userdata);
        }
        return 1;
      }
    }
    return Fl_Group::handle(e);
  }

public:
  SelectableTreeWidget(int X, int Y, int W, int H, viaems::StructurePath path)
      : Fl_Group(X, Y, W, H) {

    this->path = path;

    auto id = path.back();
    std::string label;
    if (std::holds_alternative<int>(id)) {
      label = std::to_string(std::get<int>(id));
    } else {
      label = std::get<std::string>(id).c_str();
    }

    int w, h;
    fl_measure(label.c_str(), w, h);

    id_box = new Fl_Box{X, Y, w + 10, H};
    id_box->box(FL_FLAT_BOX);
    id_box->color(FL_WHITE);
    id_box->copy_label(label.c_str());
    id_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    end();
  }

  void select_callback(Fl_Callback *c, void *p = nullptr) {
    select_cb = c;
    select_cb_userdata = p;
  }

  void dirty(bool d) {
    id_box->color(d ? FL_RED : FL_WHITE);
    id_box->redraw();
  }

  virtual void update_value(viaems::ConfigValue v) { dirty(false); }
  virtual viaems::ConfigValue get_value() { return (uint32_t)0; }

  viaems::StructurePath path;
};

template <typename T> class NumericTreeWidget : public SelectableTreeWidget {
  Fl_Input *field;

  static void changed_callback(Fl_Widget *fl, void *p) {
    auto ctw = static_cast<NumericTreeWidget *>(p);
    ctw->callback()(ctw, ctw->user_data());
    ctw->dirty(true);
  }

public:
  NumericTreeWidget(int X, int Y, int W, int H, viaems::StructurePath path)
      : SelectableTreeWidget(X, Y, W, H, path) {

    begin();
    this->field = new Fl_Input{X + id_box->w(), Y, 100, H};
    this->field->callback(changed_callback, this);
    end();
  }

  void update_value(viaems::ConfigValue value) {
    this->field->value(std::to_string(std::get<T>(value)).c_str());
    dirty(false);
  }

  viaems::ConfigValue get_value() {
    T value;
    std::istringstream ss{field->value()};
    ss >> value;
    return value;
  }
};

class ChoiceTreeWidget : public SelectableTreeWidget {
  Fl_Choice *chooser;

protected:
  static void changed_callback(Fl_Widget *fl, void *p) {
    auto ctw = static_cast<ChoiceTreeWidget *>(p);
    ctw->callback()(ctw, ctw->user_data());
    ctw->dirty(true);
  }

public:
  ChoiceTreeWidget(int X, int Y, int W, int H, viaems::StructurePath path,
                   std::vector<std::string> choices)
      : SelectableTreeWidget(X, Y, W, H, path) {

    begin();

    this->chooser = new Fl_Choice{X + id_box->w(), Y, 100, H};
    this->chooser->callback(changed_callback, this);
    for (auto choice : choices) {
      this->chooser->add(choice.c_str());
    }

    end();
  }

  void update_value(viaems::ConfigValue value) {
    try {
      int index = this->chooser->find_index(std::get<std::string>(value).c_str());
      this->chooser->value(index);
      dirty(false);
    } catch (std::exception &_) {
      std::cerr <<"Exception!" << std::endl;
    }
  }

  viaems::ConfigValue get_value() { return std::string{chooser->text()}; }
};

static SelectableTreeWidget *get_config_tree_widget(Fl_Tree *tree, viaems::StructurePath path) {
  for (Fl_Tree_Item *item = tree->first(); item; item = item->next()) {
    auto w = dynamic_cast<SelectableTreeWidget *>(item->widget());
    if (w && w->path == path) {
      return w;
    }
  }
  return nullptr;
}


MainWindow::MainWindow() : MainWindowUI() {
  /* Tables */
  m_table_title->callback(table_value_changed_callback, this);
  m_table_rows->callback(table_value_changed_callback, this);
  m_table_cols->callback(table_value_changed_callback, this);
  m_table_editor_box->callback(table_value_changed_callback, this);

  logwriter.log.SetOutputFile("log.vlog");
  logwriter.start();

  /* Default log output */
  log.SetOutputFile("log.vlog");
//  log.SetOutputFile(":memory:");
  m_logview->SetLog(&log);
}

void MainWindow::update_connection_status(bool status) {
  m_connection_status->color(status ? FL_GREEN : FL_RED);
  m_connection_status->redraw();
}

void MainWindow::update_feed_hz(int hz) {
  m_rate->value(std::to_string(hz).c_str());
  m_rate->redraw();
}

void MainWindow::feed_update(viaems::LogChunk &&updates) {
  std::map<std::string, viaems::FeedValue> status;
  for (int i = 0; i < updates.keys.size(); i++) {
    status.insert(std::make_pair(updates.keys[i], updates.points[0].values[i]));
  }
  m_status_table->feed_update(status);
//  log.Update(updates);
  logwriter.add_chunk(std::move(updates));

  auto stop_time = std::chrono::system_clock::now();
  auto start_time = stop_time - std::chrono::seconds{20};
  m_logview->update_time_range(start_time, stop_time);
}

void MainWindow::select_table(Fl_Widget *w, void *p) {
  auto mw = (MainWindow *)p;
  auto s = (SelectableTreeWidget *)w;

  auto value = mw->m_model->get_value(s->path);
  mw->m_table_editor->take_focus();
  mw->update_table_editor(std::get<viaems::TableValue>(value));
  mw->detail_path = s->path;
}

void MainWindow::table_value_changed_callback(Fl_Widget *w, void *ptr) {
  auto mw = static_cast<MainWindow *>(ptr);
  auto table = mw->m_table_editor->getTable();
  table.title = mw->m_table_title->value();
  mw->m_model->set_value(mw->detail_path, table);

  auto item = get_config_tree_widget(mw->m_config_tree, mw->detail_path);
  item->dirty(true);
}

void MainWindow::update_table_editor(viaems::TableValue val) {
  m_table_title->value(val.title.c_str());
  if (val.axis.size() == 1) {
    m_table_rows->value(val.one.size());
    m_table_cols->value(1);
  } else {
    m_table_rows->value(val.two.size());
    m_table_cols->value(val.two[0].size());
  }
  m_table_editor->setTable(val);

  m_table_editor_box->show();
  m_sensor_editor_box->hide();
}

void MainWindow::structure_value_update_callback(Fl_Widget *w, void *p) {
  auto c = dynamic_cast<SelectableTreeWidget *>(w);
  auto m = static_cast<MainWindow *>(p);
  auto val = c->get_value();
  m->m_model->set_value(c->path, val);
}

void MainWindow::add_config_structure_entry(Fl_Tree_Item *parent,
                                            viaems::StructureNode node) {
  if (node.is_map()) {
    for (auto child : node.map()) {
      auto item = new Fl_Tree_Item(m_config_tree->prefs());
      item->label(child.first.c_str());
      parent->add(m_config_tree->prefs(), "", item);
      if (child.second.is_leaf()) {
        auto leaf = child.second.leaf();
        auto value = m_model->get_value(leaf.path);
        SelectableTreeWidget *w;

        if (leaf.type == "uint32") {
          auto value = m_model->get_value(leaf.path);
          w = new NumericTreeWidget<uint32_t>(0, 0, 300, 18, leaf.path);
          w->update_value(value);
          w->callback(structure_value_update_callback, this);
        } else if (leaf.type == "float") {
          auto value = m_model->get_value(leaf.path);
          w = new NumericTreeWidget<float>(0, 0, 300, 18, leaf.path);
          w->update_value(value);
          w->callback(structure_value_update_callback, this);
        } else if (leaf.type == "string") {
          auto value = m_model->get_value(leaf.path);
          w = new ChoiceTreeWidget(0, 0, 300, 18, leaf.path, leaf.choices);
          w->update_value(value);
          w->callback(structure_value_update_callback, this);
        } else if (leaf.type == "table") {
          w = new SelectableTreeWidget(0, 0, 300, 18, leaf.path);
          w->select_callback(select_table, this);
        } else {
          w = new SelectableTreeWidget(0, 0, 300, 18, leaf.path);
        }
        item->widget(w);
      } else {
        add_config_structure_entry(item, child.second);
      }
    }
  } else if (node.is_list()) {
    int index = 0;
    for (auto child : node.list()) {
      auto item = new Fl_Tree_Item(m_config_tree->prefs());
      item->label(std::to_string(index).c_str());
      parent->add(m_config_tree->prefs(), "", item);
      add_config_structure_entry(item, child);
      index += 1;
    }
  }
}

void MainWindow::update_config_structure(viaems::StructureNode top) {
  m_config_tree->showroot(0);
  m_config_tree->selectmode(FL_TREE_SELECT_NONE);
  auto root = m_config_tree->root();
  m_config_tree->begin();
  for (auto child : top.map()) {
    auto item = new Fl_Tree_Item(m_config_tree->prefs());
    item->label(child.first.c_str());
    root->add(m_config_tree->prefs(), "", item);
    add_config_structure_entry(item, child.second);
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
  m_config_tree->clear_children(m_config_tree->root());

  m_model = model;
  update_config_structure(model->structure());
}

void MainWindow::update_config_value(viaems::StructurePath path,
                                     viaems::ConfigValue value) {
  auto w = get_config_tree_widget(m_config_tree, path);
  if (w == nullptr) {
    return;
  }

  w->update_value(value);

  if (path == detail_path) {
    update_table_editor(std::get<viaems::TableValue>(value));
  }
}
