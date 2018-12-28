#include "tags/scripttag.h"
#include <QApplication>  // TODO only for icon testing
#include <QStyle>        // TODO only for icon testing
#include <pybind11/embed.h>

#include "properties/stringproperty.h"
#include "properties/boolproperty.h"

namespace py = pybind11;

namespace omm
{

ScriptTag::ScriptTag(Object& owner)
  : Tag(owner)
{
  add_property<StringProperty>(CODE_PROPERTY_KEY)
    .set_is_multi_line(true)
    .set_label("code").set_category("script");
}

ScriptTag::~ScriptTag()
{
}

std::string ScriptTag::type() const
{
  return TYPE;
}

QIcon ScriptTag::icon() const
{
  return QApplication::style()->standardIcon(QStyle::SP_FileDialogListView);
}

std::unique_ptr<Tag> ScriptTag::clone() const
{
  return std::make_unique<ScriptTag>(*this);
}

}  // namespace omm
