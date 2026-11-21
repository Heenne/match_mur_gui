#include "hardware_start.hpp"
#include "process_manager.hpp"
#include "log_store.hpp"

#include <QSettings>
#include <QRegularExpression>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <memory>
#include <utility>

namespace
{
QStringList selected_arms(const QVariantMap & options)
{
  QStringList arms;
  if (options.value("arm_l", true).toBool()) arms << "UR10_l";
  if (options.value("arm_r", true).toBool()) arms << "UR10_r";
  return arms;
}

QString quote_shell(const QString & value)
{
  QString escaped = value;
  escaped.replace("'", "'\"'\"'");
  return "'" + escaped + "'";
}

QString workspace_expression(const QString & configured)
{
  const auto path = configured.trimmed().isEmpty() ? QString("$HOME/colcon_ws") : configured.trimmed();
  static const QRegularExpression safe_path("^(\\$HOME|~|/[A-Za-z0-9_./-]+)(/[A-Za-z0-9_./-]*)?$");
  if (!safe_path.match(path).hasMatch()) return {};
  if (path.startsWith("$HOME")) return "\"${HOME}" + path.mid(5) + "\"";
  if (path.startsWith("~")) return "\"${HOME}" + path.mid(1) + "\"";
  return quote_shell(path);
}

QString ssh_shell_command(const QString & host, const QString & command)
{
  Q_UNUSED(host)
  return "bash -lc " + quote_shell(command);
}
}

SshHardwareStartAdapter::SshHardwareStartAdapter(ProcessManager * processes, LogStore * logs,
  const QString & ssh_program, const QString & rsync_program)
: QObject(nullptr), processes_(processes), logs_(logs), ssh_program_(ssh_program), rsync_program_(rsync_program) {}

void SshHardwareStartAdapter::runPreflight(const HardwareStartConfig & config,
  PreflightCallback callback)
{
  const auto workspace = workspace_expression(config.remote_workspace);
  if (workspace.isEmpty()) {
    HardwarePreflightResult result;
    result.host_detail = "Remote workspace must be $HOME-relative or an absolute path without shell metacharacters.";
    for (const auto & arm : config.arms) result.arms.insert(arm,
      {"Unknown", "Dashboard safety could not be checked because the workspace path is invalid."});
    callback(result);
    return;
  }
  const auto arm_names = config.arms.join(' ');
  const auto command = QString("workspace=%1; repo=\"$workspace/src/match_mobile_robotics_jazzy\"; "
    "test -x \"$repo/setup_mur_hardware_host.sh\" || { echo 'MUR_GUI_PREFLIGHT: status=unknown reason=setup_script_missing'; exit 127; }; "
    "WS=\"$workspace\" REPO=\"$repo\" MUR_CHECK_UR_NETWORK=%2 MUR_UR_HOSTS=%3 "
    "\"$repo/setup_mur_hardware_host.sh\" --check --profile %4")
    .arg(workspace).arg(config.arms.isEmpty() ? "false" : "true")
    .arg(quote_shell(arm_names)).arg(quote_shell(config.robot));
  auto * process = new QProcess(this);
  auto output = std::make_shared<QByteArray>();
  auto completion = std::make_shared<PreflightCallback>(std::move(callback));
  auto collect = [this, process, output](bool stderr_stream) {
    const auto bytes = stderr_stream ? process->readAllStandardError() : process->readAllStandardOutput();
    if (bytes.isEmpty()) return;
    output->append(bytes);
    if (logs_) {
      const auto level = stderr_stream ? "ERROR" : "INFO";
      for (const auto & line : QString::fromLocal8Bit(bytes).split('\n', Qt::SkipEmptyParts)) {
        logs_->append("Hardware preflight · " + process->property("host").toString(), level, line);
      }
    }
  };
  process->setProperty("host", config.host);
  QObject::connect(process, &QProcess::readyReadStandardOutput, process, [collect]() { collect(false); });
  QObject::connect(process, &QProcess::readyReadStandardError, process, [collect]() { collect(true); });
  QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
    [process, output, config, completion](int exit_code, QProcess::ExitStatus status) mutable {
      HardwarePreflightResult result;
      const auto text = QString::fromLocal8Bit(*output);
      const bool summary_ok = text.contains("MUR_HOST_CHECK: summary=ok blocking=0");
      result.host_ready = status == QProcess::NormalExit && exit_code == 0 && summary_ok;
      result.host_state = result.host_ready ? "Ready" :
        (text.contains("MUR_HOST_CHECK: status=fail") ? "Blocked" : "Unknown");
      result.host_detail = text.trimmed().isEmpty() ?
        (result.host_ready ? "Host setup, permissions and selected network checks passed." :
          "Host preflight returned no diagnostic output; readiness is unknown.") : text.trimmed().right(1200);
      for (const auto & arm : config.arms) {
        if (text.contains("MUR_HOST_CHECK: status=ok issue=ur_dashboard_state host=" + arm)) {
          result.arms.insert(arm, {"Ready", "Dashboard safety and remote-control state were confirmed."});
        } else if (text.contains("host=" + arm + " detail=dashboard_unreachable") ||
          text.contains("issue=ur_safety host=" + arm) ||
          text.contains("issue=ur_remote_control host=" + arm) ||
          text.contains("issue=ur_robotmode host=" + arm)) {
          result.arms.insert(arm, {"Blocked", "UR dashboard reported an unreachable robot or blocking safety/mode state."});
        } else {
          result.arms.insert(arm, {"Unknown", "The dashboard did not return a confirmed safety state."});
        }
      }
      if (*completion) { auto done = std::move(*completion); done(result); }
      process->deleteLater();
    });
  QObject::connect(process, &QProcess::errorOccurred, process,
    [process, completion, config](QProcess::ProcessError error) mutable {
      if (error != QProcess::FailedToStart) return;
      HardwarePreflightResult result;
      result.host_detail = "Could not start SSH preflight: " + process->errorString();
      for (const auto & arm : config.arms) result.arms.insert(arm,
        {"Unknown", "No safety result was received because SSH did not start."});
      if (*completion) { auto done = std::move(*completion); done(result); }
      process->deleteLater();
    });
  process->setProgram(ssh_program_);
  process->setArguments({"-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", config.host,
    ssh_shell_command(config.host, command)});
  process->start();
}

