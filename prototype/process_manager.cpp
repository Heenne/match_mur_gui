#include "process_manager.hpp"
#include "log_store.hpp"

#include <QDateTime>
#include <QProcessEnvironment>
#include <QTimer>
#include <QRegularExpression>
#include <QUuid>

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

QString shell_quote(const QString & value)
{
  QString escaped = value;
  escaped.replace("'", "'\"'\"'");
  return "'" + escaped + "'";
}

QString remote_owned_command(int domain, const QString & shell_command, const QString & token)
{
  const auto inner = QString("printf '__MUR_GUI_PID__=%s\\n' \"$$\"; "
    "export MUR_GUI_RUN_ID=%1; exec bash -lc %2")
    .arg(shell_quote(token), shell_quote(shell_command));
  const auto script = QString("export ROS_DOMAIN_ID=%1; exec setsid --fork --wait bash -c %2")
    .arg(domain).arg(shell_quote(inner));
  return "bash -lc " + shell_quote(script);
}

QStringList remote_stop_arguments(const QString & host, qint64 pid, const QString & token)
{
  const auto script = QString(
    "pid=%1; token=%2; "
    "if [ \"$pid\" -gt 1 ] && [ -r \"/proc/$pid/environ\" ] && "
    "tr '\\0' '\\n' < \"/proc/$pid/environ\" | grep -Fxq \"MUR_GUI_RUN_ID=$token\" && "
    "[ \"$(ps -o pgid= -p \"$pid\" | tr -d ' ')\" = \"$pid\" ]; then "
    "kill -INT -- \"-$pid\"; else echo 'Owned remote run not found; no process was signalled' >&2; exit 1; fi")
    .arg(pid).arg(shell_quote(token));
  return {"-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", host,
    "bash -lc " + shell_quote(script)};
}
}

ProcessManager::ProcessManager(QObject * parent, const QString & ssh_program)
: QObject(parent), ssh_program_(ssh_program) {}

ProcessManager::~ProcessManager()
{
  stopAll();
  for (auto * managed : managed_) {
    if (managed->stop_process) managed->stop_process->waitForFinished(10000);
    if (managed->process->state() != QProcess::NotRunning) {
      const auto pid = static_cast<pid_t>(managed->process->processId());
      if (managed->remote) {
        managed->process->terminate();
        if (!managed->process->waitForFinished(8000)) managed->process->kill();
      } else {
        if (pid > 1) ::kill(-pid, SIGINT);
        if (!managed->process->waitForFinished(8000)) {
          if (pid > 1) ::kill(-pid, SIGTERM);
          if (!managed->process->waitForFinished(2000) && pid > 1) ::kill(-pid, SIGKILL);
          managed->process->waitForFinished(1000);
        }
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
      {"exitCode", managed->exit_code}, {"log", managed->lines.join("")},
      {"host", managed->host}});
  }
  return result;
}

bool ProcessManager::startSimulation(const QString & robot)
{
  static const QRegularExpression valid_robot("^[A-Za-z][A-Za-z0-9_]*$");
  if (!valid_robot.match(robot).hasMatch()) return false;
  return startProcess("MUR620 simulation", "ros2", simulation_arguments(robot));
}

bool ProcessManager::startRemoteSimulation(const QString & host, int ros_domain, const QString & robot)
{
  static const QRegularExpression valid_host("^[A-Za-z0-9][A-Za-z0-9.-]*$");
  static const QRegularExpression valid_robot("^[A-Za-z][A-Za-z0-9_]*$");
  const auto trimmed_host = host.trimmed();
  if (!valid_host.match(trimmed_host).hasMatch() || trimmed_host == "localhost" ||
    ros_domain < 0 || ros_domain > 232 || !valid_robot.match(robot).hasMatch()) return false;
  const auto command = QString("ros2 launch mur_launch_sim mur620.launch.py robot_name:=%1 "
    "include_gz:=true gazebo_gui:=false launch_rviz:=false").arg(shell_quote(robot));
  return startRemoteOwnedProcess("Remote MUR620 simulation", trimmed_host, ros_domain,
    command, QString("Starting remote MuR simulation on %1 · domain %2 · robot %3")
      .arg(trimmed_host).arg(ros_domain).arg(robot));
}

