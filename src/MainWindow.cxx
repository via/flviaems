#include <iostream>
#include <sstream>

#include <functional>
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
      int index =
          this->chooser->find_index(std::get<std::string>(value).c_str());
      this->chooser->value(index);
      dirty(false);
    } catch (std::exception &_) {
      std::cerr << "Exception!" << std::endl;
    }
  }

  viaems::ConfigValue get_value() { return std::string{chooser->text()}; }
};

static SelectableTreeWidget *
get_config_tree_widget(Fl_Tree *tree, viaems::StructurePath path) {
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

  m_sensor_source->callback(sensor_value_changed_callback, this);
  m_sensor_method->callback(sensor_value_changed_callback, this);
  m_sensor_pin->callback(sensor_value_changed_callback, this);
  m_sensor_lag->callback(sensor_value_changed_callback, this);
  m_sensor_range_min->callback(sensor_value_changed_callback, this);
  m_sensor_range_max->callback(sensor_value_changed_callback, this);
  m_sensor_therm_bias->callback(sensor_value_changed_callback, this);
  m_sensor_therm_A->callback(sensor_value_changed_callback, this);
  m_sensor_therm_B->callback(sensor_value_changed_callback, this);
  m_sensor_therm_C->callback(sensor_value_changed_callback, this);
  m_sensor_window_width->callback(sensor_value_changed_callback, this);
  m_sensor_window_total->callback(sensor_value_changed_callback, this);
  m_sensor_window_offset->callback(sensor_value_changed_callback, this);
  m_sensor_fault_min->callback(sensor_value_changed_callback, this);
  m_sensor_fault_max->callback(sensor_value_changed_callback, this);
  m_sensor_fault_value->callback(sensor_value_changed_callback, this);
  m_sensor_const->callback(sensor_value_changed_callback, this);

  m_output_editor->changed_callback(std::bind(output_value_changed_callback,
                                              this, std::placeholders::_1,
                                              std::placeholders::_2));

  /* Hook up buttons to logview */
  m_logview_in->callback([](Fl_Widget *w, void *v){
      ((LogView *)v)->zoom(0.2);
      }, m_logview);
  m_logview_out->callback([](Fl_Widget *w, void *v){
      ((LogView *)v)->zoom(-0.2);
      }, m_logview);
  m_logview_back->callback([](Fl_Widget *w, void *v){
      ((LogView *)v)->shift(std::chrono::seconds{-5});
      }, m_logview);
  m_logview_forward->callback([](Fl_Widget *w, void *v){
      ((LogView *)v)->shift(std::chrono::seconds{5});
      }, m_logview);
  m_logview->callback([](Fl_Widget *, void *v){
      auto mw = (MainWindow *)v;
      mw->logview_paused = true;
      mw->m_logview_pause->set();
      mw->m_logview_follow->clear();
      }, this);
  auto pause_cb = [](Fl_Widget *w, void *v){
      auto mw = (MainWindow *)v;
      mw->logview_paused = !mw->logview_paused;
      mw->m_logview_pause->value(mw->logview_paused ? 1 : 0);
      mw->m_logview_follow->value(mw->logview_paused ? 0 : 1);
      };
  m_logview_follow->set();
  m_logview_pause->callback(pause_cb, this);
  m_logview_follow->callback(pause_cb, this);
}

struct LogMenuData {
  MainWindow *mw;
  std::string text;
  viaems::Configuration config;
};

void MainWindow::select_prev_config_callback(Fl_Widget *w, void *v) {
  auto item = (LogMenuData *)v;
  auto clicked_item = item->mw->m_bar->mvalue();
  item->mw->load_config_callback(item->config);
}

void MainWindow::update_log(std::optional<std::shared_ptr<Log>> l) {
  if (l) {
    auto log = l.value();
    m_logview->SetLog(log);
    auto stop_time = l.value()->EndTime();
    auto start_time = stop_time - std::chrono::seconds{20};
    m_logview->update_time_range(start_time, stop_time);
    auto old_configs = log->LoadConfigs();

    m_file_loadconfig->flags = FL_SUBMENU;
    prev_config_menu_items.clear();
    for (const auto &conf : old_configs) {
      auto time_c = std::chrono::system_clock::to_time_t(conf.save_time);
      char timestr[64];
      std::strftime(timestr, 64, "%F %T", std::localtime(&time_c));
      std::string menu_text = conf.name + " (" + timestr + ")";

      auto item =
          new LogMenuData{.mw = this, .text = menu_text, .config = conf};
      prev_config_menu_items.push_back({item->text.c_str(), 0,
                                        select_prev_config_callback, item, 0,
                                        FL_NORMAL_LABEL, 0, 14, 0});
    }
    prev_config_menu_items.push_back({0, 0, 0, 0, 0, 0, 0, 0, 0});
    m_file_loadconfig->user_data(prev_config_menu_items.data());
    m_file_loadconfig->flags = FL_SUBMENU_POINTER;
  }
  log = l;
}

