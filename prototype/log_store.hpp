#pragma once

#include <QObject>
#include <QDateTime>
#include <QVector>
#include <QStringList>

struct LogEntry
{
  QDateTime time;
  QString level;
  QString source;
  QString message;
};

class LogStore : public QObject
{
  Q_OBJECT
public:
  explicit LogStore(QObject * parent = nullptr);

  const QVector<LogEntry> & entries() const { return entries_; }
  QString filePath() const { return file_path_; }
  QStringList sources() const;
  QString exportText(const QString & source = {}, const QStringList & levels = {},
    const QString & text = {}, bool filtered = false) const;
  static QString redact(const QString & text);

  void append(const QString & source, const QString & level, const QString & message);

signals:
  void changed();

private:
  QVector<LogEntry> entries_;
  QString file_path_;
};

bool run_log_store_contract_test();
