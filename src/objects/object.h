#pragma once

#include <vector>
#include <memory>
#include "external/json_fwd.hpp"
#include "objecttransformation.h"
#include "properties/hasproperties.h"
#include "objects/selectable.h"
#include "common.h"

namespace omm
{

class Property;
class Tag;
class Scene;

class Object : public HasProperties, public Selectable
{
public:
  explicit Object(Scene& scene);
  virtual ~Object();

  bool is_root() const;
  Object& parent() const;

  Object& adopt(std::unique_ptr<Object> object, const Object* predecessor = nullptr);

  std::unique_ptr<Object> repudiate(Object& object);

  void transform(const ObjectTransformation& transformation);
  ObjectTransformation transformation() const;
  ObjectTransformation global_transformation() const;
  void set_transformation(const ObjectTransformation& transformation);
  void set_global_transformation(const ObjectTransformation& globalTransformation);

  /**
   * @brief updates the id of this object and all its children recursively.
   * @details ids become invalid when the scene tree is modified. Call this method to update them.
   */
  void update_ids() const;
  size_t id() const;

  Tag& add_tag(std::unique_ptr<Tag> tag);
  std::unique_ptr<Tag> remove_tag(Tag& tag);

  Scene& scene() const;
  virtual nlohmann::json to_json() const;
  ObjectRefs children();
  Object& child(size_t i) const;
  size_t n_children() const;
  size_t row() const;
  const Object* predecessor() const;
  size_t get_insert_position(const Object* child_before_position) const;

  std::unordered_set<HasProperties*> get_selected_children_and_tags();

  template<typename T> T& create_child()
  {
    std::unique_ptr<T> object = std::make_unique<T>(scene());
    return static_cast<T&>(adopt(std::move(object)));
  }

  template<typename T> T& create_tag()
  {
    std::unique_ptr<T> tag = std::make_unique<T>(*this);
    return static_cast<T&>(add_tag(std::move(tag)));
  }

  void reset_parent(Object& new_parent);
  bool is_ancestor_of(const Object& descendant) const;

  static ObjectTransformation translation(const double& dx, const double dy);
  static ObjectTransformation rotation(const double& r);
  static ObjectTransformation scalation(const double& sx, const double sy);
  static ObjectTransformation identity();

  static const std::string TRANSFORMATION_PROPERTY_KEY;
  static const std::string NAME_PROPERTY_KEY;
  static const std::string THE_ANSWER_KEY;

  DEFINE_CLASSNAME("object")

private:
  std::vector<std::unique_ptr<Tag>> m_tags;
  std::vector<std::unique_ptr<Object>> m_children;
  Object* m_parent;
  Scene& m_scene;

  friend class ObjectView;

  mutable size_t m_id;
  void update_ids(size_t& last_id) const;
};

std::ostream& operator<<(std::ostream& ostream, const Object& object);
bool operator<(const Object& a, const Object& b);

}  // namespace omm