#include "objects/cloner.h"

#include <QObject>

#include "properties/integerproperty.h"
#include "properties/stringproperty.h"
#include "properties/optionsproperty.h"
#include "properties/floatproperty.h"
#include "properties/vectorproperty.h"
#include "properties/referenceproperty.h"
#include "properties/boolproperty.h"
#include "python/scenewrapper.h"
#include "python/objectwrapper.h"
#include "python/pythonengine.h"
#include "objects/empty.h"

namespace omm
{

class Style;

Cloner::Cloner(Scene* scene) : Object(scene)
{
  auto& mode_property = add_property<OptionsProperty>(MODE_PROPERTY_KEY);
  mode_property.set_options({ "Linear", "Grid", "Radial", "Path", "Script" })
    .set_label(QObject::tr("mode").toStdString())
    .set_category(QObject::tr("Cloner").toStdString());

  add_property<IntegerProperty>(COUNT_PROPERTY_KEY, 3)
    .set_label(QObject::tr("count").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Linear, Mode::Radial,                                        Mode::Path, Mode::Script });

  add_property<IntegerVectorProperty>(COUNT_2D_PROPERTY_KEY, arma::ivec2{3, 3})
    .set_label(QObject::tr("count").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Grid });

  add_property<FloatProperty>(DISTANCE_PROPERTY_KEY, 10.0)
    .set_label(QObject::tr("distance").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Linear });

  add_property<FloatVectorProperty>(DISTANCE_2D_PROPERTY_KEY, arma::vec2{10.0, 10.0})
    .set_label(QObject::tr("distance").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Grid });

  add_property<FloatProperty>(RADIUS_PROPERTY_KEY, 0.0)
    .set_label(QObject::tr("radius").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Radial });

  add_property<ReferenceProperty>(PATH_REFERENCE_PROPERTY_KEY)
    .set_allowed_kinds(Kind::Object)
    .set_label(QObject::tr("path").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Path });

  add_property<FloatProperty>(START_PROPERTY_KEY, 0.0)
    .set_label(QObject::tr("start").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Radial, Mode::Path });

  add_property<FloatProperty>(END_PROPERTY_KEY, 1.0)
    .set_label(QObject::tr("end").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Radial, Mode::Path });

  add_property<BoolProperty>(ALIGN_PROPERTY_KEY, true)
    .set_label(QObject::tr("align").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Radial, Mode::Path });

  add_property<OptionsProperty>(BORDER_PROPERTY_KEY)
    .set_options( { "Clamp", "Wrap", "Hide", "Reflect" } )
    .set_label(QObject::tr("border").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Radial, Mode::Path });

  add_property<StringProperty>(CODE_PROPERTY_KEY, "")
    .set_mode(StringProperty::Mode::MultiLine)
    .set_label(QObject::tr("code").toStdString())
    .set_category(QObject::tr("Cloner").toStdString())
    .set_enabled_buddy<Mode>(mode_property, { Mode::Script });
}


void Cloner::render(AbstractRenderer& renderer, const Style& style)
{
  if (is_active()) {
    assert(&renderer.scene == scene());
    for (auto&& clone : make_clones()) {
      renderer.push_transformation(clone->transformation());
      clone->render_recursive(renderer, style);
      renderer.pop_transformation();
    }
    m_draw_children = false;
  } else {
    m_draw_children = true;
  }
}

BoundingBox Cloner::bounding_box()
{
  return BoundingBox();  // TODO
}

Cloner::Mode Cloner::mode() const { return property(MODE_PROPERTY_KEY).value<Mode>(); }
std::string Cloner::type() const { return TYPE; }
std::unique_ptr<Object> Cloner::clone() const { return std::make_unique<Cloner>(*this); }
Object::Flag Cloner::flags() const { return Object::flags() | Flag::Convertable; }

std::unique_ptr<Object> Cloner::convert()
{
  auto converted = std::make_unique<Empty>(scene());
  copy_properties(*converted);
  copy_tags(*converted);

  auto clones = make_clones();
  for (std::size_t i = 0; i < clones.size(); ++i) {
    const auto local_transformation = clones[i]->transformation();
    auto& clone = converted->adopt(std::move(clones[i]));
    const std::string name = clone.name() + " " + std::to_string(i);
    clone.property(NAME_PROPERTY_KEY).set(name);
    clone.set_transformation(local_transformation);
  }

  return converted;
}

