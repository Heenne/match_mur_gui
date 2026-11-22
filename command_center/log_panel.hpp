#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QVector>

#include "log_store.hpp"

class QLineEdit;
class QDockWidget;
class QCheckBox;
class QPushButton;
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

protected:
  void changeEvent(QEvent * event) override;

private slots:
  void refreshSources();
  void exportFull();
  void exportFiltered();

private:
  void retranslate();
  LogStore * store_;
  QTabWidget * panes_[2];
  QVector<QWidget *> pane_widgets_[2];
  QString selected_source_;
  QLineEdit * session_label_{nullptr};
  QCheckBox * compare_;
  QPushButton * export_full_;
  QPushButton * export_filtered_;
  QDockWidget * dock_{nullptr};
};

bool run_log_panel_contract_test();