void SshHardwareStartAdapter::startHardware(const HardwareStartConfig & config,
  ActionCallback callback)
{
  QVariantMap options{{"arm_l", config.arms.contains("UR10_l")}, {"arm_r", config.arms.contains("UR10_r")},
    {"launch_mir", config.launch_mir}, {"launch_cameras", config.launch_cameras},
    {"localization_source", config.localization_source},
    {"integrated_controller", config.integrated_controller}, {"use_ft_sensor", config.use_ft_sensor},
    {"require_wrench", config.require_wrench}, {"collision_avoidance", config.collision_avoidance},
    {"collision_markers", config.collision_markers}, {"zero_admittance", config.zero_admittance},
    {"launch_moveit", config.launch_moveit}};
  const bool started = processes_ && processes_->startRemoteHardware(config.host, config.ros_domain,
    config.robot, config.remote_workspace, options);
  callback({started, started ? "Hardware launch is tracked in Processes with its SSH output." :
    "Hardware launch could not be started; check for an existing app-owned run and the target configuration."});
}

void SshHardwareStartAdapter::syncSource(const HardwareStartConfig & config, ActionCallback callback)
{
  if (config.local_workspace.trimmed().isEmpty()) {
    callback({false, "Source sync is unavailable until a local workspace path is configured in this preset."});
    return;
  }
  const auto source = QDir(config.local_workspace).filePath("src/");
  const auto remote_workspace = config.remote_workspace.trimmed().isEmpty() ? QString("$HOME/colcon_ws") : config.remote_workspace.trimmed();
  if (workspace_expression(remote_workspace).isEmpty()) {
    callback({false, "Remote workspace path is invalid; source sync was not started."});
    return;
  }
  const auto source_info = QFileInfo(source);
  if (!source_info.exists() || !source_info.isDir()) {
    callback({false, "Local workspace source directory does not exist: " + source});
    return;
  }
  auto destination_workspace = remote_workspace;
  if (destination_workspace.startsWith("$HOME")) destination_workspace = "~" + destination_workspace.mid(5);
  const auto destination = destination_workspace + "/src/";
  auto * process = new QProcess(this);
  auto output = std::make_shared<QByteArray>();
  auto completion = std::make_shared<ActionCallback>(std::move(callback));
  process->setProgram(rsync_program_);
  process->setArguments({"-a", "--exclude=build/", "--exclude=install/", "--exclude=log/",
    "--exclude=.git/", source, config.host + ":" + destination});
  QObject::connect(process, &QProcess::readyReadStandardOutput, process, [this, process, output]() {
    const auto data = process->readAllStandardOutput(); output->append(data);
    if (logs_ && !data.isEmpty()) logs_->append("Hardware source sync · " + process->property("host").toString(), "INFO", QString::fromLocal8Bit(data).trimmed());
  });
  QObject::connect(process, &QProcess::readyReadStandardError, process, [this, process, output]() {
    const auto data = process->readAllStandardError(); output->append(data);
    if (logs_ && !data.isEmpty()) logs_->append("Hardware source sync · " + process->property("host").toString(), "ERROR", QString::fromLocal8Bit(data).trimmed());
  });
  process->setProperty("host", config.host);
  QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
    [process, output, completion](int code, QProcess::ExitStatus status) mutable {
      if (*completion) { auto done = std::move(*completion); done({status == QProcess::NormalExit && code == 0,
        status == QProcess::NormalExit && code == 0 ? "Source sync finished." : QString("Source sync failed: %1").arg(QString::fromLocal8Bit(*output).trimmed().right(1000))});
      }
      process->deleteLater();
    });
  QObject::connect(process, &QProcess::errorOccurred, process,
    [process, completion](QProcess::ProcessError error) mutable {
      if (error != QProcess::FailedToStart) return;
      if (*completion) { auto done = std::move(*completion); done({false, "Could not start rsync: " + process->errorString()}); }
      process->deleteLater();
    });
  process->start();
}

