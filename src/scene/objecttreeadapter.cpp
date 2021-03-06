#include "scene/objecttreeadapter.h"

#include <QItemSelection>
#include <QDebug>

#include "scene/propertyownermimedata.h"
#include "objects/object.h"
#include "common.h"
#include "commands/movecommand.h"
#include "commands/copycommand.h"
#include "commands/addcommand.h"
#include "commands/movetagscommand.h"
#include "properties/stringproperty.h"
#include "commands/propertycommand.h"
#include "properties/referenceproperty.h"
#include "scene/scene.h"
#include "abstractraiiguard.h"
#include "tags/tag.h"
#include "tags/styletag.h"
#include "scene/contextes.h"
#include "mainwindow/application.h"

namespace
{

using AddTagCommand = omm::AddCommand<omm::List<omm::Tag>>;

class AbstractRAIISceneInvalidatorGuard : public AbstractRAIIGuard
{
protected:
  AbstractRAIISceneInvalidatorGuard(omm::Scene& scene) : m_scene(scene) {}
  ~AbstractRAIISceneInvalidatorGuard() override { m_scene.invalidate(); }

private:
  omm::Scene& m_scene;
};

template<typename T>
T* last(const std::vector<T*>& ts) { return ts.size() == 0 ? nullptr : ts.back(); }

void drop_tags_onto_object( omm::Scene& scene, omm::Object& object,
                            const std::vector<omm::Tag*>& tags, Qt::DropAction action )
{
  if (tags.size() > 0) {
    switch (action) {
    case Qt::CopyAction:
    {
      auto macro = scene.history.start_macro(QObject::tr("copy tags", "ObjectTreeAdapter"));
      for (auto* tag : tags) {
        scene.submit<AddTagCommand>(object.tags, tag->clone(object));
      }
      break;
    }
    case Qt::MoveAction:
      scene.submit<omm::MoveTagsCommand>(tags, object, last(object.tags.ordered_items()));
      break;
    default: break;
    }
  }
}

void drop_tags_behind( omm::Scene& scene, omm::Object& object, omm::Tag* current_tag_predecessor,
                        const std::vector<omm::Tag*>& tags, Qt::DropAction action )
{
  if (current_tag_predecessor != nullptr && current_tag_predecessor->owner != &object) {
    const auto& tags = object.tags;
    current_tag_predecessor = last(tags.ordered_items());
  }
  if (tags.size() > 0) {
    switch (action) {
    case Qt::CopyAction:
    {
      auto macro = scene.history.start_macro(QObject::tr("copy tags", "ObjectTreeAdapter"));
      for (auto* tag : tags) {
        omm::ListOwningContext<omm::Tag> context(tag->clone(object), current_tag_predecessor);
        scene.submit<AddTagCommand>(object.tags, std::move(context));
      }
      break;
    }
    case Qt::MoveAction:
      scene.submit<omm::MoveTagsCommand>(tags, object, current_tag_predecessor);
      break;
    default: break;
    }
  }
}

void drop_style_onto_object( omm::Scene& scene, omm::Object& object,
                             const std::vector<omm::Style*>& styles )
{
  if (styles.size() > 0) {
    auto macro = scene.history.start_macro(QObject::tr("set styles tags", "ObjectTreeAdapter"));
    for (auto* style : styles) {
      auto style_tag = std::make_unique<omm::StyleTag>(object);
      style_tag->property(omm::StyleTag::STYLE_REFERENCE_PROPERTY_KEY)->set(style);
      scene.submit<AddTagCommand>(object.tags, std::move(style_tag));
    }
  }
}

}  // namespace

namespace omm
{

ObjectTreeAdapter::ObjectTreeAdapter(Scene& scene, Tree<Object>& tree)
  : ItemModelAdapter(scene, tree)
{
  connect(&scene, &Scene::scene_changed, [this](AbstractPropertyOwner* subject, int code, auto* p) {
    if (code == AbstractPropertyOwner::PROPERTY_CHANGED) {
      auto* object = kind_cast<Object*>(subject);
      if (object != nullptr) {
        if (p == object->property(Object::NAME_PROPERTY_KEY)) {
          const auto index = index_of(*object);
          Q_EMIT dataChanged(index, index, { Qt::DisplayRole });
        } else if (p == object->property(Object::IS_ACTIVE_PROPERTY_KEY)
                   || p == object->property(Object::IS_VISIBLE_PROPERTY_KEY)) {
          auto index = index_of(*object);
          index = index.sibling(index.row(), 1);
          Q_EMIT dataChanged(index, index, { Qt::DisplayRole });
        }
      }
    }
  });
}

QModelIndex ObjectTreeAdapter::index(int row, int column, const QModelIndex& parent) const
{
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  return createIndex(row, column, &item_at(parent).tree_child(row));
}

QModelIndex ObjectTreeAdapter::parent(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return QModelIndex();
  }

