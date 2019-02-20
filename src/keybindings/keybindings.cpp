#include "keybindings/keybindings.h"
#include <QSettings>
#include "keybindings/commandinterface.h"
#include <glog/logging.h>
#include <QKeyEvent>
#include "common.h"
#include <map>
#include <list>
#include "keybindings/action.h"
#include <memory>
#include <QMenu>
#include "mainwindow/application.h"
#include "managers/stylemanager/stylemanager.h"
#include "managers/objectmanager/objectmanager.h"
#include "managers/pythonconsole/pythonconsole.h"

namespace
{

QString settings_key(const omm::KeyBinding& binding)
{
  return QString::fromStdString(binding.context()) + "/" + QString::fromStdString(binding.name());
}

template<typename CommandInterfaceT>
void collect_default_bindings(std::list<omm::KeyBinding>& bindings)
{
  for (const auto& [ name, key_sequence ] : CommandInterfaceT::default_bindings()) {
    bindings.push_back(omm::KeyBinding(name, CommandInterfaceT::TYPE, key_sequence));
  }
}

std::vector<omm::KeyBinding> collect_default_bindings()
{
  std::list<omm::KeyBinding> default_bindings;
  collect_default_bindings<omm::Application>(default_bindings);
  collect_default_bindings<omm::StyleManager>(default_bindings);
  collect_default_bindings<omm::ObjectManager>(default_bindings);
  collect_default_bindings<omm::PythonConsole>(default_bindings);
  return std::vector(default_bindings.begin(), default_bindings.end());
}

std::pair<std::string, std::string> split(const std::string& path)
{
  constexpr auto separator = '/';
  const auto it = std::find(path.rbegin(), path.rend(), separator);
  if (it == path.rend()) {
    return { "", path };
  } else {
    const auto i = std::distance(it, path.rend());
    return { path.substr(0, i-1), path.substr(i) };
  }
}

std::unique_ptr<QMenu> add_menu(const std::string& path, std::map<std::string, QMenu*>& menu_map)
{
  if (menu_map.count(path) > 0) {
    return nullptr;
  } else {
    const auto [ rest_path, menu_name ] = split(path);
    auto menu = std::make_unique<QMenu>(QString::fromStdString(menu_name));
    menu_map.insert({ path, menu.get() });

    if (rest_path.empty()) {
      return menu;  // menu is top-level and did not exist before.
    } else {
      auto top_level_menu = add_menu(rest_path, menu_map);
      assert(menu_map.count(rest_path) > 0);
      menu_map[rest_path]->addMenu(menu.release());
      return top_level_menu;  // may be nullptr if top_level_menu already existed before.
    }
  }
}

}  // namespace

