#include "log_panel.hpp"
#include "app_theme.hpp"

#include <QAction>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSyntaxHighlighter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
class LogHighlighter : public QSyntaxHighlighter
{
public:
  using QSyntaxHighlighter::QSyntaxHighlighter;

private:
  void highlightBlock(const QString & text) override
  {
    static const QRegularExpression prefix(R"(^(\d\d:\d\d:\d\d\.\d{3})\s+(DEBUG|INFO|WARN|ERROR|FATAL)\s+(\[[^\]]*\]))");
    const auto match = prefix.match(text);
    if (!match.hasMatch()) return;
    const auto & theme = app_theme::colors();
    const auto level = match.captured(2);
    const auto tone = level == "INFO" ? "focus" : level == "WARN" ? "warning" :
      level == "DEBUG" ? "muted" : "danger";
    setFormat(match.capturedStart(1), match.capturedLength(1), theme.value("subtle").value<QColor>());
    setFormat(match.capturedStart(2), match.capturedLength(2), theme.value(tone).value<QColor>());
    setFormat(match.capturedStart(3), match.capturedLength(3), theme.value("info").value<QColor>());
  }
};

class LogSourceView : public QWidget
{
  Q_DECLARE_TR_FUNCTIONS(LogSourceView)

public:
  LogSourceView(LogStore * store, const QString & source, QWidget * parent = nullptr)
  : QWidget(parent), store_(store), source_(source)
  {
    auto * layout = new QVBoxLayout(this);
    auto * filters = new QHBoxLayout;
    search_ = new QLineEdit(this);
    filters->addWidget(search_, 1);
    severity_ = new QToolButton(this);
    severity_->setPopupMode(QToolButton::InstantPopup);
    auto * severity_menu = new QMenu(severity_);
    severity_->setMenu(severity_menu);
    filters->addWidget(severity_);
    follow_ = new QCheckBox(this);
    follow_->setObjectName("followLatest");
    follow_->setChecked(true);
    filters->addWidget(follow_);
    for (const auto & level : {QString("DEBUG"), QString("INFO"), QString("WARN"), QString("ERROR"), QString("FATAL")}) {
      auto * check = severity_menu->addAction(level);
      check->setCheckable(true);
      check->setChecked(true);
      levels_.insert(level, check);
      QObject::connect(check, &QAction::toggled, this, [this]() {
        updateSeverityText();
        reloadFilteredOutput();
      });
    }
    QObject::connect(search_, &QLineEdit::textChanged, this, [this]() { reloadFilteredOutput(); });
    layout->addLayout(filters);
    output_ = new QPlainTextEdit(this);
    output_->setReadOnly(true);
    output_->setLineWrapMode(QPlainTextEdit::NoWrap);
    output_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    highlighter_ = new LogHighlighter(output_->document());
    retranslate();
    layout->addWidget(output_, 1);
    auto * vertical = output_->verticalScrollBar();
    QObject::connect(follow_, &QCheckBox::toggled, this, [vertical](bool follow) {
      if (follow) vertical->setValue(vertical->maximum());
    });
    QObject::connect(vertical, &QScrollBar::rangeChanged, this, [this, vertical](int, int maximum) {
      if (follow_->isChecked()) vertical->setValue(maximum);
    });
    appendNewEntries();
  }

