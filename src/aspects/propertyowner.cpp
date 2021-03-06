#include "aspects/propertyowner.h"

#include <QObject>

#include "external/json.hpp"
#include "serializers/abstractserializer.h"
#include "properties/referenceproperty.h"

namespace
{
  constexpr auto PROPERTIES_POINTER = "properties";
  constexpr auto PROPERTY_TYPE_POINTER = "type";
  constexpr auto PROPERTY_KEY_POINTER = "key";
  constexpr auto ID_POINTER = "id";
}  // namespace

namespace omm
{

const std::string AbstractPropertyOwner::NAME_PROPERTY_KEY = "name";

const OrderedMap<std::string, Property>& AbstractPropertyOwner::properties() const
{
  return m_properties;
}

AbstractPropertyOwner::AbstractPropertyOwner(const AbstractPropertyOwner &other)
{
  for (auto&& key : other.m_properties.keys()) {
    add_property(key, other.m_properties.at(key)->clone());
  }
}

AbstractPropertyOwner::~AbstractPropertyOwner()
{
  for (ReferenceProperty* ref_prop : m_referees) {
    ReferenceProperty::NotificationBlocker blocker(*ref_prop);
    ref_prop->set(nullptr);
  }
}

Property *AbstractPropertyOwner::property(const std::string& key) const
{
  if (has_property(key)) {
    return m_properties.at(key).get();
  } else {
    return nullptr;
  }
}

bool AbstractPropertyOwner::has_property(const std::string& key) const
{
  return m_properties.contains(key);
}

void AbstractPropertyOwner::serialize(AbstractSerializer& serializer, const Pointer& root) const
{
  Serializable::serialize(serializer, root);
  const auto id_pointer = make_pointer(root, ID_POINTER);
  serializer.set_value(this, id_pointer);

  const auto properties_pointer = make_pointer(root, PROPERTIES_POINTER);
  serializer.start_array(m_properties.size(), properties_pointer);
  for (size_t i = 0; i < m_properties.size(); ++i) {
    const auto property_key = m_properties.keys().at(i);
    const auto property_pointer = make_pointer(properties_pointer, i);
    const auto& property = *this->property(property_key);
    serializer.set_value(property_key, make_pointer(property_pointer, PROPERTY_KEY_POINTER));
    serializer.set_value(property.type(), make_pointer(property_pointer, PROPERTY_TYPE_POINTER));
    property.serialize(serializer, property_pointer);
  }
  serializer.end_array();
}

void AbstractPropertyOwner::deserialize(AbstractDeserializer& deserializer, const Pointer& root)
{
  Serializable::deserialize(deserializer, root);

  const auto id_pointer = make_pointer(root, ID_POINTER);
  m_id = deserializer.get_size_t(id_pointer);
  deserializer.register_reference(m_id, *this);

  const auto properties_pointer = make_pointer(root, PROPERTIES_POINTER);
  size_t n_properties = deserializer.array_size(properties_pointer);
  for (size_t i = 0; i < n_properties; ++i) {
    const auto property_pointer = make_pointer(properties_pointer, i);

    const auto property_key =
      deserializer.get_string(make_pointer(property_pointer, PROPERTY_KEY_POINTER));
    const auto property_type =
      deserializer.get_string(make_pointer(property_pointer, PROPERTY_TYPE_POINTER));

    if (properties().contains(property_key)) {
      assert(property_type == property(property_key)->type());
    } else {
      try {
        add_property(property_key, Property::make(property_type));
      } catch (const std::out_of_range&) {
        const auto msg = "Failed to retrieve property type '" + property_type + "'.";
        LERROR << msg;
        throw AbstractDeserializer::DeserializeError(msg);
      }
    }

    property(property_key)->deserialize(deserializer, property_pointer);
  }
}

void AbstractPropertyOwner::on_property_value_changed(Property& property, std::set<const void *> trace)
{
  trace.insert(this);
  on_change(this, PROPERTY_CHANGED, &property, trace);
}

void AbstractPropertyOwner::on_change(AbstractPropertyOwner *subject, int what, Property *property,
                                      std::set<const void*> trace)
{
  trace.insert(this);
  Observed<AbstractPropertyOwnerObserver>::for_each([=](auto* observer) {
    observer->on_change(subject, what, property, trace);
  });
  Q_EMIT property_changed(property, trace);
}

std::string AbstractPropertyOwner::name() const
{
  return property(NAME_PROPERTY_KEY)->value<std::string>();
}

std::unique_ptr<Property> AbstractPropertyOwner::extract_property(const std::string& key)
{
  auto property = m_properties.extract(key);
  property->Observed<AbstractPropertyObserver>::unregister_observer(this);
  return property;
}

std::ostream& operator<<(std::ostream& ostream, const AbstractPropertyOwner* apo)
{
  if (apo == nullptr) {
    ostream << "AbstractPropertyOwner[nullptr]";
  } else {
    const auto kind = static_cast<std::underlying_type_t<decltype(apo->kind())>>(apo->kind());
    ostream << "AbstractPropertyOwner[" << kind << "]";
  }
  return ostream;
}

void AbstractPropertyOwner::copy_properties(AbstractPropertyOwner& target) const
{
  const auto keys = [](const AbstractPropertyOwner& o) {
    return ::transform<std::string, std::set>(o.properties().keys(), ::identity);
  };

  for (const auto& key : ::intersect(keys(target), keys(*this))) {
    const auto& p = *property(key);
    auto& other_property = *target.property(key);
    if (other_property.is_compatible(p)) {
      other_property.set(p.variant_value());
    }
  }
}

std::size_t AbstractPropertyOwner::id() const
{
  if (m_id == 0) {
    m_id = std::hash<const void*>()(this);
  }
  return m_id;
}

}  // namespace omm
