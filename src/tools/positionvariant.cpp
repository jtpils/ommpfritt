#include "tools/positionvariant.h"
#include "objects/path.h"
#include "scene/scene.h"

bool arma::operator<(const arma::vec2& a, const arma::vec2& b)
{
  return a[0] == b[0] ? a[1] < b[1] : a[0] < b[0];
}

namespace
{

template<typename Ts, typename T, typename... F> T mean(const Ts& ts, const T& null, F&&... fs)
{
  if (ts.size() > 0) {
    return std::accumulate(std::begin(ts), std::end(ts), null, fs...) / ts.size();
  } else {
    return null;
  }
}

}

namespace omm
{

void PointPositions::make_handles(handles_type& handles, Tool& tool) const
{
  for (auto* path : paths()) {
    const auto t = path->global_transformation();
    handles.reserve(handles.size() + path->points().size());
    for (auto&& point : path->points()) {
      handles.push_back(std::make_unique<PointSelectHandle>(tool, *path, *point));
    }
  }
}

void PointPositions::clear_selection()
{
  for (auto* path : paths()) {
    for (auto* point : path->points()) {
      point->is_selected = false;
    }
  }
}

arma::vec2 PointPositions::selection_center() const
{
  std::set<arma::vec2> positions;
  for (auto* path : paths()) {
    for (auto* point : path->points()) {
      if (point->is_selected) {
        positions.insert(path->global_transformation().apply_to_position(point->position));
      }
    }
  }
  return mean(positions, arma::vec2{0.0, 0.0});
}

double PointPositions::selection_rotation() const
{
  return 0.0;
}

std::set<Point*> PointPositions::selected_points() const
{
  std::set<Point*> selected_points;
  for (auto* path : paths()) {
    for (auto* point : path->points()) {
      if (point->is_selected) {
        selected_points.insert(point);
      }
    }
  }
  return selected_points;
}

bool PointPositions::is_empty() const
{
  return selected_points().size() == 0;
}

std::set<Path*> PointPositions::paths() const
{
  const auto is_path = [](const auto* o) { return o->type() == Path::TYPE; };
  const auto to_path = [](auto* o) { return static_cast<Path*>(o); };
  return ::transform<Path*>(::filter_if(scene.item_selection<Object>(), is_path), to_path);
}

void ObjectPositions::make_handles(handles_type& handles, Tool& tool) const
{
  // ignore object selection. Return a handle for each object.
  const auto objects = scene.object_tree.items();
  handles.reserve(objects.size());
  auto inserter = std::back_inserter(handles);
  std::transform(objects.begin(), objects.end(), inserter, [this, &tool](Object* o) {
    return std::make_unique<ObjectSelectHandle>(tool, scene, *o);
  });
}

void ObjectPositions::clear_selection()
{
  scene.set_selection({});
}

arma::vec2 ObjectPositions::selection_center() const
{
  const auto objects = scene.item_selection<Object>();
  const auto add = [](const arma::vec2& accu, const Object* object) -> arma::vec2 {
    return accu + object->global_transformation().translation();
  };
  return mean(objects, arma::vec2{0.0, 0.0}, add);
}

double ObjectPositions::selection_rotation() const
{
  const auto objects = scene.item_selection<Object>();
  if (objects.size() == 1) {
    return (**objects.begin()).global_transformation().rotation();
  } else {
    return 0.0;
  }
}

bool ObjectPositions::is_empty() const
{
  return scene.item_selection<Object>().size() == 0;
}

}  // namespace omm
