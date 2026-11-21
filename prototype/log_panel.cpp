#include "log_panel.hpp"

#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
class LogSourceView : public QWidget
{
public:
  LogSourceView(LogStore * store, const QString & source, QWidget * parent = nullptr)
  : QWidget(parent), store_(store), source_(source)
  {
    auto * layout = new QVBoxLayout(this);
    auto * filters = new QHBoxLayout;
    search_ = new QLineEdit(this);
    search_->setPlaceholderText("Search this log");
    filters->addWidget(search_, 1);
    auto * severity = new QToolButton(this);
    severity->setText("Severity");
    severity->setPopupMode(QToolButton::InstantPopup);
    auto * severity_menu = new QMenu(severity);
    severity->setMenu(severity_menu);
    filters->addWidget(severity);
    for (const auto & level : {QString("DEBUG"), QString("INFO"), QString("WARN"), QString("ERROR"), QString("FATAL")}) {
      auto * check = severity_menu->addAction(level);
      check->setCheckable(true);
      check->setChecked(true);
      levels_.insert(level, check);
      QObject::connect(check, &QAction::toggled, this, [this, severity]() {
        const auto count = selectedLevels().size();
        severity->setText(count == levels_.size() ? "Severity" : QString("Severity (%1)").arg(count));
        render();
      });
    }
    QObject::connect(search_, &QLineEdit::textChanged, this, [this]() { render(); });
    layout->addLayout(filters);
    output_ = new QPlainTextEdit(this);
    output_->setReadOnly(true);
    output_->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(output_, 1);
    render();
  }

  QString source() const { return source_; }
  QStringList selectedLevels() const
  {
    QStringList selected;
    for (auto it = levels_.cbegin(); it != levels_.cend(); ++it) if (it.value()->isChecked()) selected.append(it.key());
    return selected;
  }
  QString searchText() const { return search_->text(); }
  void render()
  {
    QStringList lines;
    for (const auto & entry : store_->entries()) {
      if (source_ != "All logs" && entry.source != source_) continue;
      if (!levels_.value(entry.level) || !levels_.value(entry.level)->isChecked()) continue;
      if (!entry.message.contains(search_->text(), Qt::CaseInsensitive)) continue;
      lines.append(QString("%1  %2  [%3]  %4")
        .arg(entry.time.toString("HH:mm:ss.zzz"), entry.level, entry.source, entry.message));
    }
    output_->setPlainText(lines.join('\n'));
  }

private:
  LogStore * store_;
  QString source_;
  QLineEdit * search_{};
  QPlainTextEdit * output_{};
  QMap<QString, QAction *> levels_;
};
}

LogPanel::LogPanel(LogStore * store, QWidget * parent) : QWidget(parent), store_(store)
{
  auto * root = new QVBoxLayout(this);
  auto * toolbar = new QHBoxLayout;
  session_label_ = new QLineEdit(this);
  session_label_->setReadOnly(true);
  session_label_->setFrame(false);
  session_label_->setAccessibleName("Session log location");
  session_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  toolbar->addWidget(session_label_, 1);
  auto * full = new QPushButton("Export full recording…", this);
  auto * filtered = new QPushButton("Export left filtered view…", this);
  toolbar->addWidget(full);
  toolbar->addWidget(filtered);
  root->addLayout(toolbar);

  auto * split = new QHBoxLayout;
  for (int index = 0; index < 2; ++index) {
    panes_[index] = new QTabWidget(this);
    panes_[index]->setTabsClosable(false);
    panes_[index]->setUsesScrollButtons(true);
    panes_[index]->setElideMode(Qt::ElideRight);
    split->addWidget(panes_[index], 1);
    connect(panes_[index], &QTabWidget::currentChanged, this, [this, index](int) {
      if (index == 0) selected_source_ = selectedSource();
    });
  }
  root->addLayout(split, 1);
  connect(store_, &LogStore::changed, this, &LogPanel::refreshSources);
  connect(full, &QPushButton::clicked, this, &LogPanel::exportFull);
  connect(filtered, &QPushButton::clicked, this, &LogPanel::exportFiltered);
  refreshSources();
}

QString LogPanel::selectedSource() const
{
  auto * current = dynamic_cast<LogSourceView *>(panes_[0]->currentWidget());
  return current ? current->source() : QString("All logs");
}

void LogPanel::showSource(const QString & source)
{
  refreshSources();
  for (int i = 0; i < panes_[0]->count(); ++i) {
    if (panes_[0]->tabText(i) == source) {
      panes_[0]->setCurrentIndex(i);
      selected_source_ = source;
      return;
    }
  }
  panes_[0]->setCurrentIndex(0);
}

void LogPanel::raiseDock()
{
  if (!dock_) return;
  const bool was_hidden = dock_->isHidden();
  dock_->show();
  dock_->raise();
  if (was_hidden && !dock_->isFloating()) {
    if (auto * window = qobject_cast<QMainWindow *>(dock_->parentWidget())) {
      auto area = window->dockWidgetArea(dock_);
      if (area == Qt::TopDockWidgetArea || area == Qt::BottomDockWidgetArea) {
        QTimer::singleShot(0, dock_, [window, dock = dock_]() {
          window->resizeDocks({dock}, {qMin(220, window->height() / 3)}, Qt::Vertical);
        });
      }
    }
  }
}

void LogPanel::setSessionLabel(const QString & label)
{
  const auto description = "Full session log · " + label + " · " + store_->filePath();
  session_label_->setText(description);
  session_label_->setCursorPosition(0);
  session_label_->setToolTip(description);
}

void LogPanel::refreshSources()
{
  for (int pane = 0; pane < 2; ++pane) {
    for (int i = 0; i < panes_[pane]->count(); ++i) {
      if (auto * view = dynamic_cast<LogSourceView *>(panes_[pane]->widget(i))) view->render();
    }
    for (const auto & source : store_->sources()) {
      bool found = false;
      for (int i = 0; i < panes_[pane]->count(); ++i) found |= panes_[pane]->tabText(i) == source;
      if (found) continue;
      auto * view = new LogSourceView(store_, source, panes_[pane]);
      panes_[pane]->addTab(view, source);
    }
  }
}

void LogPanel::exportFull()
{
  const auto path = QFileDialog::getSaveFileName(this, "Export full log recording", "session.log", "Log files (*.log *.txt);;All files (*)");
  if (path.isEmpty()) return;
  QSaveFile file(path);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write(store_->exportText().toUtf8());
    file.commit();
  }
}

void LogPanel::exportFiltered()
{
  auto * view = dynamic_cast<LogSourceView *>(panes_[0]->currentWidget());
  if (!view) return;
  const auto path = QFileDialog::getSaveFileName(this, "Export filtered log view", "session-filtered.log", "Log files (*.log *.txt);;All files (*)");
  if (path.isEmpty()) return;
  QSaveFile file(path);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write(store_->exportText(view->source(), view->selectedLevels(), view->searchText(), true).toUtf8());
    file.commit();
  }
}