void MainWindow::update_connection_status(bool status) {
  m_connection_status->color(status ? FL_GREEN : FL_RED);
  m_connection_status->redraw();
}

void MainWindow::update_feed_hz(int hz) {
  m_rate->value(std::to_string(hz).c_str());
  m_rate->redraw();
}

void MainWindow::feed_update(std::map<std::string, viaems::FeedValue> status) {
  m_status_table->feed_update(status);
  if (!logview_paused) {
    auto stop_time = log.value()->EndTime();
    auto start_time = stop_time - std::chrono::seconds{20};
    m_logview->update_time_range(start_time, stop_time);
  } else {
    m_logview->update();
  }
}

void MainWindow::sensor_value_changed_callback(Fl_Widget *w, void *ptr) {
  auto mw = static_cast<MainWindow *>(ptr);
  auto sensor = std::get<viaems::SensorValue>(
      mw->m_model->configuration().get(mw->detail_path).value());

  sensor.pin = static_cast<uint32_t>(mw->m_sensor_pin->value());
  sensor.lag = static_cast<float>(mw->m_sensor_lag->value());

  sensor.method = mw->m_sensor_method->text();
  sensor.source = mw->m_sensor_source->text();

  sensor.range_min = mw->m_sensor_range_min->value();
  sensor.range_max = mw->m_sensor_range_max->value();

  sensor.fault.min = mw->m_sensor_fault_min->value();
  sensor.fault.max = mw->m_sensor_fault_max->value();
  sensor.fault.value = mw->m_sensor_fault_value->value();
  sensor.therm.a = mw->m_sensor_therm_A->value();
  sensor.therm.b = mw->m_sensor_therm_B->value();
  sensor.therm.c = mw->m_sensor_therm_C->value();
  sensor.therm.bias = mw->m_sensor_therm_bias->value();

  sensor.const_value = mw->m_sensor_const->value();

  mw->m_model->set_value(mw->detail_path, sensor);
  auto item = get_config_tree_widget(mw->m_config_tree, mw->detail_path);
  item->dirty(true);
}

void MainWindow::update_sensor_editor(viaems::SensorValue s) {
  auto sensor_type = m_model->configuration().types.at("sensor");

  m_sensor_method->clear();
  for (auto choice : sensor_type.map()["method"].leaf().choices) {
    m_sensor_method->add(choice.c_str());
  }
  m_sensor_method->value(m_sensor_method->find_index(s.method.c_str()));

  m_sensor_source->clear();
  for (auto choice : sensor_type.map()["source"].leaf().choices) {
    m_sensor_source->add(choice.c_str());
  }
  m_sensor_source->value(m_sensor_source->find_index(s.source.c_str()));

  if (s.source == "const") {
    m_sensor_const->activate();
  } else {
    m_sensor_const->deactivate();
  }

  if (s.source != "const" &&
      (s.method == "linear" || s.method == "linear-window")) {
    m_sensor_range_min->activate();
    m_sensor_range_max->activate();
  } else {
    m_sensor_range_min->deactivate();
    m_sensor_range_max->deactivate();
  }

  if (s.method == "therm") {
    m_sensor_therm_A->activate();
    m_sensor_therm_B->activate();
    m_sensor_therm_C->activate();
    m_sensor_therm_bias->activate();
  } else {
    m_sensor_therm_A->deactivate();
    m_sensor_therm_B->deactivate();
    m_sensor_therm_C->deactivate();
    m_sensor_therm_bias->deactivate();
  }

  if (s.method == "linear-window") {
    m_sensor_window_offset->activate();
    m_sensor_window_total->activate();
    m_sensor_window_width->activate();
  } else {
    m_sensor_window_offset->deactivate();
    m_sensor_window_total->deactivate();
    m_sensor_window_width->deactivate();
  }

  m_sensor_pin->value(s.pin);
  m_sensor_lag->value(s.lag);

  m_sensor_range_min->value(s.range_min);
  m_sensor_range_max->value(s.range_max);

  m_sensor_therm_bias->value(s.therm.bias);
  m_sensor_therm_A->value(s.therm.a);
  m_sensor_therm_B->value(s.therm.b);
  m_sensor_therm_C->value(s.therm.c);

  m_sensor_window_width->value(s.window.capture_width);
  m_sensor_window_total->value(s.window.total_width);
  m_sensor_window_offset->value(s.window.offset);

  m_sensor_fault_min->value(s.fault.min);
  m_sensor_fault_max->value(s.fault.max);
  m_sensor_fault_value->value(s.fault.value);

  m_sensor_const->value(s.const_value);

  m_table_editor_box->hide();
  m_output_editor_box->hide();
  m_sensor_editor_box->show();
}

void MainWindow::select_sensor(Fl_Widget *w, void *p) {
  auto mw = (MainWindow *)p;
  auto s = (SelectableTreeWidget *)w;

  auto value = mw->m_model->configuration().get(s->path);
  if (value) {
    mw->m_sensor_editor_box->take_focus();
    mw->update_sensor_editor(std::get<viaems::SensorValue>(value.value()));
    mw->detail_path = s->path;
  }
  const auto &name = std::get<std::string>(s->path[s->path.size() - 1]);
  mw->m_sensor_editor_box->label(name.c_str());
}