void SshHardwareStartAdapter::buildWorkspace(const HardwareStartConfig & config, ActionCallback callback)
{
  const auto workspace = workspace_expression(config.remote_workspace);
  if (workspace.isEmpty()) {
    callback({false, "Remote workspace path is invalid."});
    return;
  }
  const auto command = QString("workspace=%1; source /opt/ros/jazzy/setup.bash && "
    "cd \"$workspace\" && colcon build --base-paths \"$workspace/src\" --symlink-install "
    "--packages-up-to serial ewellix_driver mur_control mur_moveit_config mur_launch_hardware "
    "match_mur_gui mir_launch_hardware mir_driver mir_msgs mir_srvs mir_restapi sdc21x0")
    .arg(workspace);
  runRemoteAction("Hardware workspace build", config, command, std::move(callback));
}

void SshHardwareStartAdapter::runRemoteAction(const QString & name,
  const HardwareStartConfig & config, const QString & shell_command, ActionCallback callback)
{
  auto * process = new QProcess(this);
  auto output = std::make_shared<QByteArray>();
  auto completion = std::make_shared<ActionCallback>(std::move(callback));
  process->setProperty("host", config.host);
  auto collect = [this, process, output, name](bool stderr_stream) {
    const auto data = stderr_stream ? process->readAllStandardError() : process->readAllStandardOutput();
    output->append(data);
    if (logs_ && !data.isEmpty()) {
      const auto level = stderr_stream ? "ERROR" : "INFO";
      for (const auto & line : QString::fromLocal8Bit(data).split('\n', Qt::SkipEmptyParts))
        logs_->append(name + " · " + process->property("host").toString(), level, line);
    }
  };
  QObject::connect(process, &QProcess::readyReadStandardOutput, process, [collect]() { collect(false); });
  QObject::connect(process, &QProcess::readyReadStandardError, process, [collect]() { collect(true); });
  QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
    [process, output, completion, name](int code, QProcess::ExitStatus status) mutable {
      const bool ok = status == QProcess::NormalExit && code == 0;
      if (*completion) { auto done = std::move(*completion); done({ok, ok ? name + " completed." : QString("%1 failed: %2").arg(name, QString::fromLocal8Bit(*output).trimmed().right(1000))}); }
      process->deleteLater();
    });
  QObject::connect(process, &QProcess::errorOccurred, process,
    [process, completion, name](QProcess::ProcessError error) mutable {
      if (error != QProcess::FailedToStart) return;
      if (*completion) { auto done = std::move(*completion); done({false, QString("%1 could not start: %2").arg(name, process->errorString())}); }
      process->deleteLater();
    });
  process->setProgram(ssh_program_);
  process->setArguments({"-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", config.host,
    ssh_shell_command(config.host, shell_command)});
  process->start();
}

