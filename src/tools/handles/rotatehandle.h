#pragma once

#include "tools/handles/handle.h"

namespace omm
{

template<typename ToolT>
class RotateHandle : public Handle
{
public:
  RotateHandle(ToolT& tool) : Handle(tool, true)
  {
    set_style(Status::Active, omm::ContourStyle(omm::Color(1.0, 1.0, 1.0)));
    set_style(Status::Hovered, omm::ContourStyle(omm::Color(0.0, 0.0, 1.0)));
    set_style(Status::Inactive, omm::ContourStyle(omm::Color(0.3, 0.3, 1.0)));
  }

  void draw(Painter& renderer) const override
  {
    renderer.set_style(current_style());
    renderer.painter->drawEllipse(-RADIUS, -RADIUS, 2*RADIUS, 2*RADIUS);
  }

  bool contains_global(const Vec2f& point) const override
  {
    const auto global_point = transform_position_to_global(point);
    const double r = global_point.euclidean_norm();
    return RADIUS - interact_epsilon() <= r && r <= RADIUS + interact_epsilon();
  }

  bool mouse_move(const Vec2f& delta, const Vec2f& pos, const QMouseEvent& e) override
  {
    Handle::mouse_move(delta, pos, e);
    if (status() == Status::Active) {
      const auto global_pos = transformation().inverted().apply_to_position(pos);
      const auto origin = transformation().inverted().apply_to_position(press_pos());
      const auto delta = global_pos - origin;

      double angle = global_pos.arg() - origin.arg();
      if (tool.integer_transformation()) {
        static constexpr double step = 15 * M_PI / 180.0;
        angle = step * static_cast<int>(angle / step);
      }
      const auto t = omm::ObjectTransformation().rotated(angle);
      static_cast<ToolT&>(tool).transform_objects_absolute(t, transform_in_tool_space);
      static_cast<ToolT&>(tool).tool_info = QString("%1°").arg(angle / M_PI * 180.0).toStdString();
      return true;
    } else {
      return false;
    }
  }

private:
  static constexpr double RADIUS = 100;
};

}  // namespace omm
