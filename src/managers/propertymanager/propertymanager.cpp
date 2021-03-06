#include "managers/propertymanager/propertymanager.h"

#include <algorithm>
#include <set>
#include <QTabWidget>
#include <QTimer>
#include <QCoreApplication>
#include <QPushButton>

#include "properties/optionsproperty.h"
#include "managers/propertymanager/propertymanagertab.h"
#include "propertywidgets/propertywidget.h"
#include "aspects/propertyowner.h"
#include "common.h"
#include "menuhelper.h"
#include "logging.h"

namespace
{

std::vector<std::string>
get_key_intersection(const std::set<omm::AbstractPropertyOwner*>& selection)
{
  if (selection.size() == 0) {
    return std::vector<std::string>();
  }

  const auto* the_entity = *selection.begin();
  auto keys = the_entity->properties().keys();
  std::unordered_map<std::string, omm::Property*> the_properties;
  for (auto&& key : keys) {
    the_properties.insert(std::make_pair(key, the_entity->property(key)));
  }

  for (auto it = std::next(selection.begin()); it != selection.end(); ++it) {
    const auto has_key_of_same_type = [&](const std::string& key) {
      auto&& property_keys = (*it)->properties().keys();
      if (std::find(property_keys.begin(), property_keys.end(), key) == property_keys.end()) {
        return false;
      } else {
        return the_properties.at(key)->is_compatible(*(*it)->property(key));
      }
    };

    const auto sr = std::remove_if(keys.begin(), keys.end(), std::not_fn(has_key_of_same_type));
    keys.erase(sr, keys.end());
  }

  return keys;
}

auto collect_properties( const std::string& key,
                         const std::set<omm::AbstractPropertyOwner*>& selection )
{
  std::set<omm::AbstractPropertyOwner*> collection;
  const auto f = [key](omm::AbstractPropertyOwner* entity) {
    return entity->property(key);
  };

  return transform<omm::Property*>(selection, f);
}

std::string get_tab_label(const std::set<omm::Property*>& properties)
{
  assert(properties.size() > 0);
  const auto tab_label = (*properties.begin())->category();
#ifndef NDEBUG
  for (auto&& property : properties) {
    assert(property != nullptr);
    if (tab_label != property->category()) {
      LWARNING << "category is not consistent: '" << tab_label
                   << "' != '" << property->category() << "'.";
    }
  }
#endif
  return tab_label;
}

size_t find_tab_label(const std::string& label, const std::vector<std::string>& labels)
{
  const auto it = std::find(labels.cbegin(), labels.cend(), label);
  assert(it != labels.cend());
  return std::distance(labels.cbegin(), it);
}

}  // namespace