bool ProcessManager::startRemoteHardware(const QString & host, int ros_domain, const QString & robot,
  const QString & remote_workspace, const QVariantMap & options)
{
  static const QRegularExpression valid_host("^[A-Za-z0-9][A-Za-z0-9.-]*$");
  static const QRegularExpression valid_robot("^[A-Za-z][A-Za-z0-9_]*$");
  static const QRegularExpression valid_path("^(\\$HOME|~|/[A-Za-z0-9_./-]+)(/[A-Za-z0-9_./-]*)?$");
  const auto trimmed_host = host.trimmed();
  const auto workspace = remote_workspace.trimmed().isEmpty() ? "$HOME/colcon_ws" : remote_workspace.trimmed();
  if (!valid_host.match(trimmed_host).hasMatch() || trimmed_host == "localhost" ||
    ros_domain < 0 || ros_domain > 232 || !valid_robot.match(robot).hasMatch() ||
    !valid_path.match(workspace).hasMatch()) return false;
  QStringList arms;
  if (options.value("arm_l", false).toBool()) arms << "UR10_l";
  if (options.value("arm_r", false).toBool()) arms << "UR10_r";
  if (arms.isEmpty() && !options.value("launch_mir", false).toBool()) return false;
  const auto localization = options.value("localization_source", "mir").toString();
  if (localization != "mir" && localization != "mocap" && localization != "amcl") return false;
  QStringList args{
    "mir_localization_source:=" + localization,
    "robot_name:=" + robot, "robot_profile:=" + robot,
    QString("launch_ur_r:=%1").arg(options.value("arm_r", false).toBool() ? "true" : "false"),
    QString("launch_ur_l:=%1").arg(options.value("arm_l", false).toBool() ? "true" : "false"),
    QString("launch_mir:=%1").arg(options.value("launch_mir", false).toBool() ? "true" : "false"),
    QString("launch_mir_cameras:=%1").arg(options.value("launch_cameras", false).toBool() ? "true" : "false"),
    QString("integrated_controller_enable_collision_avoidance:=%1").arg(options.value("collision_avoidance", true).toBool() ? "true" : "false"),
    QString("integrated_controller_publish_collision_markers:=%1").arg(options.value("collision_markers", false).toBool() ? "true" : "false"),
    QString("launch_moveit:=%1").arg(options.value("launch_moveit", true).toBool() ? "true" : "false"),
    "launch_bms:=true", "bms_can_interface:=can0", "bms_can_bitrate:=250000",
    "auto_switch_moveit_controllers:=true", "launch_moveit_rviz:=false"};
  if (options.value("zero_admittance", false).toBool()) {
    args << "integrated_controller_admittance:=0.0 0.0 0.0 0.0 0.0 0.0"
      << "integrated_controller_wrench_twist_gain:=0.0 0.0 0.0 0.0 0.0 0.0";
  }
  if (options.value("integrated_controller", true).toBool()) {
    args << "use_integrated_cartesian_admittance_controller:=true"
      << "integrated_controller_initial_active:=false" << "activate_joint_controller:=false"
      << QString("integrated_controller_use_ft_sensor:=%1").arg(options.value("use_ft_sensor", true).toBool() ? "true" : "false")
      << QString("integrated_controller_require_wrench:=%1").arg(options.value("require_wrench", false).toBool() ? "true" : "false")
      << "launch_integrated_cartesian_move_action:=true"
      << "launch_arm_velocity_safety:=false" << "launch_jparse_idk:=false";
  } else {
    args << "use_integrated_cartesian_admittance_controller:=false";
  }
  QString workspace_expression = shell_quote(workspace);
  if (workspace.startsWith("$HOME")) workspace_expression = "\"${HOME}" + workspace.mid(5) + "\"";
  else if (workspace.startsWith("~")) workspace_expression = "\"${HOME}" + workspace.mid(1) + "\"";
  QStringList quoted_args;
  for (const auto & arg : args) quoted_args << shell_quote(arg);
  QString command = QString("workspace=%1; repo=\"$workspace/src/match_mobile_robotics_jazzy\"; "
    "export WS=\"$workspace\" REPO=\"$repo\" ROS_DOMAIN_ID=%2 "
    "ROBOT_PROFILE=%3 BUILD_BEFORE_LAUNCH=false CLEAN_START=false "
    "MUR_REQUIRE_HOST_PREFLIGHT=true MUR_CHECK_UR_NETWORK=%4 MUR_UR_HOSTS=%5; "
    "exec \"$repo/start_mur620_hardware_logged.sh\" %6")
    .arg(workspace_expression).arg(ros_domain).arg(shell_quote(robot))
    .arg(arms.isEmpty() ? "false" : "true").arg(shell_quote(arms.join(' ')))
    .arg(quoted_args.join(' '));
  return startRemoteOwnedProcess("Hardware · " + robot, trimmed_host, ros_domain, command,
    QString("Starting hardware on %1 · robot %2 · arms %3 · MiR %4")
      .arg(trimmed_host, robot, arms.isEmpty() ? "none" : arms.join(", "),
        options.value("launch_mir", false).toBool() ? "enabled" : "disabled"));
}