std::vector<std::unique_ptr<Object>> Cloner::make_clones()
{
  const auto count = [this]() -> int {
    switch (mode()) {
    case Mode::Linear:
    case Mode::Radial:
    case Mode::Path:
    case Mode::Script:
      return property(COUNT_PROPERTY_KEY).value<int>();
    case Mode::Grid:
    {
      const auto c = property(COUNT_2D_PROPERTY_KEY).value<VectorPropertyValueType<arma::ivec2>>();
      return c(0) * c(1);
    }
    }
  };

  auto clones = copy_children(count());
  for (std::size_t i = 0; i < clones.size(); ++i) {
    switch (mode()) {
    case Mode::Linear: set_linear(*clones[i], i); break;
    case Mode::Radial: set_radial(*clones[i], i); break;
    case Mode::Path: set_path(*clones[i], i); break;
    case Mode::Script: set_by_script(*clones[i], i); break;
    case Mode::Grid: set_grid(*clones[i], i); break;
    }
  }

  return clones;
}

std::vector<std::unique_ptr<Object>> Cloner::copy_children(const int count)
{
  const auto n_children = this->n_children();
  std::vector<std::unique_ptr<Object>> clones;
  if (n_children > 0 && count > 0) {
    clones.reserve(count);
    for (int i = 0; i < count; ++i) {
      clones.push_back(child(i % n_children).clone());
    }
  }
  return clones;
}

double Cloner::get_t(std::size_t i, const bool inclusive) const
{
  const auto n = property(COUNT_PROPERTY_KEY).value<int>() + (inclusive ? 0 : 1);
  const auto start = property(START_PROPERTY_KEY).value<double>();
  const auto end = property(END_PROPERTY_KEY).value<double>();
  const auto border = property(BORDER_PROPERTY_KEY).value<Border>();

  if (n <= 1) {
    return 0.0;
  } else {
    const auto spacing =  (end - start) / (n-1);
    return apply_border(start + spacing * i, border);
  };
}

void Cloner::set_linear(Object& object, std::size_t i)
{
  const double x = i * property(DISTANCE_PROPERTY_KEY).value<double>();
  auto t = object.transformation();
  t.set_translation(arma::vec2{x, 0.0});
  object.set_transformation(t);
}

void Cloner::set_grid(Object& object, std::size_t i)
{
  const auto n = property(COUNT_2D_PROPERTY_KEY).value<VectorPropertyValueType<arma::ivec2>>();
  const auto v = property(DISTANCE_2D_PROPERTY_KEY).value<VectorPropertyValueType<arma::vec2>>();
  auto t = object.transformation();
  t.set_translation({ v(0) * (i % n(0)), v(1) * (i / n(0)) });
  object.set_transformation(t);
}

void Cloner::set_radial(Object& object, std::size_t i)
{
  const double angle = 2*M_PI * get_t(i, false);
  const double r = property(RADIUS_PROPERTY_KEY).value<double>();
  const OrientedPoint op({std::cos(angle) * r, std::sin(angle) * r}, angle + M_PI/2.0);
  object.set_oriented_position(op, property(ALIGN_PROPERTY_KEY).value<bool>());
}

void Cloner::set_path(Object& object, std::size_t i)
{
  auto* o = property(PATH_REFERENCE_PROPERTY_KEY).value<AbstractPropertyOwner*>();

  const double t_start = property(START_PROPERTY_KEY).value<double>();
  const double t_end = property(END_PROPERTY_KEY).value<double>();
  const bool align = property(ALIGN_PROPERTY_KEY).value<bool>();
  object.set_position_on_path(o, align, get_t(i, true));
}

void Cloner::set_by_script(Object& object, std::size_t i)
{
  using namespace pybind11::literals;
  const auto locals = pybind11::dict( "id"_a=i,
                                      "count"_a=property(COUNT_PROPERTY_KEY).value<int>(),
                                      "copy"_a=ObjectWrapper::make(object),
                                      "this"_a=ObjectWrapper::make(*this),
                                      "scene"_a=SceneWrapper(*scene()) );
  scene()->python_engine.run(property(CODE_PROPERTY_KEY).value<std::string>(), locals);
}


}  // namespace omm
