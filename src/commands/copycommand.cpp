#include "commands/copycommand.h"
#include "objects/object.h"
#include "renderers/style.h"
#include "scene/tree.h"
#include "scene/list.h"

namespace omm
{

template<typename Structure>
CopyCommand<Structure>::CopyCommand(Structure& structure, std::vector<Context> contextes)
  : Command(QObject::tr("copy").toStdString())
  , m_contextes(std::move(contextes))
  , m_structure(structure)
{
  const auto* predecessor = m_contextes.front().predecessor;
  for (auto& context : m_contextes) {
    context.subject.capture_by_copy();
    context.predecessor = predecessor;
    predecessor = &context.subject.get();
  }
}

template<typename Structure> void CopyCommand<Structure>::redo()
{
  for (auto&& context : m_contextes) {
    assert(context.subject.owns());
    assert(context.is_sane());
    m_structure.insert(context);
  }
}

template<typename Structure> void CopyCommand<Structure>::undo()
{
  for (auto&& it = m_contextes.rbegin(); it != m_contextes.rend(); ++it) {
    assert(!it->subject.owns());
    assert(it->is_sane());
    m_structure.remove(*it);
  }
}

template class CopyCommand<Tree<Object>>;
template class CopyCommand<List<Style>>;

}  // namespace omm
