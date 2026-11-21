#include "app_theme.hpp"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace app_theme
{
const QVariantMap & colors()
{
  static const QVariantMap palette{
    {"canvas", QColor("#0d131d")}, {"surface", QColor("#151e2b")},
    {"raised", QColor("#1c2838")}, {"hover", QColor("#253449")},
    {"line", QColor("#2d3c51")}, {"ink", QColor("#eef3fb")},
    {"muted", QColor("#a7b6cc")}, {"subtle", QColor("#778ba5")},
    {"accent", QColor("#65e5c6")}, {"accentInk", QColor("#092920")},
    {"selected", QColor("#183b3b")}, {"selectedLine", QColor("#31635e")},
    {"warning", QColor("#f4c47b")}, {"warningSurface", QColor("#3b2e20")},
    {"danger", QColor("#ff9ea5")}, {"dangerSurface", QColor("#432730")},
    {"info", QColor("#9cc9ff")}, {"infoSurface", QColor("#24354d")},
  };
  return palette;
}

void apply(QApplication & app)
{
  app.setStyle("Fusion");
  const auto color = [](const QString & key) { return colors().value(key).value<QColor>(); };
  QPalette palette;
  palette.setColor(QPalette::Window, color("surface"));
  palette.setColor(QPalette::WindowText, color("ink"));
  palette.setColor(QPalette::Base, color("canvas"));
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
  palette.setColor(QPalette::ToolTipBase, color("hover"));
  palette.setColor(QPalette::ToolTipText, color("ink"));
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
    QWidget#commandLogPanel QTabBar::tab:selected { background: %5; color: %3; }
  )CSS").arg(color("surface").name(), color("line").name(), color("accent").name(),
    color("muted").name(), color("selected").name()));
}
}
