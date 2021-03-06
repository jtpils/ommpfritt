#pragma once

#include "geometry/boundingbox.h"
#include "geometry/vec2.h"
#include "geometry/rectangle.h"

class QRectF;

namespace omm
{

class BoundingBox : public Rectangle
{
public:
  explicit BoundingBox(const std::vector<Vec2f>& points = { Vec2f::o() });
  explicit BoundingBox(const std::vector<double>& xs, const std::vector<double>& ys);
  explicit BoundingBox(const std::vector<Point>& points);

  bool contains(const BoundingBox& other) const;
  using Rectangle::contains;

  BoundingBox& operator |=(const BoundingBox& other);
  BoundingBox& operator &=(const BoundingBox& other);

};

std::ostream& operator<<(std::ostream& ostream, const BoundingBox& bb);
BoundingBox operator|(const BoundingBox& a, const BoundingBox& b);
BoundingBox operator&(const BoundingBox& a, const BoundingBox& b);

}  // namespace omm