bool ProcessManager::startRemoteOwnedProcess(const QString & process_name, const QString & host,
  int ros_domain, const QString & shell_command, const QString & start_log)
{
  auto * managed = managed_.value(process_name, nullptr);
  if (managed && managed->process->state() != QProcess::NotRunning) return false;
  if (!managed) {
    managed = new ManagedProcess;
    managed->name = process_name;
    managed->remote = true;
    managed->process = new QProcess(this);
    managed_.insert(managed->name, managed);
    connect(managed->process, &QProcess::readyReadStandardOutput, this, [this, managed]() {
      appendOutput(managed, managed->process->readAllStandardOutput(), false);
    });
    connect(managed->process, &QProcess::readyReadStandardError, this, [this, managed]() {
      appendOutput(managed, managed->process->readAllStandardError(), true);
    });
    connect(managed->process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
      [this, managed](int exit_code, QProcess::ExitStatus status) {
        if (!managed->remote_handshake.isEmpty()) {
          appendOutput(managed, managed->remote_handshake, false);
          managed->remote_handshake.clear();
        }
        managed->exit_code = exit_code;
        const auto final_state = managed->remote && managed->stopping && managed->remote_pid <= 1
          ? "Stop unconfirmed" : managed->stopping ? "Stopped" :
          (status == QProcess::NormalExit && exit_code == 0 ? "Exited" : "Failed");
        managed->stopping = false;
        setState(managed, final_state);
        appendLog(QString("%1 on %2 finished with exit code %3 (%4)")
          .arg(managed->name, managed->host).arg(exit_code).arg(final_state));
        if (final_state == "Stop unconfirmed") {
          appendLog(QString("No owned-process handshake was received from %1; remote stop was not confirmed")
            .arg(managed->host));
        }
      });
    connect(managed->process, &QProcess::errorOccurred, this, [this, managed](QProcess::ProcessError error) {
      if (error == QProcess::FailedToStart) {
        managed->exit_code = -1;
        setState(managed, "Failed");
        appendLog(QString("Remote process on %1 could not start: %2")
          .arg(managed->host, managed->process->errorString()));
      }
    });
  }
  managed->remote = true;
  managed->host = host;
  managed->run_token = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
  managed->remote_pid = -1;
  managed->remote_handshake.clear();
  managed->stopping = false;
  managed->lines.clear();
  managed->exit_code = -1;
  ++managed->run_number;
  managed->log_source = QString("%1 · %2 · run %3")
    .arg(managed->name, managed->host).arg(managed->run_number);
  auto environment = QProcessEnvironment::systemEnvironment();
  managed->process->setProcessEnvironment(environment);
  managed->process->setProgram(ssh_program_);
  managed->process->setArguments({"-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8",
      host, remote_owned_command(ros_domain, shell_command, managed->run_token)});
  managed->process->setProcessChannelMode(QProcess::SeparateChannels);
  setState(managed, "Connecting");
  appendLog(start_log);
  if (log_store_) log_store_->append(managed->log_source, "INFO", start_log);
  managed->process->start();
  return true;
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
  if (managed->remote) {
    if (managed->stopping && managed->stop_process) return;
    if (managed->remote_pid <= 1) {
      managed->stopping = true;
      setState(managed, "Stopping");
      return;
    }
    managed->stopping = true;
    setState(managed, "Stopping");
    managed->stop_process = new QProcess(this);
    auto * stop_process = managed->stop_process;
    connect(managed->stop_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
      this, [this, managed, stop_process](int code, QProcess::ExitStatus status) {
        if (managed->stop_process != stop_process) return;
        if (status != QProcess::NormalExit || code != 0) {
          managed->stopping = false;
          managed->state = "Stop failed";
          appendLog(QString("Could not confirm stop of app-owned remote run on %1; remote process may still be running")
            .arg(managed->host));
          emit changed();
        } else {
          appendLog(QString("Stop signal sent to app-owned remote run %1 on %2")
            .arg(managed->remote_pid).arg(managed->host));
        }
        managed->stop_process->deleteLater();
        managed->stop_process = nullptr;
      });
    connect(managed->stop_process, &QProcess::errorOccurred, this,
      [this, managed, stop_process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || managed->stop_process != stop_process) return;
        managed->stopping = false;
        managed->state = "Stop failed";
        appendLog(QString("SSH stop command could not start for %1; remote process may still be running")
          .arg(managed->host));
        emit changed();
        managed->stop_process->deleteLater();
        managed->stop_process = nullptr;
      });
    managed->stop_process->setProgram(ssh_program_);
    managed->stop_process->setArguments(remote_stop_arguments(
      managed->host, managed->remote_pid, managed->run_token));
    managed->stop_process->start();
    return;
  }
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
  if (managed->remote && !stderr_stream) {
    managed->remote_handshake.append(bytes);
    while (true) {
      const auto newline = managed->remote_handshake.indexOf('\n');
      if (newline < 0) break;
      auto line = managed->remote_handshake.left(newline).trimmed();
      managed->remote_handshake.remove(0, newline + 1);
      static const QRegularExpression pid_marker("^__MUR_GUI_PID__=([0-9]+)$");
      const auto match = pid_marker.match(QString::fromUtf8(line));
      if (match.hasMatch()) {
        managed->remote_pid = match.captured(1).toLongLong();
        if (managed->remote_pid <= 1) {
          setState(managed, "Failed");
          appendLog("Remote launch returned an invalid owned-process identifier");
          managed->process->kill();
          return;
        }
        setState(managed, managed->stopping ? "Stopping" : "Running");
        if (managed->stopping) {
          managed->stopping = false;
          stopProcess(managed->name);
        }
      } else {
        const auto text = LogStore::redact(QString::fromUtf8(line + "\n"));
        managed->lines.append(text);
        if (log_store_) log_store_->append(managed->log_source, "INFO", text.trimmed());
        appendLog(QString("[%1@%2] %3").arg(managed->name, managed->host, text.trimmed()));
        emit changed();
      }
    }
    return;
  }
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

bool ProcessManager::startRemoteOwnedCommand(const QString & name, const QString & host,
  int ros_domain, const QString & command, const QString & start_log)
{
  static const QRegularExpression valid_host("^[A-Za-z0-9][A-Za-z0-9.-]*$");
  if (name.trimmed().isEmpty() || !valid_host.match(host).hasMatch() ||
      host == "localhost" || ros_domain < 0 || ros_domain > 232 || command.isEmpty()) return false;
  return startRemoteOwnedProcess(name, host, ros_domain, command, start_log);
}
