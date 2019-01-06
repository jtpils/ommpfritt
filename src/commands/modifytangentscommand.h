#pragma once

#include <set>

#include "commands/command.h"
#include "geometry/point.h"

namespace omm
{

class ModifyTangentsCommand : public Command
{
public:
  class PointWithAlternative
  {
  public:
    PointWithAlternative(Point& point, const Point& alternative);
    void swap();
    bool operator==(const PointWithAlternative& other) const;
  private:
    Point& m_point;
    Point m_alternative;
  };

  ModifyTangentsCommand(const std::vector<PointWithAlternative>& alternatives);
  void undo() override;
  void redo() override;
  int id() const override;
  bool mergeWith(const QUndoCommand* command) override;

private:
  std::vector<PointWithAlternative> m_alternatives;
};

}  // namespace