HardwareStartController::HardwareStartController(std::shared_ptr<HardwareStartAdapter> adapter,
  QObject * parent, const QString & settings_path)
: QObject(parent), adapter_(std::move(adapter))
{
  if (settings_path.isEmpty()) {
    settings_ = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
      "MATCH", "MuRCommandCenter");
  } else {
    settings_ = std::make_unique<QSettings>(settings_path, QSettings::IniFormat);
  }
  options_ = defaultOptions();
}

QVariantMap HardwareStartController::defaultOptions() const
{
  return {{"arm_l", true}, {"arm_r", true}, {"launch_mir", false},
    {"launch_cameras", false}, {"integrated_controller", true}, {"use_ft_sensor", true},
    {"require_wrench", false}, {"collision_avoidance", true}, {"collision_markers", false},
    {"zero_admittance", false}, {"launch_moveit", true}, {"moveit_speed_percent", 20},
    {"remote_workspace", "$HOME/colcon_ws"}, {"localization_source", "mir"}};
}

void HardwareStartController::setContext(const QString & preset, const QString & mode,
  const QString & host, int ros_domain, const QString & robot,
  const QString & connection_state, const QString & local_workspace,
  const QString & remote_workspace)
{
  const auto old_fingerprint = contextFingerprint();
  const auto old_preset = preset_;
  const auto old_target = host_ + "|" + robot_ + "|" + QString::number(ros_domain_) + "|" + connection_state_;
  preset_ = preset.trimmed();
  mode_ = mode.trimmed();
  host_ = host.trimmed();
  ros_domain_ = ros_domain;
  robot_ = robot.trimmed();
  connection_state_ = connection_state.trimmed();
  local_workspace_ = local_workspace.trimmed();

  if (old_preset != preset_) {
    options_ = defaultOptions();
    settings_->beginGroup(QString("hardwareLaunch/%1").arg(preset_));
    for (auto it = options_.begin(); it != options_.end(); ++it) {
      if (settings_->contains(it.key())) it.value() = settings_->value(it.key());
    }
    settings_->endGroup();
  }
  remote_workspace_ = options_.value("remote_workspace", remote_workspace.trimmed()).toString();
  if (old_target != host_ + "|" + robot_ + "|" + QString::number(ros_domain_) + "|" + connection_state_)
    localization_statuses_.clear();
  if (old_fingerprint != contextFingerprint()) invalidatePreflight(
    "Target, robot, connection or arm selection changed; run preflight again.");
  emit changed();
}

void HardwareStartController::setOption(const QString & name, const QVariant & value)
{
  if (!options_.contains(name)) return;
  QVariant normalized = value;
  if (name == "localization_source") {
    const auto source = value.toString().trimmed();
    if (source != "mir" && source != "mocap" && source != "amcl") return;
    normalized = source;
  }
  if (name == "moveit_speed_percent") {
    bool ok = false;
    const int speed = value.toInt(&ok);
    if (!ok) return;
    normalized = std::max(1, std::min(100, speed));
  }
  if (options_.value(name) == normalized) return;
  options_.insert(name, normalized);
  if (name == "remote_workspace") remote_workspace_ = normalized.toString().trimmed();
  saveOptions();
  if (name == "arm_l" || name == "arm_r" || name == "remote_workspace" ||
    name == "localization_source") invalidatePreflight(
    "Hardware start options changed; run preflight again.");
  emit changed();
}