namespace omm
{

KeyBindings::KeyBindings()
  : m_bindings(collect_default_bindings())
{
  restore();
  m_reset_timer.setSingleShot(true);
  connect(&m_reset_timer, &QTimer::timeout, [this]() {
    m_current_sequene.clear();
  });
}

KeyBindings::~KeyBindings() { store(); }

void KeyBindings::restore()
{
  QSettings settings;
  settings.beginGroup(settings_group);
  for (auto& key_binding : m_bindings) {
    if (const auto key = settings_key(key_binding); settings.contains(key)) {
      const auto sequence = settings.value(key).toString();
      key_binding.set_key_sequence(QKeySequence(sequence, QKeySequence::PortableText));
    }
  }
  settings.endGroup();
}

void KeyBindings::store() const
{
  QSettings settings;
  settings.beginGroup(settings_group);
  for (const auto& key_binding : m_bindings) {
    const auto sequence = key_binding.key_sequence().toString(QKeySequence::PortableText);
    settings.setValue(settings_key(key_binding), sequence);
  }
  settings.endGroup();
}

bool KeyBindings::call(const QKeySequence& sequence, CommandInterface& interface) const
{
  const auto context = interface.type();

  const auto is_match = [sequence, context](const auto& binding) {
    return binding.matches(sequence, context);
  };

  const auto it = std::find_if(m_bindings.begin(), m_bindings.end(), is_match);
  if (it != m_bindings.end()) {
    interface.call(it->name());
    return true;
  } else if (&interface != m_global_command_interface && m_global_command_interface != nullptr) {
    call(sequence, *m_global_command_interface);
    return true;
  } else {
    return false;
  }
}

QKeySequence KeyBindings::make_key_sequence(const QKeyEvent& event) const
{
  static constexpr std::size_t MAX_SEQUENCE_LENGTH = 4;  // must be 4 to match QKeySequence impl.
  const int code = event.key() | event.modifiers();
  m_current_sequene.push_back(code);
  if (m_current_sequene.size() > MAX_SEQUENCE_LENGTH) { m_current_sequene.pop_front(); }

  std::vector sequence(m_current_sequene.begin(), m_current_sequene.end());
  sequence.reserve(MAX_SEQUENCE_LENGTH);
  while (sequence.size() < MAX_SEQUENCE_LENGTH) { sequence.push_back(0); }

  return QKeySequence(sequence[0], sequence[1], sequence[2], sequence[3]);
}

bool KeyBindings::call(const QKeyEvent& key_event, CommandInterface& interface) const
{
  m_reset_timer.start(m_reset_delay);
  // QKeySequence does not support combinations without non-modifier keys.
  static const auto bad_keys = std::set { Qt::Key_unknown, Qt::Key_Shift, Qt::Key_Control,
                                          Qt::Key_Meta, Qt::Key_Alt };

  if (::contains(bad_keys, key_event.key())) {
    return false;
  } if (key_event.key() == Qt::Key_Escape) {
    m_current_sequene.clear();
    return false;
  } else if (const auto sequence = make_key_sequence(key_event); call(sequence, interface)) {
    m_current_sequene.clear();
    return true;
  } else {
    LOG(INFO) << "key sequence was not (yet) accepted: " << sequence.toString().toStdString();
    return false;
  }
}
int KeyBindings::columnCount(const QModelIndex& parent) const { return 3; }
int KeyBindings::rowCount(const QModelIndex& parent) const { return m_bindings.size(); }
QVariant KeyBindings::data(const QModelIndex& index, int role) const
{
  assert(index.isValid());
  const auto& binding = m_bindings[index.row()];
  switch (role) {
  case Qt::DisplayRole:
    switch (index.column()) {
    case NAME_COLUMN:
      return QString::fromStdString(binding.name());
    case CONTEXT_COLUMN:
      return QString::fromStdString(binding.context());
    case SEQUENCE_COLUMN:
      return binding.key_sequence().toString(QKeySequence::PortableText);
    }
    break;
  case Qt::EditRole:
    switch (index.column()) {
    case SEQUENCE_COLUMN:
      return binding.key_sequence();
    }
    break;
  }
  return QVariant();
}

bool KeyBindings::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (role != Qt::EditRole) { return false; }
  assert(index.column() == SEQUENCE_COLUMN);
  if (value.canConvert<QKeySequence>()) {
    m_bindings[index.row()].set_key_sequence(value.value<QKeySequence>());
    return true;
  }  else {
    return false;
  }
}

QVariant KeyBindings::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal) {
    switch (role) {
    case Qt::DisplayRole:
      switch (section) {
      case NAME_COLUMN: return tr("name");
      case CONTEXT_COLUMN: return tr("context");
      case SEQUENCE_COLUMN: return tr("sequence");
      }
    }
    return QVariant();
  } else {
    return QVariant();
  }
}


Qt::ItemFlags KeyBindings::flags(const QModelIndex& index) const
{
  Qt::ItemFlags flags = Qt::ItemIsEnabled;
  if (index.column() == SEQUENCE_COLUMN) { flags |= Qt::ItemIsEditable; }
  return flags;
}

void KeyBindings::set_global_command_interface(CommandInterface& global_command_interface)
{
  m_global_command_interface = &global_command_interface;
}

std::unique_ptr<QAction>
KeyBindings::make_action(CommandInterface& ci, const std::string& action_name) const
{
  auto label = QString::fromStdString(action_name);
  const auto it = std::find_if(m_bindings.begin(), m_bindings.end(), [&](const auto& binding) {
    return binding.name() == action_name && binding.context() == ci.type();
  });
  if (it == m_bindings.end()) {
    LOG(ERROR) << "Failed to find keybindings for '" << ci.type() << "::" << action_name << "'.";
    return nullptr;
  } else {
    auto action = std::make_unique<Action>(*it);
    QObject::connect(action.get(), &QAction::triggered, [&ci, action_name] {
      ci.call(action_name);
    });
    return action;
  }
}

std::vector<std::unique_ptr<QMenu>>
KeyBindings::make_menus(CommandInterface& ci, const std::vector<std::string>& actions) const
{
  std::list<std::unique_ptr<QMenu>> menus;
  std::map<std::string, QMenu*> menu_map;
  for (const auto& action_path : actions) {
    const auto [path, action_name] = split(action_path);
    auto menu = ::add_menu(path, menu_map);
    if (menu) { menus.push_back(std::move(menu)); }
    assert(menu_map.count(path) > 0);
    if (action_name == SEPARATOR) {
      menu_map[path]->addSeparator();
    } else if (!action_name.empty()) {
      menu_map[path]->addAction(make_action(ci, action_name).release());
    }
  }
  return std::vector(std::make_move_iterator(menus.begin()), std::make_move_iterator(menus.end()));
}

}  // namespace omm