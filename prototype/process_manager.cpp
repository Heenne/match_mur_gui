#include "process_manager.hpp"
#include "log_store.hpp"

#include <QDateTime>
#include <QProcessEnvironment>
#include <QTimer>
#include <QRegularExpression>

#include <csignal>
#include <sys/types.h>
#include <unistd.h>

namespace
{
QStringList simulation_arguments(const QString & robot)
{
  return {"launch", "mur_launch_sim", "mur620.launch.py", "robot_name:=" + robot,
    "include_gz:=true", "gazebo_gui:=false", "launch_rviz:=false"};
}
}

ProcessManager::ProcessManager(QObject * parent) : QObject(parent) {}

ProcessManager::~ProcessManager()
{
  stopAll();
  for (auto * managed : managed_) {
    if (managed->process->state() != QProcess::NotRunning) {
      const auto pid = static_cast<pid_t>(managed->process->processId());
      if (pid > 1) ::kill(-pid, SIGINT);
      if (!managed->process->waitForFinished(8000)) {
        if (pid > 1) ::kill(-pid, SIGTERM);
        if (!managed->process->waitForFinished(2000) && pid > 1) ::kill(-pid, SIGKILL);
        managed->process->waitForFinished(1000);
      }
    }
    delete managed;
  }
}

QVariantList ProcessManager::processes() const
{
  QVariantList result;
  for (auto it = managed_.cbegin(); it != managed_.cend(); ++it) {
    const auto * managed = it.value();
    result.append(QVariantMap{{"name", managed->name}, {"state", managed->state},
      {"exitCode", managed->exit_code}, {"log", managed->lines.join("")}});
  }
  return result;
}

bool ProcessManager::startSimulation(const QString & robot)
{
  static const QRegularExpression valid_robot("^[A-Za-z][A-Za-z0-9_]*$");
  if (!valid_robot.match(robot).hasMatch()) return false;
  return startProcess("MUR620 simulation", "ros2", simulation_arguments(robot));
}

bool ProcessManager::startGazeboView()
{
  return startProcess("Gazebo world view", "gz", {"sim", "-g"});
}

bool ProcessManager::startProcess(const QString & name, const QString & program,
  const QStringList & arguments)
{
  if (name.trimmed().isEmpty() || program.trimmed().isEmpty()) return false;
  auto * managed = managed_.value(name, nullptr);
  if (managed && managed->process->state() != QProcess::NotRunning) return false;
  if (!managed) {
    managed = new ManagedProcess;
    managed->name = name;
    managed->process = new QProcess(this);
    managed_.insert(name, managed);
    connect(managed->process, &QProcess::readyReadStandardOutput, this, [this, managed]() {
      appendOutput(managed, managed->process->readAllStandardOutput(), false);
    });
    connect(managed->process, &QProcess::readyReadStandardError, this, [this, managed]() {
      appendOutput(managed, managed->process->readAllStandardError(), true);
    });
    connect(managed->process,
      qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
      [this, managed](int exit_code, QProcess::ExitStatus status) {
        managed->exit_code = exit_code;
        const auto final_state = managed->stopping ? "Stopped" :
          (status == QProcess::NormalExit && exit_code == 0 ? "Exited" : "Failed");
        managed->stopping = false;
        setState(managed, final_state);
        if (log_store_) log_store_->append(managed->log_source,
          final_state == "Failed" ? "ERROR" : "INFO",
          QString("Process finished with exit code %1 (%2)").arg(exit_code).arg(final_state));
        appendLog(QString("%1 finished with exit code %2").arg(managed->name).arg(exit_code));
      });
    connect(managed->process, &QProcess::errorOccurred, this,
      [this, managed](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
          managed->exit_code = -1;
          setState(managed, "Failed");
          if (log_store_) log_store_->append(managed->log_source, "ERROR",
            QString("Process could not start: %1").arg(managed->process->errorString()));
          appendLog(QString("%1 could not start: %2")
            .arg(managed->name, managed->process->errorString()));
        }
      });
    connect(managed->process, &QProcess::started, this, [this, managed]() {
      if (managed->stopping) {
        const auto pid = static_cast<pid_t>(managed->process->processId());
        if (pid > 1) ::kill(-pid, SIGINT);
        setState(managed, "Stopping");
      } else {
        setState(managed, "Running");
      }
    });
  }
  managed->lines.clear();
  ++managed->run_number;
  managed->log_source = QString("%1 · run %2").arg(name).arg(managed->run_number);
  managed->exit_code = -1;
  managed->stopping = false;
  auto environment = QProcessEnvironment::systemEnvironment();
  // The desktop launcher normally starts us with ROS already sourced. Preserve that
  // environment and make the profile's local ROS domain explicit for ROS commands.
  if (!environment.contains("ROS_DOMAIN_ID")) environment.insert("ROS_DOMAIN_ID", "62");
  managed->process->setProcessEnvironment(environment);
  managed->process->setProgram("/usr/bin/setsid");
  QStringList owned_argv{program};
  owned_argv.append(arguments);
  managed->process->setArguments(owned_argv);
  managed->process->setProcessChannelMode(QProcess::SeparateChannels);
  setState(managed, "Starting");
  appendLog(QString("Starting %1: %2 %3")
    .arg(name, program, arguments.join(' ')));
  if (log_store_) log_store_->append(managed->log_source, "INFO",
    QString("Starting %1: %2 %3").arg(name, program, arguments.join(' ')));
  managed->process->start();
  return true;
}

