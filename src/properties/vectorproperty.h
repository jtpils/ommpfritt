#pragma once

#include "properties/numericproperty.h"
#include <Qt>

namespace omm
{

struct FloatVectorPropertyLimits
{
  static const Vec2f lower;
  static const Vec2f upper;
  static const Vec2f step;
};

class FloatVectorProperty
  : public NumericProperty<Vec2f, FloatVectorPropertyLimits>
{
public:
  using NumericProperty::NumericProperty;
  std::string type() const override;
  void deserialize(AbstractDeserializer& deserializer, const Pointer& root) override;
  void serialize(AbstractSerializer& serializer, const Pointer& root) const override;
  static constexpr auto TYPE = QT_TRANSLATE_NOOP("FloatVectorProperty", "FloatVectorProperty");
  std::unique_ptr<Property> clone() const override;
};

struct IntegerVectorPropertyLimits
{
  static const Vec2i lower;
  static const Vec2i upper;
  static const Vec2i step;
};

class IntegerVectorProperty
  : public NumericProperty<Vec2i, IntegerVectorPropertyLimits>
{
public:
  using NumericProperty::NumericProperty;
  std::string type() const override;
  void deserialize(AbstractDeserializer& deserializer, const Pointer& root) override;
  void serialize(AbstractSerializer& serializer, const Pointer& root) const override;
  static constexpr auto TYPE = QT_TRANSLATE_NOOP("IntegerVectorProperty", "IntegerVectorProperty");
  std::unique_ptr<Property> clone() const override;
};

}  // namespace omm
