#include "tools/scaletool.h"
#include <memory>
#include "tools/handles/axishandle.h"
#include "scene/scene.h"
#include "commands/objectstransformationcommand.h"

namespace
{

arma::vec2 get_scale(const arma::vec2& pos, const arma::vec2& delta, const arma::vec2& direction)
{
  const auto factor = [](const double new_pos, const double delta, const double constraint) {
    const double old_pos = new_pos - delta;
    const double epsilon = 0.0001;
    if (std::abs(old_pos) < epsilon) {
      return 1.0;
    } else {
      double s = new_pos / old_pos;
      s = std::copysign(std::pow(std::abs(s), std::abs(constraint)), s);
      return s;
      if (std::abs(s) < 0.1) {
        return std::copysign(1.0, s);
      } else {
        return s;
      }
    }
  };
  const arma::vec old_pos = pos - delta;
  const arma::vec d = direction / arma::norm(direction);
  return arma::vec({ factor(pos(0), delta(0), d(0)), factor(pos(1), delta(1), d(1)) });
}

class ScaleAxisHandle : public omm::AxisHandle
{
public:
  ScaleAxisHandle(omm::ScaleTool& tool) : m_tool(tool) { }
  void mouse_move(const arma::vec2& delta, const arma::vec2& pos, const bool allow_hover) override
  {
    Handle::mouse_move(delta, pos, allow_hover);
    if (status() == Status::Active) {
      const auto t = omm::ObjectTransformation().scaled(get_scale(pos, delta, m_direction));
      m_tool.transform_objects(t);
    }
  }

  void draw(omm::AbstractRenderer& renderer) const override
  {
    const omm::Point o(arma::vec2{ 0.0, 0.0 });
    const omm::Point tip(m_direction);
    const double r = 3.0;
    const omm::Point tl(m_direction + arma::vec2{-r,  r });
    const omm::Point bl(m_direction + arma::vec2{ r,  r });
    const omm::Point tr(m_direction + arma::vec2{-r, -r });
    const omm::Point br(m_direction + arma::vec2{ r, -r });

    renderer.draw_spline({ o, tip }, current_style());
    renderer.draw_spline({ tl, bl, br, tr, tl }, current_style());
  }

private:
  omm::ScaleTool& m_tool;
};

auto make_handles(omm::ScaleTool& tool)
{
  const auto make_style = [](const omm::Color& color) {
    omm::Style style;
    style.property(omm::Style::PEN_IS_ACTIVE_KEY).set(true);
    style.property(omm::Style::BRUSH_IS_ACTIVE_KEY).set(true);
    style.property(omm::Style::PEN_COLOR_KEY).set(color);
    style.property(omm::Style::PEN_WIDTH_KEY).set(2.0);
    style.property(omm::Style::BRUSH_COLOR_KEY).set(color);
    return style;
  };

  using Status = omm::Handle::Status;
  auto x_axis = std::make_unique<ScaleAxisHandle>(tool);
  x_axis->set_style(Status::Active, make_style(omm::Color(1.0, 1.0, 1.0)));
  x_axis->set_style(Status::Hovered, make_style(omm::Color(1.0, 0.0, 0.0)));
  x_axis->set_style(Status::Inactive, make_style(omm::Color(1.0, 0.3, 0.3)));
  x_axis->set_direction({100, 0});

  auto y_axis = std::make_unique<ScaleAxisHandle>(tool);
  y_axis->set_style(Status::Active, make_style(omm::Color(1.0, 1.0, 1.0)));
  y_axis->set_style(Status::Hovered, make_style(omm::Color(0.0, 1.0, 0.0)));
  y_axis->set_style(Status::Inactive, make_style(omm::Color(0.3, 1.0, 0.3)));
  y_axis->set_direction({0, 100});

  std::vector<std::unique_ptr<omm::Handle>> handles;
  handles.push_back(std::move(x_axis));
  handles.push_back(std::move(y_axis));
  return handles;
}

}  // namespace

namespace omm
{

ScaleTool::ScaleTool(Scene& scene)
  : ObjectTransformationTool(scene, make_handles(*this))
{

}

std::string ScaleTool::type() const
{
  return TYPE;
}

QIcon ScaleTool::icon() const
{
  return QIcon();
}

}  // namespace omm