namespace omm
{

PropertyManager::PropertyManager(Scene& scene)
  : Manager(QCoreApplication::translate("any-context", "Properties"), scene, make_menu_bar())
{
  auto tabs = std::make_unique<QTabWidget>();
  m_tabs = tabs.get();
  set_widget(std::move(tabs));
  setWindowTitle(QString::fromStdString(make_window_title()));
  setObjectName(TYPE);
  connect(m_tabs, &QTabWidget::currentChanged, [this](int index) {
    if (index >= 0) {
      m_active_category = m_tabs->tabText(index).toStdString();
    }
  });

  connect(&scene, SIGNAL(selection_changed(std::set<AbstractPropertyOwner*>)),
          this, SLOT(set_selection(std::set<AbstractPropertyOwner*>)));
}

PropertyManager::~PropertyManager()
{
  clear();
}

std::unique_ptr<QWidget> PropertyManager::make_menu_bar()
{
  auto menu_bar = std::make_unique<QMenuBar>();
  auto user_properties_menu = menu_bar->addMenu(QObject::tr("user properties", "PropertyManager"));
  const auto exec_user_property_dialog = [this]() {
    auto dialog = UserPropertyDialog(this, **m_current_selection.begin());
    if (dialog.exec() == QDialog::Accepted) {
      m_scene.submit(dialog.make_user_property_config_command());
      m_scene.set_selection(m_scene.selection());
    }
  };
  m_manage_user_properties_action = &action( *user_properties_menu,
                                             QObject::tr("edit ...", "PropertyManager"),
                                             exec_user_property_dialog );
  m_manage_user_properties_action->setEnabled(false);

  auto lock_button = std::make_unique<QPushButton>();
  lock_button->setFixedSize(24, 24);
  lock_button->setText("L");
  lock_button->setCheckable(true);
  connect(lock_button.get(), &QPushButton::toggled, [this](bool checked) { set_locked(checked); });

  auto container = std::make_unique<QWidget>();
  auto layout = std::make_unique<QHBoxLayout>();
  layout->addWidget(menu_bar.release());
  layout->addStretch();
  layout->addWidget(lock_button.release());
  container->setLayout(layout.release());
  return container;
}

void PropertyManager::set_selection(const std::set<AbstractPropertyOwner*>& selection)
{
  if (m_is_locked) {
    return;
  }

  clear();
  OrderedMap<std::string, PropertyManagerTab> tabs;

  for (const auto& key : get_key_intersection(selection)) {
    const auto properties = collect_properties(key, selection);
    assert(properties.size() > 0);
    const auto tab_label = get_tab_label(properties);
    if (!tabs.contains(tab_label)) {
      tabs.insert(tab_label, std::make_unique<PropertyManagerTab>());
    }
    if (Property::get_value<bool>(properties, std::mem_fn(&Property::is_enabled))) {
      tabs.at(tab_label)->add_properties(m_scene, key, properties);
    }
    for (Property* property : properties) {
      auto* enabled_buddy = property->enabled_buddy();
      if (enabled_buddy != nullptr && !::contains(m_observed_properties, enabled_buddy)) {
        enabled_buddy->Observed<AbstractPropertyObserver>::register_observer(this);
        m_observed_properties.insert(enabled_buddy);
      }
    }
  }

  const auto active_category = m_active_category;
  for (auto&& tab_label : tabs.keys()) {
    auto& tab = tabs.at(tab_label);
    tab->end_add_properties();
    m_tabs->addTab(tab.release(), QString::fromStdString(tab_label));
  }

  if (tabs.contains(active_category)) {
    m_tabs->setCurrentIndex(find_tab_label(active_category, tabs.keys()));
  }

  m_current_selection = selection;
  m_manage_user_properties_action->setEnabled(m_current_selection.size() == 1);
  setWindowTitle(QString::fromStdString(make_window_title()));
}

void PropertyManager::set_locked(bool locked) { m_is_locked = locked; }

void PropertyManager::clear()
{
  for (auto* observed_property : m_observed_properties) {
    observed_property->Observed<AbstractPropertyObserver>::unregister_observer(this);
  }
  m_observed_properties.clear();

  const auto active_category = m_active_category;
  while (m_tabs->count() > 0) {
    m_tabs->widget(0)->deleteLater();
    m_tabs->removeTab(0);
  }
  m_active_category = active_category;
}

void PropertyManager::on_property_value_changed(Property&, std::set<const void *> trace)
{
  // As  (A) the current widgets will be deleted in `set_selection`
  // and (B) the widget of `property` still has pending events, it's not wise to call
  // `set_selection` directly. Instead, wait until all events in the Qt event queue are handled.
  QTimer::singleShot(0, [this](){ set_selection(m_current_selection); });
}

std::string PropertyManager::type() const { return TYPE; }

std::string PropertyManager::make_window_title() const
{
  std::ostringstream ss;
  ss << QObject::tr("property manager", "PropertyManager").toStdString();
  for (auto&& selected : m_current_selection) {
    ss << " " << selected->name();
  }
  return ss.str();
}

}  // namespace omm