void ProcessManager::stopProcess(const QString & name)
{
  auto * managed = managed_.value(name, nullptr);
  if (!managed || managed->process->state() == QProcess::NotRunning) return;
  managed->stopping = true;
  // setsid gives this QProcess its own session/process group. Signalling the negative
  // PID reaches only this app-owned launch and the children it created.
  const auto pid = static_cast<pid_t>(managed->process->processId());
  if (pid > 1) ::kill(-pid, SIGINT);
  setState(managed, "Stopping");
  QTimer::singleShot(8000, this, [this, managed]() {
    if (managed->process->state() == QProcess::NotRunning) return;
    const auto pid = static_cast<pid_t>(managed->process->processId());
    if (pid > 1) ::kill(-pid, SIGTERM);
    QTimer::singleShot(2000, this, [managed]() {
      const auto pid = static_cast<pid_t>(managed->process->processId());
      if (managed->process->state() != QProcess::NotRunning && pid > 1) ::kill(-pid, SIGKILL);
    });
  });
}

void ProcessManager::stopAll()
{
  const auto names = managed_.keys();
  for (const auto & name : names) stopProcess(name);
}

QString ProcessManager::logSource(const QString & name) const
{
  const auto * managed = managed_.value(name, nullptr);
  return managed ? managed->log_source : QString();
}

void ProcessManager::appendOutput(ManagedProcess * managed, const QByteArray & bytes,
  bool stderr_stream)
{
  if (bytes.isEmpty()) return;
  const auto prefix = stderr_stream ? "stderr: " : "";
  const auto text = LogStore::redact(QString::fromLocal8Bit(bytes));
  managed->lines.append(text);
  const auto level = stderr_stream ? "ERROR" : "INFO";
  if (log_store_) log_store_->append(managed->log_source, level, text.trimmed());
  appendLog(QString("[%1] %2%3").arg(managed->name, prefix, text.trimmed()));
  emit changed();
}

void ProcessManager::setState(ManagedProcess * managed, const QString & state)
{
  managed->state = state;
  emit changed();
}

void ProcessManager::appendLog(const QString & line)
{
  const auto safe = LogStore::redact(line);
  log_ += QString("%1 %2\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate), safe);
  if (log_store_) log_store_->append("Process manager", "INFO", safe);
  emit logChanged();
}
