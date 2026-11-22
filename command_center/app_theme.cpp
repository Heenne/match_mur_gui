#include "app_theme.hpp"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace app_theme
{
namespace
{
bool light_theme = false;
}

const QVariantMap & colors()
{
  static const QVariantMap dark{
    {"canvas", QColor("#181A1D")}, {"surface", QColor("#24272B")},
    {"raised", QColor("#1D2024")}, {"formSection", QColor("#2C3137")},
    {"hover", QColor("#30353B")}, {"sidebar", QColor("#1D2024")},
    {"line", QColor("#3B4047")}, {"ink", QColor("#F0F1F3")},
    {"muted", QColor("#B0B5BD")}, {"subtle", QColor("#B0B5BD")},
    {"accent", QColor("#B1CA21")}, {"accentInk", QColor("#202900")},
    {"focus", QColor("#B1CA21")}, {"selected", QColor("#30353B")},
    {"selectedLine", QColor("#B1CA21")}, {"badge", QColor("#343A42")},
    {"badgeInk", QColor("#E0E4EA")}, {"tooltip", QColor("#30353B")},
    {"tooltipInk", QColor("#C3C8CF")},
    {"warning", QColor("#F4C47B")}, {"warningSurface", QColor("#3B2E20")},
    {"danger", QColor("#FFAAA5")}, {"dangerSurface", QColor("#42272B")},
    {"info", QColor("#B0B5BD")}, {"infoSurface", QColor("#343A42")},
  };
  static const QVariantMap light{
    {"canvas", QColor("#F5F5F5")}, {"surface", QColor("#FFFFFF")},
    {"raised", QColor("#F0F0F0")}, {"formSection", QColor("#FFFFFF")},
    {"hover", QColor("#EEEEEE")}, {"sidebar", QColor("#FFFFFF")},
    {"line", QColor("#DEDEDE")}, {"ink", QColor("#242424")},
    {"muted", QColor("#666666")}, {"subtle", QColor("#626262")},
    {"accent", QColor("#B1CA21")}, {"accentInk", QColor("#242900")},
    {"focus", QColor("#647900")}, {"selected", QColor("#EEEEEE")},
    {"selectedLine", QColor("#B1CA21")}, {"badge", QColor("#EEEEEE")},
    {"badgeInk", QColor("#454545")}, {"tooltip", QColor("#FAFAFA")},
    {"tooltipInk", QColor("#555555")},
    {"warning", QColor("#81510B")}, {"warningSurface", QColor("#FFF0D8")},
    {"danger", QColor("#AD4B1E")}, {"dangerSurface", QColor("#FFF0E6")},
    {"info", QColor("#626262")}, {"infoSurface", QColor("#EEEEEE")},
  };
  return light_theme ? light : dark;
}

void apply(QApplication & app, bool light)
{
  light_theme = light;
  const auto color = [](const QString & key) { return colors().value(key).value<QColor>(); };
  QPalette palette;
  palette.setColor(QPalette::Window, color("surface"));
  palette.setColor(QPalette::WindowText, color("ink"));
  palette.setColor(QPalette::Base, color("raised"));
  palette.setColor(QPalette::AlternateBase, color("surface"));
  palette.setColor(QPalette::Text, color("ink"));
  palette.setColor(QPalette::Button, color("raised"));
  palette.setColor(QPalette::ButtonText, color("ink"));
  palette.setColor(QPalette::BrightText, color("danger"));
  palette.setColor(QPalette::Light, color("hover"));
  palette.setColor(QPalette::Midlight, color("line"));
  palette.setColor(QPalette::Mid, color("line"));
  palette.setColor(QPalette::Dark, color("canvas"));
  palette.setColor(QPalette::Shadow, color("canvas"));
  palette.setColor(QPalette::Highlight, color("accent"));
  palette.setColor(QPalette::HighlightedText, color("accentInk"));
  palette.setColor(QPalette::Link, color("info"));
  palette.setColor(QPalette::ToolTipBase, color("tooltip"));
  palette.setColor(QPalette::ToolTipText, color("tooltipInk"));
  palette.setColor(QPalette::PlaceholderText, color("muted"));
  for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
    palette.setColor(QPalette::Disabled, role, color("subtle"));
  }
  app.setPalette(palette);

  app.setStyleSheet(QString(R"CSS(
    QDockWidget::title { background: %1; border-bottom: 1px solid %2; padding: 5px 8px; }
    QMainWindow::separator { background: %2; width: 5px; height: 5px; }
    QMainWindow::separator:hover { background: %3; }
    QStatusBar { color: %4; background: %1; }
    QWidget#commandLogPanel { background: %1; }
    QWidget#commandLogPanel QLineEdit#sessionLogLocation { background: %1; color: %4; }
    QWidget#commandLogPanel QPushButton, QWidget#commandLogPanel QToolButton {
      background: palette(button); border: 1px solid %2; border-radius: 5px; padding: 4px 8px;
    }
    QWidget#commandLogPanel QPushButton:hover, QWidget#commandLogPanel QToolButton:hover {
      background: palette(light);
    }
    QWidget#commandLogPanel QPlainTextEdit { border: 1px solid %2; border-radius: 6px; padding: 5px; }
    QWidget#commandLogPanel QTabWidget::pane { border: 1px solid %2; border-radius: 5px; }
    QWidget#commandLogPanel QTabBar::tab { background: %1; color: %4; padding: 6px 12px; }
    QWidget#commandLogPanel QTabBar::tab:selected { background: %5; color: palette(text); border-bottom: 2px solid %3; }
  )CSS").arg(color("surface").name(), color("line").name(), color("accent").name(),
    color("muted").name(), color("selected").name()));
}
}
