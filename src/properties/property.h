#pragma once

#include <pybind11/embed.h>
#include <string>
#include <typeinfo>
#include <unordered_set>
#include <glog/logging.h>

#include "objects/objecttransformation.h" //TODO remove that include
#include "external/json.hpp"
#include "observerregister.h"

namespace py = pybind11;

namespace omm
{

template<typename ValueT> class TypedProperty;
class Object;

#define DISABLE_DANGEROUS_PROPERTY_TYPE(T) \
  static_assert(!std::is_same<ValueT, T>(), \
                "Forbidden Property Type '"#T"'.");

// Disable some T in TypedProperty<T>
// Without this guard, it was easy to accidentally use the
// TypedProperty<const char*> acidentally when you actually want
// TypedProperty<std::string>. It's hard to notice and things will
// break at strange places so debugging it becomes ugly.
#define DISABLE_DANGEROUS_PROPERTY_TYPES \
  DISABLE_DANGEROUS_PROPERTY_TYPE(const char*)


namespace {
// template<typename T>
// nlohmann::json to_json_value(const T& value)
// {
//   return value;
// }

// nlohmann::json to_json_value(const ObjectTransformation& t)
// {
//   return {};
// }

// nlohmann::json to_json_value(const Object*& t)
// {
//   return {};
// }

// nlohmann::json to_json_value(const std::string& t)
// {
//   return {};
// }


}  // namespace

class Property
{
public:
  Property(const std::string& label, const std::string& category);
  virtual ~Property();

  template<typename ValueT> bool is_type() const
  {
    return cast<ValueT>() != nullptr;
  }

  template<typename ValueT> const TypedProperty<ValueT>& cast() const
  {
    DISABLE_DANGEROUS_PROPERTY_TYPES
    return *dynamic_cast<const TypedProperty<ValueT>*>(this);
  }

  template<typename ValueT> TypedProperty<ValueT>& cast()
  {
    DISABLE_DANGEROUS_PROPERTY_TYPES
    return *dynamic_cast<TypedProperty<ValueT>*>(this);
  }

  virtual void set_py_object(const py::object& value) = 0;
  virtual py::object get_py_object() const = 0;
  virtual std::type_index type_index() const = 0;
  virtual nlohmann::json to_json() const = 0;
  std::string label() const;
  std::string category() const;

  static std::unique_ptr<Property> from_json(const nlohmann::json& json);

private:
  const std::string m_label;
  const std::string m_category;
};

class AbstractTypedPropertyObserver
{
protected:
  virtual void on_value_changed() = 0;
  template<typename ValueT> friend class TypedProperty;
};

template<typename ValueT>
class TypedProperty : public Property, public ObserverRegister<AbstractTypedPropertyObserver>
{
public:
  using value_type = ValueT;

  TypedProperty(const std::string& label, const std::string& category, ValueT defaultValue)
    : Property(label, category)
    , m_value(defaultValue)
    , m_defaultValue(defaultValue)
  {
  }

public:
  virtual void set_value(ValueT value)
  {
    m_value = value;
  }

  virtual ValueT value() const
  {
    return m_value;
  }

  virtual ValueT& value()
  {
    return m_value;
  }

  virtual void reset()
  {
    m_value = m_defaultValue;
  }

  py::object get_py_object() const override;

  void set_py_object(const py::object& value) override;
  // {
  //   return py::cast(m_value);
  // }
  // {
  //   m_value = value.cast<ValueT>();
  // }

  std::type_index type_index() const override
  {
    return std::type_index(typeid(ValueT));
  }

  static const std::type_index TYPE_INDEX;

private:
  ValueT m_value;
  const ValueT m_defaultValue;
};

template<typename ValueT> const std::type_index TypedProperty<ValueT>::TYPE_INDEX = std::type_index(typeid(ValueT));

template<typename T>
class SimpleTypeProperty : public TypedProperty<T>
{
public:
  using TypedProperty<T>::TypedProperty;
  nlohmann::json to_json() const
  {
    return TypedProperty<T>::value();
  }
};

using IntegerProperty = SimpleTypeProperty<int>;
using FloatProperty = SimpleTypeProperty<double>;
using StringProperty = SimpleTypeProperty<std::string>;
using TransformationProperty = SimpleTypeProperty<ObjectTransformation>;

class ReferenceProperty : public TypedProperty<Object*>
{
public:
  ReferenceProperty(const std::string& label, const std::string& category);
  static bool is_referenced(const Object* candidate);
  void set_value(Object* value) override;
  nlohmann::json to_json() const override;

private:
  static std::unordered_set<const Object*> m_references;
};

template<typename ValueT>
py::object TypedProperty<ValueT>::get_py_object() const
{
  return py::cast(m_value);
}

template<typename ValueT>
void TypedProperty<ValueT>::set_py_object(const py::object& value)
{
  m_value = value.cast<ValueT>();
}

}  // namespace omm
