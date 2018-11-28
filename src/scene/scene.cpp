#include "scene.h"
#include <random>
#include <glog/logging.h>
#include <assert.h>
#include <QDebug>

#include "objects/empty.h"
#include "external/json.hpp"
#include "properties/stringproperty.h"
#include "serializers/abstractserializer.h"
#include "commands/command.h"

namespace
{

constexpr auto ROOT_POINTER = "root";

auto make_root()
{
  return std::make_unique<omm::Empty>();
}

template<typename T> bool is_selected(const T* t) { return t->is_selected(); }

}  // namespace

namespace omm
{

Scene* Scene::m_current = nullptr;

Scene::Scene()
  : m_root(make_root())
{
  m_root->property<StringProperty>(Object::NAME_PROPERTY_KEY).value() = "_root_";
  m_current = this;
}

Scene::~Scene()
{
  if (m_current == this) {
    m_current = nullptr;
  }
}

ObjectView Scene::root_view()
{
  return ObjectView(*m_root);
}

Scene* Scene::currentInstance()
{
  return m_current;
}

void Scene::insert_object(std::unique_ptr<Object> object, Object& parent)
{
  size_t n = parent.children().size();

  Observed<AbstractObjectTreeObserver>::for_each(
    [&parent, n] (auto* observer) { observer->beginInsertObject(parent, n); }
  );

  parent.adopt(std::move(object));

  Observed<AbstractObjectTreeObserver>::for_each(
    [] (auto* observer) { observer->endInsertObject(); }
  );
}

Object& Scene::root() const
{
  return *m_root;
}

std::set<Object*> Scene::objects() const
{
  return root().all_descendants();
}

std::set<Tag*> Scene::tags(const std::set<Object*>& objects) const
{
  std::set<Tag*> tags;
  for (const auto object : objects) {
    const auto object_tags = object->tags();
    tags.insert(object_tags.begin(), object_tags.end());
  }
  return tags;
}

std::set<Tag*> Scene::tags() const
{
  return tags(objects());
}

std::set<Tag*> Scene::selected_tags(const std::set<Object*>& objects) const
{
  return ::filter_if(tags(objects), is_selected<Tag>);
}

std::set<Tag*> Scene::selected_tags() const
{
  return ::filter_if(tags(), is_selected<Tag>);
}

std::set<Object*> Scene::selected_objects() const
{
  return ::filter_if(objects(), is_selected<Object>);
}

std::set<AbstractPropertyOwner*> Scene::selection() const
{
  std::set<AbstractPropertyOwner*> selection;
  const auto selected_objects = this->selected_objects();
  const auto selected_tags = this->selected_tags(selected_objects);
  selection.insert(selected_objects.begin(), selected_objects.end());
  selection.insert(selected_tags.begin(), selected_tags.end());
  // TODO add selected styles
  return selection;
}

void Scene::selection_changed()
{
  const auto selected_objects = selection();

  Observed<AbstractSelectionObserver>::for_each(
    [selected_objects](auto* observer) { observer->set_selection(selected_objects); }
  );
}

void Scene::clear_selection()
{
  for (auto& o : root().all_descendants()) {
    o->set_selected(false);
    for (auto& t : o->tags()) {
      t->set_selected(false);
    }
  }
  selection_changed();
}

void Scene::move_object(MoveObjectTreeContext context)
{
  assert(context.is_valid());
  Object& old_parent = context.subject.get().parent();

  Observed<AbstractObjectTreeObserver>::for_each(
    [&context](auto* observer) { observer->beginMoveObject(context); }
  );
  context.parent.get().adopt(old_parent.repudiate(context.subject), context.predecessor);
  Observed<AbstractObjectTreeObserver>::for_each(
    [](auto* observer) { observer->endMoveObject(); }
  );
}

void Scene::insert_object(OwningObjectTreeContext& context)
{
  assert(context.subject.owns());
  Observed<AbstractObjectTreeObserver>::for_each(
    [&context](auto* observer) { observer->beginInsertObject(context); }
  );
  context.parent.get().adopt(context.subject.release(), context.predecessor);
  Observed<AbstractObjectTreeObserver>::for_each(
    [](auto* observer) { observer->endInsertObject(); }
  );
}

void Scene::remove_object(OwningObjectTreeContext& context)
{
  assert(!context.subject.owns());
  Observed<AbstractObjectTreeObserver>::for_each(
    [&context](auto* observer) { observer->beginRemoveObject(context.subject); }
  );
  assert(context.predecessor == context.subject.reference().predecessor());
  context.subject.capture(context.parent.get().repudiate(context.subject));
  Observed<AbstractObjectTreeObserver>::for_each(
    [](auto* observer) { observer->endRemoveObject(); }
  );
}

bool Scene::save_as(const std::string &filename)
{
  std::ofstream ofstream(filename);
  if (ofstream) {
    auto serializer = AbstractSerializer::make( "JSONSerializer",
                                                static_cast<std::ostream&>(ofstream) );
    root().serialize(*serializer, ROOT_POINTER);

    LOG(INFO) << "Saved current scene to '" << filename << "'";
    set_has_pending_changes(false);
    m_filename = filename;
    return true;
  } else {
    LOG(ERROR) << "Failed to open ofstream at '" << filename << "'";
    return false;
  }
}

bool Scene::load_from(const std::string &filename)
{
  std::ifstream ifstream(filename);
  if (ifstream) {
    auto deserializer = AbstractDeserializer::make( "JSONDeserializer",
                                                    static_cast<std::istream&>(ifstream) );
    try {
      auto new_root = make_root();
      new_root->deserialize(*deserializer, ROOT_POINTER);
      replace_root(std::move(new_root));
    } catch (const AbstractDeserializer::DeserializeError& deserialize_error) {
      LOG(ERROR) << "Failed to deserialize file at '" << filename << "'.";
      LOG(INFO) << deserialize_error.what();
      return false;
    }

    set_has_pending_changes(false);
    m_filename = filename;
    return true;
  } else {
    LOG(ERROR) << "Failed to open '" << filename << "'.";
    return false;
  }
}

void Scene::reset()
{
  set_has_pending_changes(false);
  replace_root(make_root());
}

std::string Scene::filename() const
{
  return m_filename;
}

void Scene::set_has_pending_changes(bool v)
{
  m_has_pending_changes = v;
}

bool Scene::has_pending_changes() const
{
  return m_has_pending_changes;
}

void Scene::submit(std::unique_ptr<Command> command)
{
  m_undo_stack.push(command.release());
  set_has_pending_changes(true);
}

QUndoStack& Scene::undo_stack()
{
  return m_undo_stack;
}

std::unique_ptr<Object> Scene::replace_root(std::unique_ptr<Object> new_root)
{
  auto old_root = std::move(m_root);
  m_root = std::move(new_root);
  return old_root;
}

StylePool& Scene::style_pool()
{
  return m_style_pool;
}

const StylePool& Scene::style_pool() const
{
  return m_style_pool;
}

}  // namespace omm