void HardwareStartController::setLocalizationStatus(const QString & source, const QString & state,
  const QString & detail)
{
  if (source != "mir" && source != "mocap" && source != "amcl") return;
  static const QStringList allowed_states{"Ready", "Blocked", "Waiting", "Unknown"};
  if (!allowed_states.contains(state)) return;
  const auto status = QVariantMap{{"state", state}, {"detail", detail}};
  if (localization_statuses_.value(source) == status) return;
  localization_statuses_.insert(source, status);
  emit changed();
}

QString HardwareStartController::localizationState() const
{
  const auto source = options_.value("localization_source", "mir").toString();
  return localization_statuses_.value(source).value("state", "Unknown").toString();
}

QString HardwareStartController::localizationDetail() const
{
  const auto source = options_.value("localization_source", "mir").toString();
  return localization_statuses_.value(source).value("detail",
    source == "amcl" ? "AMCL source adapter is not available in this command center; existing AMCL sessions cannot yet be verified." :
    source == "mocap" ? "Waiting for a fresh calibrated Qualisys map pose and map → mocap TF." :
    "MiR localization data has not been confirmed; it may become available after launch.").toString();
}

HardwareStartConfig HardwareStartController::config() const
{
  HardwareStartConfig result;
  result.preset = preset_;
  result.mode = mode_;
  result.host = host_;
  result.ros_domain = ros_domain_;
  result.robot = robot_;
  result.local_workspace = local_workspace_;
  result.remote_workspace = remote_workspace_;
  result.arms = selected_arms(options_);
  result.launch_mir = options_.value("launch_mir").toBool();
  result.launch_cameras = options_.value("launch_cameras").toBool();
  result.integrated_controller = options_.value("integrated_controller").toBool();
  result.use_ft_sensor = options_.value("use_ft_sensor").toBool();
  result.require_wrench = options_.value("require_wrench").toBool();
  result.collision_avoidance = options_.value("collision_avoidance").toBool();
  result.collision_markers = options_.value("collision_markers").toBool();
  result.zero_admittance = options_.value("zero_admittance").toBool();
  result.launch_moveit = options_.value("launch_moveit").toBool();
  result.moveit_speed_percent = options_.value("moveit_speed_percent").toInt();
  result.localization_source = options_.value("localization_source", "mir").toString();
  return result;
}

QString HardwareStartController::contextFingerprint() const
{
  return QString("%1|%2|%3|%4|%5|%6|%7|%8")
    .arg(preset_, mode_, host_).arg(ros_domain_)
    .arg(robot_, connection_state_, selected_arms(options_).join(','),
      remote_workspace_ + "|" + options_.value("localization_source", "mir").toString());
}

void HardwareStartController::invalidatePreflight(const QString & reason)
{
  preflight_ready_ = false;
  checked_context_.clear();
  if (!busy_) state_ = "Preflight required";
  message_ = reason;
  checks_.clear();
}

void HardwareStartController::runPreflight()
{
  if (busy_) return;
  if (!adapter_ || mode_ != "Hardware" || connection_state_ != "Connected" ||
    host_.isEmpty() || robot_.isEmpty() || ros_domain_ < 0 || ros_domain_ > 232) {
    invalidatePreflight("Preflight requires a connected Hardware session with a valid target, robot and ROS domain.");
    state_ = "Blocked";
    emit changed();
    return;
  }
  busy_ = true;
  preflight_ready_ = false;
  state_ = "Checking";
  message_ = QString("Checking %1 and selected UR arms…").arg(host_);
  checks_ = {QVariantMap{{"name", "Host setup and hardware interfaces"}, {"state", "Checking"},
      {"detail", "Verifying ROS install, permissions, realtime limits, CAN and workspace"}}};
  for (const auto & arm : selected_arms(options_)) {
    checks_.append(QVariantMap{{"name", arm + " dashboard safety"}, {"state", "Checking"},
      {"detail", "Safety status must be confirmed before launch"}});
  }
  emit changed();
  const auto expected_context = contextFingerprint();
  adapter_->runPreflight(config(), [this, expected_context](const HardwarePreflightResult & result) {
    busy_ = false;
    if (expected_context != contextFingerprint()) {
      invalidatePreflight("Session changed while preflight was running; results were discarded.");
      emit changed();
      return;
    }
    checks_.clear();
    checks_.append(QVariantMap{{"name", "Host setup and hardware interfaces"},
      {"state", result.host_ready ? "Ready" : result.host_state}, {"detail", result.host_detail}});
    bool ready = result.host_ready;
    for (const auto & arm : selected_arms(options_)) {
      const auto check = result.arms.value(arm, HardwareArmCheck{});
      const bool arm_ready = check.state == "Ready";
      ready = ready && arm_ready;
      checks_.append(QVariantMap{{"name", arm + " dashboard safety"},
        {"state", arm_ready ? "Ready" : check.state}, {"detail", check.detail}});
    }
    preflight_ready_ = ready;
    checked_context_ = ready ? expected_context : QString();
    state_ = ready ? "Ready" : "Blocked";
    message_ = ready ? "All required preflight checks are confirmed. Hardware can be started." :
      "Hardware launch is blocked: every required host and selected-arm check must be confirmed Ready.";
    emit changed();
  });
}