  QString source() const { return source_; }
  QStringList selectedLevels() const
  {
    QStringList selected;
    for (auto it = levels_.cbegin(); it != levels_.cend(); ++it) if (it.value()->isChecked()) selected.append(it.key());
    return selected;
  }
  QString searchText() const { return search_->text(); }
  void appendNewEntries()
  {
    const auto & entries = store_->entries();
    const auto search = search_->text();
    static const QRegularExpression ansi_escape(R"(\x1b\[[0-?]*[ -/]*[@-~])");
    QStringList lines;
    while (next_entry_ < entries.size()) {
      const auto & entry = entries.at(next_entry_++);
      if (source_ != "All logs" && entry.source != source_) continue;
      if (!levels_.value(entry.level) || !levels_.value(entry.level)->isChecked()) continue;
      if (!entry.message.contains(search, Qt::CaseInsensitive)) continue;
      const auto message = QString(entry.message).remove(ansi_escape);
      lines.append(QString("%1  %2  [%3]  %4")
        .arg(entry.time.toString("HH:mm:ss.zzz"), entry.level, entry.source, message));
    }
    if (lines.isEmpty()) return;
    auto * vertical = output_->verticalScrollBar();
    auto * horizontal = output_->horizontalScrollBar();
    const int old_line = vertical->value();
    const int old_column = horizontal->value();
    output_->appendPlainText(lines.join('\n'));
    vertical->setValue(follow_->isChecked() ? vertical->maximum() : old_line);
    horizontal->setValue(old_column);
  }

protected:
  void changeEvent(QEvent * event) override
  {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) retranslate();
    if (event->type() == QEvent::PaletteChange && highlighter_) highlighter_->rehighlight();
  }

private:
  void retranslate()
  {
    search_->setPlaceholderText(tr("Search this log"));
    follow_->setText(tr("Follow latest"));
    updateSeverityText();
  }

  void updateSeverityText()
  {
    const auto count = selectedLevels().size();
    severity_->setText(count == levels_.size() ? tr("Severity") : tr("Severity (%1)").arg(count));
  }

  void reloadFilteredOutput()
  {
    next_entry_ = 0;
    output_->clear();
    appendNewEntries();
  }

  LogStore * store_;
  QString source_;
  int next_entry_{0};
  QLineEdit * search_{};
  QCheckBox * follow_{};
  QToolButton * severity_{};
  QPlainTextEdit * output_{};
  LogHighlighter * highlighter_{};
  QMap<QString, QAction *> levels_;
};
}

LogPanel::LogPanel(LogStore * store, QWidget * parent) : QWidget(parent), store_(store)
{
  setObjectName("commandLogPanel");
  auto * root = new QVBoxLayout(this);
  root->setContentsMargins(10, 6, 10, 10);
  auto * toolbar = new QHBoxLayout;
  session_label_ = new QLineEdit(this);
  session_label_->setReadOnly(true);
  session_label_->setObjectName("sessionLogLocation");
  session_label_->setFrame(false);
  session_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  toolbar->addWidget(session_label_, 1);
  compare_ = new QCheckBox(this);
  compare_->setObjectName("compareLogs");
  toolbar->addWidget(compare_);
  export_full_ = new QPushButton(this);
  export_filtered_ = new QPushButton(this);
  toolbar->addWidget(export_full_);
  toolbar->addWidget(export_filtered_);
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
  panes_[1]->hide();
  root->addLayout(split, 1);
  connect(compare_, &QCheckBox::toggled, this, [this](bool enabled) {
    panes_[1]->setVisible(enabled);
    retranslate();
    if (enabled) refreshSources();
  });
  connect(store_, &LogStore::changed, this, &LogPanel::refreshSources);
  connect(export_full_, &QPushButton::clicked, this, &LogPanel::exportFull);
  connect(export_filtered_, &QPushButton::clicked, this, &LogPanel::exportFiltered);
  refreshSources();
  retranslate();
}

void LogPanel::changeEvent(QEvent * event)
{
  QWidget::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) retranslate();
}

