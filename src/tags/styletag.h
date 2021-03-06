#pragma once

#include <memory>
#include "tags/tag.h"
#include <Qt>

namespace omm {

class StyleTag : public Tag
{
public:
  explicit StyleTag(Object& owner);
  std::string type() const override;
  QIcon icon() const override;
  static constexpr auto STYLE_REFERENCE_PROPERTY_KEY = "style";
  static constexpr auto TYPE = QT_TRANSLATE_NOOP("any-context", "StyleTag");
  std::unique_ptr<Tag> clone() const override;
  void evaluate() override;
  Flag flags() const override;
};

}  // namespace omm
