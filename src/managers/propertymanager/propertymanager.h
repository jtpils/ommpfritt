#pragma once

#include <vector>
#include <functional>

#include "properties/hasproperties.h"
#include "managers/manager.h"
#include "scene/scene.h"

class QTabWidget;

namespace omm
{

class PropertyView;

class PropertyManager : public Manager, public AbstractSelectionObserver
{
  Q_OBJECT
  DECLARE_MANAGER_TYPE(PropertyManager)

public:
  explicit PropertyManager(Scene& scene);
  ~PropertyManager();

  void set_selection(const std::set<HasProperties*>& selection) override;
  PropertyView property(const HasProperties::Key& key);
  void clear();

private:
  QTabWidget& m_tabs;
};

}  // namespace omm