void MainWindow::output_value_changed_callback(MainWindow *mw, int index,
                                               viaems::OutputValue v) {
  viaems::StructurePath path = {"outputs", index};
  mw->m_model->set_value(path, v);
  auto item = get_config_tree_widget(mw->m_config_tree, {"outputs"});
  item->dirty(true);
}

static std::vector<viaems::OutputValue> get_model_outputs(viaems::Model *m) {
  viaems::StructurePath basepath = {"outputs"};
  std::vector<viaems::OutputValue> values;
  int index = 0;
  while (true) {
    auto thispath = basepath;
    thispath.push_back({index});
    auto value = m->configuration().get(thispath);
    if (value) {
      values.push_back(std::get<viaems::OutputValue>(value.value()));
    } else {
      break;
    }
    index += 1;
  }
  return values;
}

void MainWindow::select_output(Fl_Widget *w, void *p) {
  auto mw = (MainWindow *)p;

  auto values = get_model_outputs(mw->m_model);
  mw->m_output_editor->set_outputs(values);
  mw->detail_path = {"outputs"};

  mw->m_table_editor_box->hide();
  mw->m_sensor_editor_box->hide();
  mw->m_output_editor_box->show();
}

void MainWindow::select_table(Fl_Widget *w, void *p) {
  auto mw = (MainWindow *)p;
  auto s = (SelectableTreeWidget *)w;

  auto value = mw->m_model->configuration().get(s->path);
  if (value) {
    mw->m_table_editor->take_focus();
    mw->update_table_editor(std::get<viaems::TableValue>(value.value()));
    mw->detail_path = s->path;
  }
  const auto &name = std::get<std::string>(s->path[s->path.size() - 1]);
  mw->m_table_editor_box->label(name.c_str());
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
  m_output_editor_box->hide();
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
        auto value =
            m_model->configuration().get(leaf.path).value_or(uint32_t{0});
        SelectableTreeWidget *w;

        if (leaf.type == "uint32") {
          w = new NumericTreeWidget<uint32_t>(0, 0, 300, 18, leaf.path);
          w->update_value(value);
          w->callback(structure_value_update_callback, this);
        } else if (leaf.type == "float") {
          w = new NumericTreeWidget<float>(0, 0, 300, 18, leaf.path);
          w->update_value(value);
          w->callback(structure_value_update_callback, this);
        } else if (leaf.type == "string") {
          w = new ChoiceTreeWidget(0, 0, 300, 18, leaf.path, leaf.choices);
          w->update_value(value);
          w->callback(structure_value_update_callback, this);
        } else if (leaf.type == "table") {
          w = new SelectableTreeWidget(0, 0, 300, 18, leaf.path);
          w->select_callback(select_table, this);
        } else if (leaf.type == "sensor") {
          w = new SelectableTreeWidget(0, 0, 300, 18, leaf.path);
          w->select_callback(select_sensor, this);
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
      if (child.is_leaf()) {
        auto leaf = child.leaf();
        auto w = new SelectableTreeWidget(0, 0, 300, 18, leaf.path);
        item->widget(w);
      } else {
        add_config_structure_entry(item, child);
      }
      index += 1;
    }
  }
}

void MainWindow::update_config_structure(viaems::StructureNode top) {
  m_config_tree->showroot(0);
  m_config_tree->selectmode(FL_TREE_SELECT_NONE);
  auto root = m_config_tree->root();
  if (!top.is_map()) {
    return;
  }
  m_config_tree->begin();
  for (auto child : top.map()) {
    auto item = new Fl_Tree_Item(m_config_tree->prefs());
    item->label(child.first.c_str());
    root->add(m_config_tree->prefs(), "", item);
    if (child.first == "outputs") {
      /* Special handling for outputs, present the list as a single item */
      auto w = new SelectableTreeWidget(0, 0, 300, 18, {"outputs"});
      w->select_callback(select_output, this);
      item->widget(w);
    } else {
      add_config_structure_entry(item, child.second);
    }
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
  update_config_structure(model->configuration().structure);
}

void MainWindow::update_config_value(viaems::StructurePath path,
                                     viaems::ConfigValue value) {
  if (std::get<std::string>(path.at(0)) == "outputs") {
    path = {"outputs"};
  }

  auto w = get_config_tree_widget(m_config_tree, path);
  if (w == nullptr) {
    return;
  }

  w->update_value(value);

  if (path == detail_path) {
    if (std::holds_alternative<viaems::TableValue>(value)) {
      update_table_editor(std::get<viaems::TableValue>(value));
    } else if (std::holds_alternative<viaems::SensorValue>(value)) {
      update_sensor_editor(std::get<viaems::SensorValue>(value));
    } else if (std::holds_alternative<viaems::OutputValue>(value)) {
      m_output_editor->set_outputs(get_model_outputs(m_model));
    }
  }
}
