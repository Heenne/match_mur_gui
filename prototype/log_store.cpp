#include "log_store.hpp"
#include "process_manager.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

LogStore::LogStore(QObject * parent) : QObject(parent)
{
  const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
  QDir().mkpath(directory);
  file_path_ = directory + "/session-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzz") + ".tsv";
}

QStringList LogStore::sources() const
{
  QStringList result{"All logs", "ROS /rosout"};
  for (const auto & entry : entries_) {
    if (entry.source != "ROS /rosout" && !result.contains(entry.source)) result.append(entry.source);
  }
  return result;
}

QString LogStore::redact(const QString & text)
{
  QString safe = text;
  static const QList<QRegularExpression> patterns{
    QRegularExpression(R"((?i)\b([A-Za-z0-9_-]*(?:password|passwd|token|secret|api[_-]?key|access[_-]?key|private[_-]?key)[A-Za-z0-9_-]*\s*[=:]\s*)(?:"[^"]*"|'[^']*'|[^\s,;]+))"),
    QRegularExpression(R"((?i)\b(Bearer)\s+[A-Za-z0-9._~+/=-]+)"),
    QRegularExpression(R"((https?://)[^/@\s:]+:[^/@\s]+@)"),
    QRegularExpression(R"((?i)(sshpass\s+-p\s+)(?:"[^"]*"|'[^']*'|[^\s]+))"),
    QRegularExpression(R"((?i)(--(?:password|token|secret|api-key|access-key)\s+)(?:"[^"]*"|'[^']*'|[^\s]+))"),
  };
  safe.replace(patterns[0], QStringLiteral("\\1[REDACTED]"));
  safe.replace(patterns[1], QStringLiteral("\\1 [REDACTED]"));
  safe.replace(patterns[2], QStringLiteral("\\1[REDACTED]@"));
  safe.replace(patterns[3], QStringLiteral("\\1[REDACTED]"));
  safe.replace(patterns[4], QStringLiteral("\\1[REDACTED]"));
  return safe;
}

void LogStore::append(const QString & source, const QString & level, const QString & message)
{
  if (message.trimmed().isEmpty()) return;
  entries_.append({QDateTime::currentDateTimeUtc(), level.trimmed().isEmpty() ? "INFO" : level,
    source.trimmed().isEmpty() ? "GUI" : source, redact(message)});
  QFile file(file_path_);
  if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    const auto & entry = entries_.last();
    file.write(QString("%1\t%2\t%3\t%4\n")
      .arg(entry.time.toString(Qt::ISODateWithMs), entry.level, entry.source,
        redact(entry.message)).toUtf8());
  }
  emit changed();
}

QString LogStore::exportText(const QString & source, const QStringList & levels,
  const QString & text, bool filtered) const
{
  QStringList lines;
  const auto query = text.trimmed();
  for (const auto & entry : entries_) {
    if (filtered && !source.isEmpty() && source != "All logs" && entry.source != source) continue;
    if (filtered && !levels.isEmpty() && !levels.contains(entry.level)) continue;
    if (filtered && !query.isEmpty() && !entry.message.contains(query, Qt::CaseInsensitive)) continue;
    lines.append(QString("%1\t%2\t%3\t%4")
      .arg(entry.time.toString(Qt::ISODateWithMs), entry.level, entry.source, redact(entry.message)));
  }
  return lines.join('\n') + (lines.isEmpty() ? QString() : QStringLiteral("\n"));
}

bool run_log_store_contract_test()
{
  LogStore logs;
  logs.append("alpha · run 1", "INFO", "camera online");
  logs.append("alpha · run 1", "ERROR", "camera failed password=hunter2");
  logs.append("ROS /rosout", "WARN", "planner waiting");
  const auto all = logs.exportText();
  const auto filtered = logs.exportText("alpha · run 1", {"INFO"}, "online", true);
  if (all.count('\n') != 3 || filtered.count('\n') != 1 || !filtered.contains("camera online") ||
    filtered.contains("camera failed") || all.contains("hunter2") || !all.contains("[REDACTED]")) return false;
  const auto sources = logs.sources();
  if (!sources.contains("All logs") || !sources.contains("ROS /rosout") || !sources.contains("alpha · run 1")) return false;
  if (LogStore::redact("Authorization: Bearer secret-value").contains("secret-value")) return false;
  if (LogStore::redact("https://user:password@example.test").contains("user:password")) return false;
  if (LogStore::redact("PASSWORD=\"my private value\"").contains("private value")) return false;
  if (LogStore::redact("--password 'spaced value'").contains("spaced value")) return false;

  ProcessManager manager;
  manager.setLogStore(&logs);
  if (!manager.startProcess("contract process", "/bin/sh", {"-c", "printf 'stdout marker\\n'; printf 'password=abc123\\n' >&2"})) return false;
  QElapsedTimer timeout;
  timeout.start();
  while (timeout.elapsed() < 3000) {
    QCoreApplication::processEvents();
    const auto rows = manager.processes();
    if (!rows.isEmpty() && rows.first().toMap().value("state").toString() == "Exited") break;
    QCoreApplication::sendPostedEvents();
  }
  QCoreApplication::processEvents();
  const auto run_source = manager.logSource("contract process");
  if (run_source != "contract process · run 1" || all.contains("stdout marker")) return false;
  const auto process_log = logs.exportText(run_source, {}, {}, true);
  return process_log.contains("stdout marker") && process_log.contains("[REDACTED]") &&
    !process_log.contains("abc123") && manager.processes().first().toMap().value("log").toString().contains("[REDACTED]");
}
