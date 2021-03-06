#include "mainwindow/toolbar.h"
#include "tools/toolbox.h"
#include "application.h"
#include "iconprovider.h"

#include <QApplication>

namespace omm
{

ToolBar::ToolBar(QWidget* parent, ToolBox& tool_box, const std::vector<std::string>& tools)
  : QToolBar(tr("ToolBar"), parent)
  , m_tools(tools)
{
  setAttribute(Qt::WA_DeleteOnClose);
  setObjectName("ToolBar");
  const QSize icon_size(24, 24);
  auto& icon_provider = Application::instance().icon_provider;
  for (auto&& tool_class : tools) {
    auto& tool = tool_box.tool(tool_class);
    auto* action = addAction(icon_provider.get_icon(tool.type(), icon_size),
                             QString::fromStdString(tool.name()));
    connect(action, &QAction::triggered, [tool_class, &tool_box]() {
      tool_box.set_active_tool(tool_class);
    });
  }
}

const std::vector<std::string>& ToolBar::tools() const
{
  return m_tools;
}

}  // namespace omm
