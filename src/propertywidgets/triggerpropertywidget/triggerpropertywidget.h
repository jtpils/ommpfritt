#pragma once

#include "propertywidgets/propertywidget.h"
#include "properties/triggerproperty.h"

namespace omm
{

class TriggerPropertyWidget : public PropertyWidget<TriggerProperty>
{
public:
  explicit TriggerPropertyWidget(Scene& scene, const std::set<Property*>& properties);
  void trigger();

protected:
  std::string type() const override;
  void update_edit() override;
};

}  // namespace omm
