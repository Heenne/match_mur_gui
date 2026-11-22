#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class LogStore;

class ProcessManager : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariantList processes READ processes NOTIFY changed)
  Q_PROPERTY(QString log READ log NOTIFY logChanged)
  Q_PROPERTY(QString gazeboServerStatus READ gazeboServerStatus NOTIFY gazeboServerStatusChanged)
  Q_PROPERTY(QVariantList simulationWorlds READ simulationWorlds CONSTANT)
  Q_PROPERTY(int simulationDomain READ simulationDomain NOTIFY changed)

public:
  explicit ProcessManager(QObject * parent = nullptr, const QString & ssh_program = "ssh");
  ~ProcessManager() override;

  QVariantList processes() const;
  QString log() const { return log_; }
  QString gazeboServerStatus() const { return gazebo_server_status_; }
  QVariantList simulationWorlds() const;
  int simulationDomain() const { return simulation_domain_; }
  Q_INVOKABLE bool startSimulationWorld(const QString & world_file, const QString & map_file, int domain);
  Q_INVOKABLE bool spawnSimulationRobot(const QString & robot, const QString & model, double x, double y, double yaw);
  Q_INVOKABLE void probeGazeboServer();
  Q_INVOKABLE bool startSimulation(const QString & robot);
  Q_INVOKABLE bool startRemoteSimulation(const QString & host, int ros_domain, const QString & robot);
  Q_INVOKABLE bool startRemoteHardware(const QString & host, int ros_domain, const QString & robot,
    const QString & remote_workspace, const QVariantMap & options);
  bool startRemoteOwnedCommand(const QString & name, const QString & host, int ros_domain,
    const QString & command, const QString & start_log);
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
  void gazeboServerStatusChanged();

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
    QProcess * removal_process{};
  };
  void appendOutput(ManagedProcess * managed, const QByteArray & bytes, bool stderr_stream);
  bool startLocalProcess(const QString & name, const QString & program, const QStringList & arguments,
    const QProcessEnvironment & environment);
  QProcessEnvironment gazeboEnvironment() const;
  void stopWorldChildren();
  void onLocalProcessFinished(ManagedProcess * managed, const QString & state);
  bool startRemoteOwnedProcess(const QString & name, const QString & host, int ros_domain,
    const QString & shell_command, const QString & start_log);
  void setState(ManagedProcess * managed, const QString & state);
  void appendLog(const QString & line);
  void setGazeboServerStatus(const QString & status, const QString & error = {});
  QHash<QString, ManagedProcess *> managed_;
  QString ssh_program_;
  QString log_;
  QProcess gazebo_probe_;
  QTimer gazebo_probe_timeout_;
  QElapsedTimer gazebo_probe_age_;
  QString gazebo_server_status_{"Not checked"};
  QString gazebo_partition_;
  QString probe_partition_;
  QString simulation_world_name_;
  QString simulation_world_file_;
  int simulation_domain_{-1};
  LogStore * log_store_{nullptr};
};
