#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QVector>

#include "log_store.hpp"

class QLineEdit;
class QDockWidget;
class LogPanel : public QWidget
{
  Q_OBJECT
public:
  explicit LogPanel(LogStore * store, QWidget * parent = nullptr);
  QString selectedSource() const;

public slots:
  void showSource(const QString & source);
  Q_INVOKABLE void raiseDock();
  Q_INVOKABLE void setSessionLabel(const QString & label);
  void setDock(QDockWidget * dock) { dock_ = dock; }

private slots:
  void refreshSources();
  void exportFull();
  void exportFiltered();

private:
  LogStore * store_;
  QTabWidget * panes_[2];
  QVector<QWidget *> pane_widgets_[2];
  QString selected_source_;
  QLineEdit * session_label_{nullptr};
  QDockWidget * dock_{nullptr};
};
