#pragma once

#include <QObject>
#include <QHash>
#include <QProcess>
#include <QVariantList>
#include <QVariantMap>

class LogStore;

class ProcessManager : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariantList processes READ processes NOTIFY changed)
  Q_PROPERTY(QString log READ log NOTIFY logChanged)

public:
  explicit ProcessManager(QObject * parent = nullptr, const QString & ssh_program = "ssh");
  ~ProcessManager() override;

  QVariantList processes() const;
  QString log() const { return log_; }
  Q_INVOKABLE bool startSimulation(const QString & robot);
  Q_INVOKABLE bool startRemoteSimulation(const QString & host, int ros_domain, const QString & robot);
  Q_INVOKABLE bool startRemoteHardware(const QString & host, int ros_domain, const QString & robot,
    const QString & remote_workspace, const QVariantMap & options);
  Q_INVOKABLE bool startGazeboView();
  Q_INVOKABLE bool startProcess(const QString & name, const QString & program,
    const QStringList & arguments);
  Q_INVOKABLE void stopProcess(const QString & name);
  Q_INVOKABLE void stopAll();
  Q_INVOKABLE QString logSource(const QString & name) const;
  void setLogStore(LogStore * store) { log_store_ = store; }

signals:
  void changed();
  void logChanged();

private:
  struct ManagedProcess {
    QProcess * process{};
    QString name;
    QString state{"Stopped"};
    QStringList lines;
    QString log_source;
    int run_number{0};
    int exit_code{-1};
    bool stopping{false};
    bool remote{false};
    QString host;
    QString run_token;
    qint64 remote_pid{-1};
    QByteArray remote_handshake;
    QProcess * stop_process{};
  };
  void appendOutput(ManagedProcess * managed, const QByteArray & bytes, bool stderr_stream);
  bool startRemoteOwnedProcess(const QString & name, const QString & host, int ros_domain,
    const QString & shell_command, const QString & start_log);
  void setState(ManagedProcess * managed, const QString & state);
  void appendLog(const QString & line);
  QHash<QString, ManagedProcess *> managed_;
  QString ssh_program_;
  QString log_;
  LogStore * log_store_{nullptr};
};