void LogPanel::retranslate()
{
  session_label_->setAccessibleName(tr("Session log location"));
  compare_->setText(tr("Compare logs"));
  export_full_->setText(tr("Export full recording…"));
  export_filtered_->setText(compare_->isChecked() ?
    tr("Export left filtered view…") : tr("Export filtered view…"));
  for (auto * pane : panes_) {
    if (pane->count() > 0) pane->setTabText(0, tr("All logs"));
  }
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
    const auto * view = static_cast<const LogSourceView *>(panes_[0]->widget(i));
    if (view->source() == source) {
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
  session_label_->setText(store_->filePath());
  session_label_->setCursorPosition(0);
  session_label_->setToolTip(tr("Session log · %1").arg(label) + '\n' + store_->filePath());
}

void LogPanel::refreshSources()
{
  const auto sources = store_->sources();
  for (int pane = 0; pane < 2; ++pane) {
    if (pane == 1 && panes_[pane]->isHidden()) continue;
    for (int i = 0; i < panes_[pane]->count(); ++i) {
      if (auto * view = dynamic_cast<LogSourceView *>(panes_[pane]->widget(i))) view->appendNewEntries();
    }
    for (const auto & source : sources) {
      bool found = false;
      for (int i = 0; i < panes_[pane]->count(); ++i) {
        const auto * view = static_cast<const LogSourceView *>(panes_[pane]->widget(i));
        found |= view->source() == source;
      }
      if (found) continue;
      auto * view = new LogSourceView(store_, source, panes_[pane]);
      panes_[pane]->addTab(view, source == "All logs" ? tr("All logs") : source);
    }
  }
}

void LogPanel::exportFull()
{
  const auto path = QFileDialog::getSaveFileName(this, tr("Export full log recording"),
    "session.log", tr("Log files (*.log *.txt);;All files (*)"));
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
  const auto path = QFileDialog::getSaveFileName(this, tr("Export filtered log view"),
    "session-filtered.log", tr("Log files (*.log *.txt);;All files (*)"));
  if (path.isEmpty()) return;
  QSaveFile file(path);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write(store_->exportText(view->source(), view->selectedLevels(), view->searchText(), true).toUtf8());
    file.commit();
  }
}

bool run_log_panel_contract_test()
{
  LogStore store;
  for (int line = 0; line < 80; ++line) {
    store.append("Simulation", "INFO", QString("Line %1: ").arg(line) + QString(180, 'x'));
  }
  LogPanel panel(&store);
  panel.resize(1050, 300);
  panel.show();
  QCoreApplication::processEvents();

  const auto panes = panel.findChildren<QTabWidget *>(QString(), Qt::FindDirectChildrenOnly);
  if (panes.size() != 2) return false;
  auto * left_view = panes[0]->currentWidget();
  auto * left = left_view->findChild<QPlainTextEdit *>();
  if (!left || left->verticalScrollBar()->maximum() == 0) return false;
  left->verticalScrollBar()->setValue(left->verticalScrollBar()->maximum());
  store.append("Simulation", "INFO", "Live output marker");
  const auto at_tail = [](QPlainTextEdit * view) {
    return view->verticalScrollBar()->value() == view->verticalScrollBar()->maximum();
  };
  if (!at_tail(left)) {
    qCritical("New live output moved the log away from the latest line");
    return false;
  }

  auto * compare = panel.findChild<QCheckBox *>("compareLogs");
  auto * follow = left_view->findChild<QCheckBox *>("followLatest");
  if (!compare || compare->isChecked() || panes[1]->isVisible() || !follow || !follow->isChecked()) return false;
  follow->click();
  compare->click();
  QCoreApplication::processEvents();
  auto * right_view = panes[1]->currentWidget();
  auto * right = right_view ? right_view->findChild<QPlainTextEdit *>() : nullptr;
  if (!panes[1]->isVisible() || !right || !at_tail(right)) return false;

  auto * vertical = left->verticalScrollBar();
  auto * horizontal = left->horizontalScrollBar();
  const auto read_position = vertical->maximum() / 2;
  const auto read_column = horizontal->maximum() / 2;
  vertical->setValue(read_position);
  horizontal->setValue(read_column);
  store.append("Simulation", "INFO", "Reading history marker");
  if (vertical->value() != read_position || horizontal->value() != read_column ||
    !left->toPlainText().contains("Reading history marker") || !at_tail(right)) {
    qCritical("An incoming log entry changed the paused pane or failed to update the live pane");
    return false;
  }

  follow->click();
  if (!at_tail(left)) return false;
  compare->click();
  return panes[0]->isVisible() && !panes[1]->isVisible();
}