void HardwareStartController::startHardware()
{
  if (shutdown_interlock_ && shutdown_interlock_()) { message_ = "Hardware start blocked by UR shutdown interlock"; emit changed(); return; }
  if (busy_ || !adapter_) return;
  if (mode_ != "Hardware" || connection_state_ != "Connected" || !preflight_ready_ ||
    checked_context_ != contextFingerprint()) {
    state_ = "Blocked";
    message_ = "Run a successful preflight for the current connected Hardware target before starting.";
    emit changed();
    return;
  }
  if (selected_arms(options_).isEmpty() && !options_.value("launch_mir").toBool()) {
    state_ = "Blocked";
    message_ = "Select at least one UR arm or the MiR base before starting hardware.";
    emit changed();
    return;
  }
  if (options_.value("launch_mir").toBool() &&
    options_.value("localization_source").toString() != "mir" && localizationState() != "Ready") {
    state_ = "Blocked";
    const auto source = options_.value("localization_source", "mir").toString();
    message_ = QString("Hardware start blocked: %1 localization is %2. %3")
      .arg(source, localizationState().toLower(), localizationDetail());
    emit changed();
    return;
  }
  busy_ = true;
  state_ = "Starting";
  message_ = QString("Starting selected hardware components on %1…").arg(host_);
  emit changed();
  adapter_->startHardware(config(), [this](const HardwareActionResult & result) {
    finishAction(result, "Hardware start");
  });
}

void HardwareStartController::syncSource()
{
  if (busy_ || !adapter_ || mode_ != "Hardware" || connection_state_ != "Connected") {
    message_ = "Source sync requires a connected Hardware session.";
    state_ = "Blocked";
    emit changed();
    return;
  }
  busy_ = true;
  state_ = "Syncing";
  message_ = QString("Synchronizing the configured source to %1…").arg(host_);
  emit changed();
  adapter_->syncSource(config(), [this](const HardwareActionResult & result) {
    finishAction(result, "Source sync");
  });
}

void HardwareStartController::buildWorkspace()
{
  if (busy_ || !adapter_ || mode_ != "Hardware" || connection_state_ != "Connected") {
    message_ = "Workspace build requires a connected Hardware session.";
    state_ = "Blocked";
    emit changed();
    return;
  }
  busy_ = true;
  state_ = "Building";
  message_ = QString("Building the configured workspace on %1…").arg(host_);
  emit changed();
  adapter_->buildWorkspace(config(), [this](const HardwareActionResult & result) {
    finishAction(result, "Workspace build");
  });
}

void HardwareStartController::finishAction(const HardwareActionResult & result, const QString & verb)
{
  busy_ = false;
  state_ = result.accepted ? (verb == "Hardware start" ? "Starting" : "Complete") : "Failed";
  message_ = result.detail.isEmpty() ?
    (result.accepted ? verb + " accepted" : verb + " failed without confirmation") : result.detail;
  emit changed();
}

void HardwareStartController::saveOptions()
{
  if (preset_.isEmpty()) return;
  settings_->beginGroup(QString("hardwareLaunch/%1").arg(preset_));
  for (auto it = options_.cbegin(); it != options_.cend(); ++it) settings_->setValue(it.key(), it.value());
  settings_->endGroup();
  settings_->sync();
}