  const Object& parent_item = item_at(index);
  if (parent_item.is_root()) {
    return QModelIndex();
  } else {
    return index_of(parent_item.tree_parent());
  }
}

int ObjectTreeAdapter::rowCount(const QModelIndex& parent) const
{
  return item_at(parent).n_children();
}

int ObjectTreeAdapter::columnCount(const QModelIndex& parent) const
{
  Q_UNUSED(parent)
  return 3;
}

bool ObjectTreeAdapter::setData(const QModelIndex& index, const QVariant& value, int role)
{
  switch (index.column()) {
  case 0:
  {
    Property* property = item_at(index).property(Object::NAME_PROPERTY_KEY);
    const auto svalue = value.toString().toStdString();
    if (property->value<std::string>() != svalue) {
      scene.submit<PropertiesCommand<StringProperty>>(std::set { property }, svalue);
      return true;
    } else {
      return false;
    }
  }
  }

  return false;
}

QVariant ObjectTreeAdapter::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return QVariant();
  }

  switch (index.column()) {
  case 0:
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return QString::fromStdString(item_at(index).name());
    case Qt::DecorationRole:
      return Application::instance().icon_provider.get_icon(item_at(index).type(), QSize(24, 24));
    }
  }
  return QVariant();
}

Object& ObjectTreeAdapter::item_at(const QModelIndex& index) const
{
  if (index.isValid()) {
    assert(index.internalPointer() != nullptr);
    return *static_cast<Object*>(index.internalPointer());
  } else {
    return structure.root();
  }
}

QModelIndex ObjectTreeAdapter::index_of(Object& object) const
{
  if (object.is_root()) {
    return QModelIndex();
  } else {
    return createIndex(scene.object_tree.position(object), 0, &object);
  }
}

QVariant ObjectTreeAdapter::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    switch (section) {
    case 0: return QObject::tr("object", "ObjectTreeAdapter");
    case 1: return QObject::tr("is visible", "ObjectTreeAdapter");
    case 2: return QObject::tr("tags", "ObjectTreeAdapter");
    }
  }

 return QVariant();
}

Qt::ItemFlags ObjectTreeAdapter::flags(const QModelIndex &index) const
{
  if (!index.isValid()) {
    return Qt::ItemIsDropEnabled;
  }

  switch (index.column()) {
  case 0:
    return Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled
            | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
  case 2:
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
  default:
    return Qt::ItemIsEnabled;
  }
}

std::unique_ptr<AbstractRAIIGuard>
ObjectTreeAdapter::acquire_inserter_guard(Object& parent, int row)
{
  class InserterGuard : public AbstractRAIISceneInvalidatorGuard
  {
  public:
    InserterGuard(ObjectTreeAdapter& model, const QModelIndex& parent, int row)
      : AbstractRAIISceneInvalidatorGuard(model.scene), m_model(model)
    {
      m_model.beginInsertRows(parent, row, row);
    }

    ~InserterGuard() { m_model.endInsertRows(); }
  private:
    ObjectTreeAdapter& m_model;
  };
  return std::make_unique<InserterGuard>(*this, index_of(parent), row);
}

std::unique_ptr<AbstractRAIIGuard>
ObjectTreeAdapter::acquire_mover_guard(const ObjectTreeMoveContext& context)
{
  class MoverGuard : public AbstractRAIISceneInvalidatorGuard
  {
  public:
    MoverGuard( ObjectTreeAdapter& model, const QModelIndex& old_parent, const int old_pos,
                const QModelIndex& new_parent, const int new_pos )
      : AbstractRAIISceneInvalidatorGuard(model.scene), m_model(model)
    {
      m_model.beginMoveRows(old_parent, old_pos, old_pos, new_parent, new_pos);
    }

    ~MoverGuard() { m_model.endMoveRows(); }
  private:
    ObjectTreeAdapter& m_model;
  };

  assert(!context.subject.get().is_root());
  Object& old_parent = context.subject.get().tree_parent();
  Object& new_parent = context.parent.get();
  const auto old_pos = scene.object_tree.position(context.subject);
  const auto new_pos = scene.object_tree.insert_position(context.predecessor);

  if (old_pos == new_pos && &old_parent == &new_parent) {
    return nullptr;
  } else {
    return std::make_unique<MoverGuard>( *this, index_of(old_parent), old_pos,
                                         index_of(new_parent), new_pos );
  }
}

