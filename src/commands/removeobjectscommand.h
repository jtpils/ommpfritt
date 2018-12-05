#pragma once

#include <set>

#include "commands/command.h"
#include "common.h"
#include "scene/contextes.h"

namespace omm
{

class Object;

class RemoveObjectsCommand : public Command
{
public:
  RemoveObjectsCommand(Scene& scene, const std::set<omm::Object*>& objects);

  void undo() override;
  void redo() override;

private:
  std::vector<ObjectTreeOwningContext> m_contextes;
  Scene& m_scene;
};

}  // namespace
