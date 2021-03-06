#include "managers/objectmanager/objectquickaccessdelegate.h"
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
#include "managers/objectmanager/objecttreeview.h"
#include <QDebug>
#include "scene/scene.h"
#include "commands/propertycommand.h"
#include "properties/boolproperty.h"
#include "properties/optionsproperty.h"

namespace
{

omm::Object::Visibility advance_visibility(omm::Object::Visibility visibility)
{
  switch (visibility) {
  case omm::Object::Visibility::Visible: return omm::Object::Visibility::Hide;
  case omm::Object::Visibility::Hide: return omm::Object::Visibility::HideTree;
  case omm::Object::Visibility::HideTree: return omm::Object::Visibility::Visible;
  }
  Q_UNREACHABLE();
}

void draw_cross(QPainter& painter, const QRectF& area, bool is_enabled)
{
  QPen pen;
  if (is_enabled) {
    pen.setColor("#80FF80");
  } else {
    pen.setColor("#FF8080");
  }
  pen.setWidth(4.0);
  pen.setCosmetic(true);
  pen.setCapStyle(Qt::RoundCap);

  const auto rect = area - QMarginsF(0.02, 0.02, 0.02, 0.02);

  painter.save();
  painter.setPen(pen);
  painter.drawLine(rect.topLeft(), rect.bottomRight());
  painter.drawLine(rect.topRight(), rect.bottomLeft());
  painter.restore();
}

QColor get_visibility_color_code(const omm::Object::Visibility visibility)
{
  switch (visibility) {
  case omm::Object::Visibility::Visible: return QColor("#6AE008");
  case omm::Object::Visibility::Hide: return QColor("#FF4A03");
  case omm::Object::Visibility::HideTree: return QColor("#AA00ED");
  }
  Q_UNREACHABLE();
}

void draw_dot(QPainter& painter, const QRectF& area, const omm::Object::Visibility visibility)
{
  QPen pen;

  static constexpr QMarginsF margins(0.05, 0.05, 0.05, 0.05);

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::black);
  painter.drawEllipse(area);
  painter.setBrush(get_visibility_color_code(visibility));
  painter.drawEllipse(area - margins);
  painter.restore();
}

}  // namespace

namespace omm
{

ObjectQuickAccessDelegate::ObjectQuickAccessDelegate(ObjectTreeView& view)
  : m_view(view)
{
}

void ObjectQuickAccessDelegate::
paint(QPainter *painter, const QStyleOptionViewItem &, const QModelIndex &index) const
{
  static constexpr QMarginsF margins(0.1, 0.1, 0.1, 0.1);

  const auto& object = m_view.model()->item_at(index);

  painter->save();
  const auto rect = m_view.visualRect(index);
  painter->translate(rect.topLeft());
  painter->scale(rect.width(), rect.height());
  draw_cross(*painter, enabled_cross_area - margins, object.is_active());
  draw_dot(*painter, edit_visibility - margins, object.visibility());
  // draw_dot(*painter, export_visibility);
  painter->restore();
}

QSize ObjectQuickAccessDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
  return QSize(-1, -1);
}

bool ObjectQuickAccessDelegate::on_mouse_button_press(QMouseEvent& event)
{
  const auto index = m_view.indexAt(event.pos());
  auto& object = m_view.model()->item_at(index);
  const auto rect = m_view.visualRect(index);
  auto pos = QPointF(event.pos()) - rect.topLeft();
  pos.setX(pos.x() / rect.width());
  pos.setY(pos.y() / rect.height());
  if (enabled_cross_area.contains(pos)) {
    const auto is_active = object.is_active();
    auto& property = *object.property(Object::IS_ACTIVE_PROPERTY_KEY);
    m_view.scene().submit<PropertiesCommand<BoolProperty>>(std::set { &property }, !is_active);
    return true;
  } else if (edit_visibility.contains(pos)) {
    const auto visibility = static_cast<int>(advance_visibility(object.visibility()));
    auto& property = *object.property(Object::IS_VISIBLE_PROPERTY_KEY);
    m_view.scene().submit<PropertiesCommand<OptionsProperty>>(std::set { &property }, visibility);
    return true;
  } else {
    return false;
  }
}

}  // namespace omm