std::unique_ptr<AbstractRAIIGuard>
ObjectTreeAdapter::acquire_remover_guard(const Object& object)
{
  class RemoverGuard : public AbstractRAIISceneInvalidatorGuard
  {
  public:
    RemoverGuard(ObjectTreeAdapter& model, const QModelIndex& parent, int row)
      : AbstractRAIISceneInvalidatorGuard(model.scene), m_model(model)
    {
      m_model.beginRemoveRows(parent, row, row);
    }
    ~RemoverGuard() { m_model.endRemoveRows(); }

  private:
    ObjectTreeAdapter& m_model;
  };
  return std::make_unique<RemoverGuard>( *this, index_of(object.tree_parent()),
                                         scene.object_tree.position(object) );
}

std::unique_ptr<AbstractRAIIGuard>
ObjectTreeAdapter::acquire_reseter_guard()
{
  class ReseterGuard : public AbstractRAIISceneInvalidatorGuard
  {
  public:
    ReseterGuard(ObjectTreeAdapter& model)
      : AbstractRAIISceneInvalidatorGuard(model.scene), m_model(model)
    {
      m_model.beginResetModel();
    }
    ~ReseterGuard() { m_model.endResetModel(); }

  private:
    ObjectTreeAdapter& m_model;
  };
  return std::make_unique<ReseterGuard>(*this);
}

bool ObjectTreeAdapter::dropMimeData( const QMimeData *data, Qt::DropAction action,
                                      int row, int column, const QModelIndex &parent )
{
  if (ItemModelAdapter::dropMimeData(data, action, row, column, parent)) {
    return true;
  } else {
    const auto* pdata = qobject_cast<const PropertyOwnerMimeData*>(data);
    if (pdata != nullptr) {
      if (const auto tags = pdata->tags(); tags.size() > 0) {
        if (parent.column() == OBJECT_COLUMN) {
          assert(parent.isValid());  // otherwise, column was < 0.
          drop_tags_onto_object(scene, item_at(parent), tags, action);
          return true;
        } else if (parent.column() == TAGS_COLUMN) {
          assert(tags.front() != current_tag_predecessor || action != Qt::MoveAction);
          drop_tags_behind(scene, item_at(parent), current_tag_predecessor, tags, action);
          return true;
        }
      }
      if (const auto styles = pdata->styles(); styles.size() > 0) {
        if (parent.column() == OBJECT_COLUMN && column < 0) {
          assert(parent.isValid()); // otherwise, column was < 0
          drop_style_onto_object(scene, item_at(parent), styles);
        } else if (parent.column() == TAGS_COLUMN && column < 0) {
          assert(current_tag->type() == StyleTag::TYPE);
          assert(styles.size() == 1);
          auto* style_tag = static_cast<StyleTag*>(current_tag);
          auto& property = *style_tag->property(StyleTag::STYLE_REFERENCE_PROPERTY_KEY);
          using ref_prop_cmd_t = PropertiesCommand<ReferenceProperty>;
          scene.submit<ref_prop_cmd_t>(std::set { &property }, *styles.begin());
        }
      }
    }
  }
  return false;
}

bool ObjectTreeAdapter::canDropMimeData( const QMimeData *data, Qt::DropAction action,
                                         int row, int column, const QModelIndex &parent ) const
{
  const auto actual_column = column >= 0 ? column : parent.column();
  if (ItemModelAdapter::canDropMimeData(data, action, row, column, parent)) {
    return true;
  } else {
    const auto* pdata = qobject_cast<const PropertyOwnerMimeData*>(data);
    if (pdata != nullptr) {
      const auto tags = pdata->tags();
      if (tags.size() > 0) {
        switch (actual_column) {
        case OBJECT_COLUMN: return true;
        case TAGS_COLUMN:
          return tags.front() != current_tag_predecessor || action != Qt::MoveAction;
        }
        return false;
      }
      if (const auto styles = pdata->styles(); styles.size() > 0) {
        if (parent.column() == OBJECT_COLUMN && column < 0) {
          return true;
        } else if (parent.column() == TAGS_COLUMN && column < 0 && styles.size() == 1) {
          if (current_tag != nullptr && current_tag->type() == StyleTag::TYPE) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

std::size_t ObjectTreeAdapter::max_number_of_tags_on_object() const
{
  const auto objects = scene.object_tree.items();
  const auto cmp = [](const Object* lhs, const Object* rhs) {
    return lhs->tags.size() < rhs->tags.size();
  };
  const auto max = std::max_element(objects.begin(), objects.end(), cmp);
  if (max != objects.end()) {
    return (*max)->tags.size();
  } else {
    return 0;
  }
}

}  // namespace omm
