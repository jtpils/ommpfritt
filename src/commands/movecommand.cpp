#include "commands/movecommand.h"
#include "objects/object.h"
#include "renderers/style.h"
#include "scene/scene.h"
#include "scene/tree.h"
#include "scene/list.h"

namespace
{

template<typename StructureT>
using context_type = typename omm::MoveCommand<StructureT>::context_type;

template<typename StructureT>
auto make_old_contextes( const StructureT& structure,
                         const std::vector<context_type<StructureT>>& new_contextes )
{
  const auto make_old_context = [&structure](const auto& new_context) {
    return context_type<StructureT>( new_context.subject,
                                     structure.predecessor(new_context.subject) );
  };
  return ::transform<context_type<StructureT>>(new_contextes, make_old_context);
}

}  // namespace

namespace omm
{

template<typename StructureT> MoveCommand<StructureT>
::MoveCommand(StructureT& structure, const std::vector<context_type>& new_contextes)
  : Command(QObject::tr("reparent").toStdString())
  , m_old_contextes(make_old_contextes(structure, new_contextes))
  , m_new_contextes(new_contextes)
  , m_structure(structure)
{
}

template<typename StructureT> void MoveCommand<StructureT>::redo()
{
  for (auto& context : m_new_contextes) {
    assert(context.is_sane());
    m_structure.move(context);
  }
}

template<typename StructureT> void MoveCommand<StructureT>::undo()
{
  for (auto ctx_it = m_old_contextes.rbegin(); ctx_it != m_old_contextes.rend(); ++ctx_it) {
    assert(ctx_it->is_sane());
    m_structure.move(*ctx_it);
  }
}

template class MoveCommand<Tree<Object>>;
template class MoveCommand<List<Style>>;

}  // namespace omm
