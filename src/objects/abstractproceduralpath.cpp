#include "objects/abstractproceduralpath.h"
#include <QObject>
#include "properties/floatproperty.h"
#include "objects/path.h"
#include "common.h"
#include "geometry/cubics.h"

namespace omm
{

class Style;

void AbstractProceduralPath::draw_object(Painter &renderer, const Style& style) const
{
  if (QPainter* painter = renderer.painter; painter != nullptr && is_active()) {
    renderer.set_style(style);
    painter->drawPath(m_painter_path);
  }
}

AbstractPropertyOwner::Flag AbstractProceduralPath::flags() const
{
  return Object::flags() | Flag::Convertable | Flag::IsPathLike;
}

std::unique_ptr<Object> AbstractProceduralPath::convert() const
{
  auto converted = std::make_unique<Path>(scene());
  copy_properties(*converted);
  copy_tags(*converted);
  converted->property(Path::IS_CLOSED_PROPERTY_KEY)->set(is_closed());
  converted->property(Path::INTERPOLATION_PROPERTY_KEY)->set(Path::InterpolationMode::Bezier);
  converted->set_points(m_points);
  return std::unique_ptr<Object>(converted.release());
}

Point AbstractProceduralPath::evaluate(const double t) const
{
  return Cubics(m_points, is_closed()).evaluate(t);
}

BoundingBox AbstractProceduralPath::bounding_box() const { return BoundingBox(m_points); }
double AbstractProceduralPath::path_length() const
{
  return Cubics(m_points, is_closed()).length();
}

bool AbstractProceduralPath::contains(const Vec2f &pos) const
{
  return Cubics(m_points, is_closed()).contains(pos);
}

std::vector<double> AbstractProceduralPath::cut(const Vec2f& c_start, const Vec2f& c_end)
{
  const auto gti = global_transformation().inverted();
  return Cubics(m_points, is_closed()).cut( gti.apply_to_position(c_start),
                                            gti.apply_to_position(c_end) );
}

Object::PathUniquePtr AbstractProceduralPath::outline(const double t) const
{
  auto outline = std::make_unique<Path>(scene());
  outline->set_points(Point::offset(t, m_points, is_closed()));
  outline->property(Path::IS_CLOSED_PROPERTY_KEY)->set(is_closed());
  return Object::PathUniquePtr(outline.release());
}

void AbstractProceduralPath::update()
{
  m_points = points();
  m_painter_path = Painter::path(m_points, is_closed());
}


}  // namespace omm
