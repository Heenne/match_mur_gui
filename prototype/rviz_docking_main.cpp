#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <utility>

#include <QApplication>
#include <QCursor>
#include <QDockWidget>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QScrollArea>
#include <QLabel>
#include <QMainWindow>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QShortcut>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QTemporaryDir>
#include <QStringList>
#include <QHostInfo>
#include <QHostAddress>
#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTimer>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QEventLoop>
#include <QScopeGuard>
#include <QScreen>
#include <cstdlib>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"
#include "rviz_common/ros_integration/ros_node_abstraction.hpp"
#include "rviz_common/config.hpp"
#include "rviz_common/visualization_frame.hpp"
#include "rviz_common/yaml_config_reader.hpp"
#include "ros_session_monitor.hpp"
#include "simulation_base_jog.hpp"
#include "mir_hardware_control.hpp"
#include "process_manager.hpp"
#include "log_store.hpp"
#include "log_panel.hpp"
#include "goals_poses.hpp"
#include "mocap_controller.hpp"
#include "ur_arm_jog.hpp"
#include "ur_freedrive.hpp"
#include "ur_motion_actions.hpp"
#include "ur_shutdown.hpp"
#include "hardware_status.hpp"
#include "lift_controller.hpp"
#include "hardware_start.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float32.hpp"
#include "sensor_msgs/msg/battery_state.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

namespace
{
bool valid_remote_host(const QString & host)
{
  if (host == "::1") return true;
  static const QRegularExpression host_pattern(
    "^[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?$");
  return host == "localhost" || host_pattern.match(host).hasMatch();
}

QString configure_ros_environment(const QString & host, int domain,
  const QList<QHostAddress> & addresses, const QString & profile_directory)
{
  if (domain < 0 || domain > 232) throw std::invalid_argument("ROS domain must be between 0 and 232");
  if (!valid_remote_host(host)) throw std::invalid_argument("Target must be a host name only");
  const QString owned_profile = qEnvironmentVariable("MUR_GUI_DDS_PROFILE_OWNED_PATH");
  if (!owned_profile.isEmpty()) {
    if (qEnvironmentVariable("FASTDDS_DEFAULT_PROFILES_FILE") == owned_profile) {
      qunsetenv("FASTDDS_DEFAULT_PROFILES_FILE");
    }
    if (qEnvironmentVariable("FASTRTPS_DEFAULT_PROFILES_FILE") == owned_profile) {
      qunsetenv("FASTRTPS_DEFAULT_PROFILES_FILE");
    }
    qunsetenv("MUR_GUI_DDS_PROFILE_OWNED_PATH");
  }
  qputenv("ROS_DOMAIN_ID", QByteArray::number(domain));
  const bool local_only = qEnvironmentVariable("ROS_LOCALHOST_ONLY") == "1" ||
    qEnvironmentVariable("ROS_AUTOMATIC_DISCOVERY_RANGE").toUpper() == "LOCALHOST" ||
    qEnvironmentVariable("ROS_AUTOMATIC_DISCOVERY_RANGE").toUpper() == "OFF";
  if (local_only) return QString("Target %1 · domain %2 · local-only DDS settings prevent remote discovery")
      .arg(host).arg(domain);

  qputenv("ROS_AUTOMATIC_DISCOVERY_RANGE", "SUBNET");
  const QString discovery_server = qEnvironmentVariable("ROS_DISCOVERY_SERVER");
  if (!discovery_server.isEmpty()) {
    return QString("Target %1 · domain %2 · ROS discovery server route %3")
      .arg(host).arg(domain).arg(discovery_server);
  }
  const QString rmw = qEnvironmentVariable("RMW_IMPLEMENTATION");
  if (!rmw.isEmpty() && rmw != "rmw_fastrtps_cpp" && rmw != "rmw_fastrtps_dynamic_cpp") {
    return QString("Target %1 · domain %2 · middleware %3 configured; Fast DDS direct peers were not added")
      .arg(host).arg(domain).arg(rmw);
  }
  if (host == "localhost" || host == "127.0.0.1" || host == "::1") {
    return QString("Target %1 · domain %2 · subnet DDS discovery").arg(host).arg(domain);
  }
  if (addresses.isEmpty()) throw std::invalid_argument("Remote host resolved to no IPv4 addresses");

  const QString fastdds = qEnvironmentVariable("FASTDDS_DEFAULT_PROFILES_FILE");
  const QString fasrtps = qEnvironmentVariable("FASTRTPS_DEFAULT_PROFILES_FILE");
  for (const auto & profile : {fastdds, fasrtps}) {
    if (!profile.isEmpty() && QFileInfo::exists(profile)) {
      return QString("Target %1 · domain %2 · operator DDS profile %3; remote route not added")
        .arg(host).arg(domain).arg(profile);
    }
  }

  QDir().mkpath(profile_directory);
  const QString path = QDir(profile_directory).filePath(QString("remote_domain_%1.xml").arg(domain));
  QString xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<profiles xmlns=\"http://www.eprosima.com/XMLSchemas/fastRTPS_Profiles\">"
    "<participant profile_name=\"mur_gui_remote_peers\" is_default_profile=\"true\">"
    "<rtps><builtin><initialPeersList>";
  for (const auto & address : addresses) {
    for (int index = 0; index < 64; ++index) {
      const int port = 7400 + 250 * domain + 10 + 2 * index;
      if (port > 65535) break;
      xml += QString("<locator><udpv4><address>%1</address><port>%2</port></udpv4></locator>")
        .arg(address.toString()).arg(port);
    }
  }
  xml += "</initialPeersList></builtin></rtps></participant></profiles>";
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(xml.toUtf8()) != xml.toUtf8().size() || !file.commit()) {
    throw std::runtime_error("Could not write direct DDS peer profile");
  }
  qputenv("FASTDDS_DEFAULT_PROFILES_FILE", path.toLocal8Bit());
  qputenv("FASTRTPS_DEFAULT_PROFILES_FILE", path.toLocal8Bit());
  qputenv("MUR_GUI_DDS_PROFILE_OWNED_PATH", path.toLocal8Bit());
  QStringList rendered;
  for (const auto & address : addresses) rendered << address.toString();
  rendered.removeDuplicates();
  return QString("Target %1 resolved to %2 · domain %3 · direct DDS peer route; UDP reachability is not yet confirmed")
    .arg(host, rendered.join(", ")).arg(domain);
}

class SessionController : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString preset READ preset NOTIFY changed)
  Q_PROPERTY(QString target READ target NOTIFY changed)
  Q_PROPERTY(int rosDomain READ rosDomain NOTIFY changed)
  Q_PROPERTY(QString robot READ robot NOTIFY changed)
  Q_PROPERTY(QString mode READ mode NOTIFY changed)
  Q_PROPERTY(QString visualization READ visualization NOTIFY changed)
  Q_PROPERTY(QString workspacePath READ workspacePath NOTIFY changed)
  Q_PROPERTY(QString configPath READ configPath NOTIFY changed)
  Q_PROPERTY(QString navigationGoal READ navigationGoal NOTIFY changed)
  Q_PROPERTY(QString navigationCancel READ navigationCancel NOTIFY changed)
  Q_PROPERTY(QString navigationPose READ navigationPose NOTIFY changed)
  Q_PROPERTY(QString simCommandTopic READ simCommandTopic NOTIFY changed)
  Q_PROPERTY(QString mirCommandTopic READ mirCommandTopic NOTIFY changed)
  Q_PROPERTY(QString mirCommandType READ mirCommandType NOTIFY changed)
  Q_PROPERTY(bool useSimTime READ useSimTime NOTIFY changed)
  Q_PROPERTY(bool gazeboViewOnSimulationStart READ gazeboViewOnSimulationStart NOTIFY changed)
  Q_PROPERTY(bool simulationServerAvailable READ simulationServerAvailable NOTIFY changed)
  Q_PROPERTY(QString lastError READ lastError NOTIFY changed)
  Q_PROPERTY(QString view READ view NOTIFY changed)
  Q_PROPERTY(QString connectionState READ connectionState NOTIFY changed)
  Q_PROPERTY(QStringList diagnostics READ diagnostics NOTIFY changed)
  Q_PROPERTY(QStringList presets READ presets NOTIFY changed)

public:
  SessionController()
  {
    const auto test_path = qEnvironmentVariable("MUR_COMMAND_CENTER_SETTINGS");
    if (test_path.isEmpty()) {
      settings_ = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
        "MATCH", "MuRCommandCenter");
    } else {
      settings_ = std::make_unique<QSettings>(test_path, QSettings::IniFormat);
    }
    const auto active = settings_->value("activePreset", "Local simulation").toString();
    selectPreset(active);
  }

  QString preset() const { return settings_->value("activePreset").toString(); }
  QString target() const { return value("target", "localhost"); }
  int rosDomain() const
  {
    bool valid = false;
    const int configured = value("rosDomain", qEnvironmentVariable("ROS_DOMAIN_ID", "62")).toInt(&valid);
    return valid && configured >= 0 && configured <= 232 ? configured : 62;
  }
  QString robot() const { return value("robot", "mur620d"); }
  QString mode() const { return value("mode", preset() == "Local simulation" ? "Simulation" : "Hardware"); }
  QString visualization() const { return value("visualization", "RViz"); }
  QString workspacePath() const { return value("workspacePath", ""); }
  QString configPath() const { return value("configPath", ""); }
  QString navigationGoal() const { return value("navigationMode", "") == mode() ? value("navigationGoal", "") : QString(); }
  QString navigationCancel() const { return value("navigationMode", "") == mode() ? value("navigationCancel", "") : QString(); }
  QString navigationPose() const { return value("navigationMode", "") == mode() ? value("navigationPose", "") : QString(); }
  QString simCommandTopic() const { return value("simCommandTopic", ""); }
  QString mirCommandTopic() const { return value("mirCommandTopic", ""); }
  QString mirCommandType() const { return value("mirCommandType", ""); }
  static bool validMirEndpoint(const QString & topic, const QString & type, const QString & robot)
  {
    if (topic.isEmpty() && type.isEmpty()) return true;
    static const QRegularExpression valid("^/[A-Za-z_][A-Za-z0-9_]*(/[A-Za-z_][A-Za-z0-9_]*)+$");
    return valid.match(topic).hasMatch() && topic.startsWith("/" + robot + "/") &&
      (type == "geometry_msgs/msg/Twist" || type == "geometry_msgs/msg/TwistStamped");
  }
  QString rvizConfigPath(const QString & command_line_override) const
  {
    return command_line_override.trimmed().isEmpty() ? configPath().trimmed() : command_line_override.trimmed();
  }
  bool useSimTime() const { return mode() == "Simulation"; }
  bool gazeboViewOnSimulationStart() const
  {
    return settings_->value(key("gazeboViewOnSimulationStart"), false).toBool();
  }
  bool simulationServerAvailable() const { return simulation_server_available_; }
  QString contextKey(const QString & command_line_override) const
  {
    return QString("%1|%2|%3|%4|%5|%6|%7")
      .arg(preset(), mode(), target()).arg(rosDomain())
      .arg(robot(), visualization(), rvizConfigPath(command_line_override));
  }
  bool validateRvizProfile(const QString & command_line_override, QString * error) const
  {
    if (mode() != "Simulation" && mode() != "Hardware") {
      if (error) *error = QString("Selected profile '%1' has unavailable mode '%2'.")
        .arg(preset(), mode());
      return false;
    }
    if (visualization() != "RViz") {
      if (error) *error = QString("Selected profile '%1' uses '%2'; RViz is unavailable for this visualization.")
        .arg(preset(), visualization());
      return false;
    }
    const auto path = rvizConfigPath(command_line_override);
    if (path.isEmpty()) {
      if (error) *error = QString("Selected profile '%1' has no RViz configuration. Set its RViz config path in Settings.")
        .arg(preset());
      return false;
    }
    const QFileInfo info(path);
    QFile file(path);
    if (!info.exists() || !info.isFile() || !info.isReadable() || !file.open(QIODevice::ReadOnly)) {
      if (error) *error = QString("RViz configuration for profile '%1' is unavailable or unreadable: %2")
        .arg(preset(), path);
      return false;
    }
    const auto contents = file.readAll().trimmed();
    if (contents.isEmpty()) {
      if (error) *error = QString("RViz configuration for profile '%1' is empty: %2")
        .arg(preset(), path);
      return false;
    }
    rviz_common::Config parsed_config;
    rviz_common::YamlConfigReader config_reader;
    config_reader.readFile(parsed_config, path);
    if (config_reader.error() || !parsed_config.mapGetChild("Visualization Manager").isValid()) {
      if (error) *error = QString("RViz configuration for profile '%1' is invalid: missing Visualization Manager section (%2)")
        .arg(preset(), config_reader.error() ? config_reader.errorMessage() : path);
      return false;
    }
    return true;
  }
  QString lastError() const { return last_error_; }
  QString view() const { return value("view", "Overview"); }
  QString connectionState() const { return connection_state_; }
  QStringList diagnostics() const { return diagnostics_; }
  QStringList presets() const
  {
    settings_->beginGroup("presets");
    auto names = settings_->childGroups();
    settings_->endGroup();
    if (!names.contains("Local simulation")) names.append("Local simulation");
    names.removeDuplicates();
    names.sort();
    return names;
  }

public slots:
  void selectPreset(const QString &name)
  {
    const auto clean = name.trimmed();
    if (clean.isEmpty()) return;
    const auto group = QString("presets/%1/").arg(clean);
    if (!settings_->contains(group + "view")) settings_->setValue(group + "view", "Overview");
    if (!settings_->contains(group + "target")) settings_->setValue(group + "target", "localhost");
    if (!settings_->contains(group + "rosDomain")) settings_->setValue(group + "rosDomain", 62);
    if (!settings_->contains(group + "robot")) settings_->setValue(group + "robot", "mur620d");
    if (!settings_->contains(group + "visualization")) settings_->setValue(group + "visualization", "RViz");
    if (!settings_->contains(group + "workspacePath")) settings_->setValue(group + "workspacePath", "");
    if (!settings_->contains(group + "configPath")) settings_->setValue(group + "configPath", "");
    if (!settings_->contains(group + "gazeboViewOnSimulationStart")) settings_->setValue(group + "gazeboViewOnSimulationStart", false);
    if (!settings_->contains(group + "mode")) {
      settings_->setValue(group + "mode", clean == "Local simulation" ? "Simulation" : "Hardware");
    }
    settings_->setValue("activePreset", clean);
    settings_->sync();
    emit changed();
  }
  void selectTarget(const QString &target) { setValue("target", target); }
  void selectRosDomain(int domain)
  {
    if (domain >= 0 && domain <= 232) settings_->setValue(key("rosDomain"), domain);
    emit changed();
  }
  void selectRobot(const QString &robot) { setValue("robot", robot); }
  Q_INVOKABLE void setGazeboViewOnSimulationStart(bool enabled)
  {
    settings_->setValue(key("gazeboViewOnSimulationStart"), enabled);
    settings_->sync();
    emit changed();
  }
  void setSimulationServerAvailable(bool available)
  {
    if (simulation_server_available_ == available) return;
    simulation_server_available_ = available;
    emit changed();
  }
  void selectMode(const QString &mode)
  {
    if (mode == "Simulation" || mode == "Hardware") setValue("mode", mode);
  }
  Q_INVOKABLE bool saveProfile(const QString &name, const QString &mode, const QString &target,
    const QString &robot, const QString &visualization, const QString &workspace_path,
    const QString &config_path, int ros_domain, bool gazebo_view_on_simulation_start = false)
  {
    const auto clean_name = name.trimmed();
    if (clean_name.isEmpty() || clean_name.contains('/') || clean_name.contains('\\') ||
      (mode != "Simulation" && mode != "Hardware") || target.trimmed().isEmpty() ||
      robot.trimmed().isEmpty() || !valid_remote_host(target.trimmed()) ||
      ros_domain < 0 || ros_domain > 232 ||
      (visualization != "RViz" && visualization != "MuR data view")) {
      setError("Profile needs a name, Simulation or Hardware mode, host name, domain 0–232, robot, and supported visualization.");
      return false;
    }
    const auto group = QString("presets/%1/").arg(clean_name);
    settings_->setValue(group + "mode", mode);
    settings_->setValue(group + "target", target.trimmed());
    settings_->setValue(group + "rosDomain", ros_domain);
    settings_->setValue(group + "robot", robot.trimmed());
    settings_->setValue(group + "visualization", visualization);
    settings_->setValue(group + "workspacePath", workspace_path.trimmed());
    settings_->setValue(group + "configPath", config_path.trimmed());
    settings_->setValue(group + "gazeboViewOnSimulationStart", gazebo_view_on_simulation_start);
    settings_->setValue("activePreset", clean_name);
    settings_->sync();
    clearError();
    emit changed();
    return true;
  }
  Q_INVOKABLE bool validateEndpoints(const QString &robot, const QString &mir_topic, const QString &mir_type,
    const QString &sim_topic, const QString &goal, const QString &cancel, const QString &pose)
  {
    const auto valid_topic = [&](const QString &topic) {
      const auto clean = topic.trimmed();
      return validMirEndpoint(clean, clean.isEmpty() ? "" : "geometry_msgs/msg/Twist", robot.trimmed());
    };
    if (!validMirEndpoint(mir_topic.trimmed(), mir_type, robot.trimmed()) ||
      !valid_topic(sim_topic) || !valid_topic(goal) || !valid_topic(cancel) || !valid_topic(pose)) {
      setError("Endpoints must be valid absolute topics under the selected robot; MiR also needs a supported Twist type.");
      return false;
    }
    return true;
  }
  Q_INVOKABLE bool setNavigationEndpoints(const QString &goal, const QString &cancel, const QString &pose)
  {
    for (const auto &topic : {goal.trimmed(), cancel.trimmed(), pose.trimmed()}) {
      if (!validMirEndpoint(topic, topic.isEmpty() ? "" : "geometry_msgs/msg/Twist", robot())) {
        setError("Navigation endpoints must be empty or valid topics under the selected robot.");
        return false;
      }
    }
    settings_->setValue(key("navigationMode"), mode());
    settings_->setValue(key("navigationGoal"), goal.trimmed());
    settings_->setValue(key("navigationCancel"), cancel.trimmed());
    settings_->setValue(key("navigationPose"), pose.trimmed());
    settings_->sync();
    clearError();
    emit changed();
    return true;
  }
  Q_INVOKABLE bool setSimEndpoint(const QString &topic)
  {
    if (!validMirEndpoint(topic.trimmed(), topic.trimmed().isEmpty() ? "" : "geometry_msgs/msg/TwistStamped", robot())) {
      setError("Simulation command topic must be empty or a valid topic under the selected robot.");
      return false;
    }
    settings_->setValue(key("simCommandTopic"), topic.trimmed());
    settings_->sync();
    clearError();
    emit changed();
    return true;
  }
  Q_INVOKABLE bool setMirEndpoint(const QString &topic, const QString &type)
  {
    if (!validMirEndpoint(topic.trimmed(), type, robot())) {
      setError("MiR command endpoint must be empty or a valid topic under the selected robot with a supported Twist type.");
      return false;
    }
    settings_->setValue(key("mirCommandTopic"), topic.trimmed());
    settings_->setValue(key("mirCommandType"), type);
    settings_->sync();
    clearError();
    emit changed();
    return true;
  }
  Q_INVOKABLE bool exportPreset(const QString &path)
  {
    QJsonObject profile;
    profile.insert("name", preset());
    profile.insert("mode", mode());
    profile.insert("target", target());
    profile.insert("rosDomain", rosDomain());
    profile.insert("robot", robot());
    profile.insert("visualization", visualization());
    profile.insert("gazeboViewOnSimulationStart", gazeboViewOnSimulationStart());
    profile.insert("navigationGoal", navigationGoal());
    profile.insert("navigationCancel", navigationCancel());
    profile.insert("navigationPose", navigationPose());
    profile.insert("simCommandTopic", simCommandTopic());
    profile.insert("mirCommandTopic", mirCommandTopic());
    profile.insert("mirCommandType", mirCommandType());
    profile.insert("requiresWorkspaceMapping", !workspacePath().trimmed().isEmpty());
    profile.insert("requiresConfigMapping", !configPath().trimmed().isEmpty());
    QJsonObject bundle;
    bundle.insert("format", "mur-command-center-preset");
    bundle.insert("version", 1);
    bundle.insert("profile", profile);
    QSaveFile file(QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      setError(QString("Could not write preset: %1").arg(file.errorString()));
      return false;
    }
    const auto data = QJsonDocument(bundle).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
      setError(QString("Could not write preset: %1").arg(file.errorString()));
      return false;
    }
    clearError();
    return true;
  }
  Q_INVOKABLE bool importPreset(const QString &path, const QString &workspace_mapping,
    const QString &config_mapping)
  {
    QFile file(QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path);
    if (!file.open(QIODevice::ReadOnly)) {
      setError(QString("Could not read preset: %1").arg(file.errorString()));
      return false;
    }
    QJsonParseError parse_error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
      setError(QString("Invalid preset JSON: %1").arg(parse_error.errorString()));
      return false;
    }
    const auto bundle = document.object();
    const auto profile = bundle.value("profile").toObject();
    const auto name = profile.value("name").toString().trimmed();
    const auto mode = profile.value("mode").toString();
    const auto target = profile.value("target").toString().trimmed();
    const auto domain = profile.contains("rosDomain") ? profile.value("rosDomain").toInt(-1) : 62;
    const auto robot = profile.value("robot").toString().trimmed();
    const auto visualization = profile.value("visualization").toString();
    const auto nav_goal = profile.value("navigationGoal").toString();
    const auto nav_cancel = profile.value("navigationCancel").toString();
    const auto nav_pose = profile.value("navigationPose").toString();
    const auto sim_topic = profile.value("simCommandTopic").toString();
    const auto mir_topic = profile.value("mirCommandTopic").toString();
    const auto mir_type = profile.value("mirCommandType").toString();
    const auto gazebo_view_on_simulation_start = profile.value("gazeboViewOnSimulationStart").toBool(false);
    if (bundle.value("format").toString() != "mur-command-center-preset" ||
      bundle.value("version").toInt(-1) != 1 || name.isEmpty() ||
      (mode != "Simulation" && mode != "Hardware") || target.isEmpty() || robot.isEmpty() ||
      !valid_remote_host(target) || domain < 0 || domain > 232 ||
      (profile.contains("navigationGoal") && !profile.value("navigationGoal").isString()) ||
      (profile.contains("navigationCancel") && !profile.value("navigationCancel").isString()) ||
      (profile.contains("navigationPose") && !profile.value("navigationPose").isString()) ||
      (profile.contains("simCommandTopic") && !profile.value("simCommandTopic").isString()) ||
      (profile.contains("mirCommandTopic") && !profile.value("mirCommandTopic").isString()) ||
      (profile.contains("mirCommandType") && !profile.value("mirCommandType").isString()) ||
      !validMirEndpoint(nav_goal, nav_goal.isEmpty() ? "" : "geometry_msgs/msg/Twist", robot) ||
      !validMirEndpoint(nav_cancel, nav_cancel.isEmpty() ? "" : "geometry_msgs/msg/Twist", robot) ||
      !validMirEndpoint(nav_pose, nav_pose.isEmpty() ? "" : "geometry_msgs/msg/Twist", robot) ||
      !validMirEndpoint(mir_topic, mir_type, robot) ||
      !validMirEndpoint(sim_topic, sim_topic.isEmpty() ? "" : "geometry_msgs/msg/TwistStamped", robot) ||
      (visualization != "RViz" && visualization != "MuR data view") ||
      !profile.value("requiresWorkspaceMapping").isBool() ||
      !profile.value("requiresConfigMapping").isBool()) {
      setError("Unsupported or invalid preset bundle. Expected version 1 with a complete profile.");
      return false;
    }
    // Bundles contain only mapping-required flags; machine-local absolute paths never leave this host.
    const auto workspace = profile.value("requiresWorkspaceMapping").toBool() ? workspace_mapping.trimmed() : QString();
    const auto config = profile.value("requiresConfigMapping").toBool() ? config_mapping.trimmed() : QString();
    if (!saveProfile(name, mode, target, robot, visualization, workspace, config, domain,
      gazebo_view_on_simulation_start)) return false;
    return setMirEndpoint(mir_topic, mir_type) && setSimEndpoint(sim_topic) &&
      setNavigationEndpoints(nav_goal, nav_cancel, nav_pose);
  }
  void selectView(const QString &view)
  {
    static const QStringList valid = {"Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings", "Mocap"};
    if (valid.contains(view)) setValue("view", view);
  }
  void connectToTarget() { if (connection_state_ == "Disconnected" || connection_state_.startsWith("Failed")) emit connectRequested(); }
  void disconnectFromTarget() { emit disconnectRequested(); }
  void setDiagnostics(const QStringList &diagnostics)
  {
    diagnostics_ = diagnostics;
    if (!discovery_route_.isEmpty()) diagnostics_.prepend("DDS route: " + discovery_route_);
    emit changed();
  }
  void setDiscoveryRoute(const QString &route) { discovery_route_ = route; }
  void setConnectionState(const QString &state) { connection_state_ = state; emit changed(); }

signals:
  void changed();
  void connectRequested();
  void disconnectRequested();

private:
  void setError(const QString &error) { last_error_ = error; emit changed(); }
  void clearError() { last_error_.clear(); }
  QString key(const QString &field) const { return QString("presets/%1/%2").arg(preset(), field); }
  QString value(const QString &field, const QString &fallback) const
  {
    return settings_->value(key(field), fallback).toString();
  }
  void setValue(const QString &field, const QString &value)
  {
    settings_->setValue(key(field), value);
    settings_->sync();
    emit changed();
  }
  std::unique_ptr<QSettings> settings_;
  QString connection_state_{"Disconnected"};
  QString last_error_;
  QString discovery_route_;
  QStringList diagnostics_{"Not connected · choose Connect to inspect the existing ROS graph"};
  bool simulation_server_available_{false};
};

bool run_session_contract_test()
{
  {
    SessionController session;
    if (session.preset() != "Local simulation" || session.mode() != "Simulation" ||
      session.view() != "Overview" || session.target() != "localhost" ||
      session.robot() != "mur620d" || session.connectionState() != "Disconnected") return false;
    int connect_requests = 0;
    QObject::connect(&session, &SessionController::connectRequested, [&connect_requests]() {
      ++connect_requests;
    });
    session.connectToTarget();
    if (connect_requests != 1) return false;
    const QStringList expected_diagnosis = {"ROS graph: no remote nodes discovered in this domain",
      "Clock: missing (/clock has no publisher)"};
    session.setDiagnostics(expected_diagnosis);
    if (session.diagnostics() != expected_diagnosis) return false;
    session.selectView("Visualization");
    session.selectMode("Hardware");
    session.selectTarget("sim-host");
    session.selectRobot("mur620b");
    session.selectPreset("Field test");
    if (session.view() != "Overview" || session.mode() != "Hardware") return false;
    session.selectView("Processes");
    session.selectTarget("lab-host");
    session.selectRobot("mur620c");
  }
  {
    SessionController restored;
    if (restored.preset() != "Field test" || restored.mode() != "Hardware" ||
      restored.view() != "Processes" || restored.target() != "lab-host" ||
      restored.robot() != "mur620c" || restored.connectionState() != "Disconnected" ||
      restored.diagnostics().isEmpty() || rclcpp::ok()) return false;
    restored.selectPreset("Local simulation");
    if (restored.view() != "Visualization" || restored.mode() != "Hardware" ||
      restored.target() != "sim-host" || restored.robot() != "mur620b") return false;
  }
  return !rclcpp::ok();
}

bool run_ros_session_monitor_contract_test()
{
  rclcpp::init(0, nullptr);
  auto observer = std::make_shared<rclcpp::Node>("session_monitor_contract_observer");
  auto fixture = std::make_shared<rclcpp::Node>("session_monitor_contract_fixture");
  RosSessionMonitor monitor(observer);
  if (monitor.simulationServerAvailable()) return false;
  auto clock_publisher = fixture->create_publisher<rosgraph_msgs::msg::Clock>("/clock", 10);
  auto wrong_type = fixture->create_publisher<std_msgs::msg::String>(
    "/mur620d/mobile_base_controller/cmd_vel", 10);
  auto mur_soc = fixture->create_publisher<std_msgs::msg::Float32>(
    "/mur620d/bms_status/SOC", 10);
  auto mir_battery = fixture->create_publisher<sensor_msgs::msg::BatteryState>(
    "/mur620d/battery_state", 10);
  auto joints = fixture->create_publisher<sensor_msgs::msg::JointState>(
    "/mur620d/joint_states", 10);
  using ListControllers = controller_manager_msgs::srv::ListControllers;
  auto manager = fixture->create_service<ListControllers>(
    "/mur620d/controller_manager/list_controllers",
    [](const std::shared_ptr<ListControllers::Request>,
      std::shared_ptr<ListControllers::Response> response) {
      controller_manager_msgs::msg::ControllerState state;
      state.name = "drive_controller";
      state.state = "active";
      response->controller.push_back(state);
    });
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(observer);
  executor.add_node(fixture);

  bool passed = false;
  for (int attempt = 0; attempt < 100; ++attempt) {
    rosgraph_msgs::msg::Clock clock_message;
    clock_message.clock.sec = attempt;
    clock_publisher->publish(clock_message);
    std_msgs::msg::Float32 soc_message;
    soc_message.data = 81.2F;
    mur_soc->publish(soc_message);
    sensor_msgs::msg::BatteryState battery_message;
    battery_message.percentage = 0.65F;
    battery_message.voltage = 25.4F;
    mir_battery->publish(battery_message);
    sensor_msgs::msg::JointState joint_message;
    joint_message.name = {"left_lift_joint", "right_lift_joint"};
    joint_message.position = {0.42, 0.51};
    joints->publish(joint_message);
    executor.spin_some();
    const auto hardware = monitor.diagnose("localhost", "mur620d", "Hardware", 0);
    if (hardware.join("\n").contains("drive_controller: active")) {
      const auto simulation = monitor.diagnose("localhost", "mur620d", "Simulation", 0).join("\n");
      passed = hardware.join("\n").contains("remote node(s) discovered") &&
        hardware.join("\n").contains("wrong type on /mur620d/mobile_base_controller/cmd_vel") &&
        hardware.join("\n").contains("MuR battery: 81.2%") &&
        hardware.join("\n").contains("MiR battery: 65.0%") &&
        hardware.join("\n").contains("left_lift_joint: 0.420 m") &&
        hardware.join("\n").contains("Clock: not required in Hardware mode") &&
        simulation.contains("Clock: advancing") && monitor.simulationServerAvailable();
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  executor.remove_node(observer);
  executor.remove_node(fixture);
  rclcpp::shutdown();
  return passed;
}

bool run_mocap_controller_contract_test()
{
  QTemporaryDir settings_directory;
  if (!settings_directory.isValid()) return false;
  const auto previous_settings_path = qgetenv("MUR_COMMAND_CENTER_SETTINGS");
  qputenv("MUR_COMMAND_CENTER_SETTINGS",
    QDir(settings_directory.path()).filePath("mocap-settings.ini").toLocal8Bit());
  rclcpp::init(0, nullptr);
  auto observer = std::make_shared<rclcpp::Node>("mocap_contract_observer");
  auto fixture = std::make_shared<rclcpp::Node>("mocap_contract_fixture");
  MocapController controller;
  ProcessManager processes;
  controller.setProcessManager(&processes);
  controller.setNode(observer);
  controller.setRobot("mur620d");

  auto pose_publisher = fixture->create_publisher<geometry_msgs::msg::PoseStamped>(
    "/qualisys/mur620d/pose", 10);
  auto map_pose_publisher = fixture->create_publisher<geometry_msgs::msg::PoseStamped>(
    "/qualisys_map/mur620d/pose", 10);
  auto tf_publisher = fixture->create_publisher<tf2_msgs::msg::TFMessage>(
    "/tf_static", rclcpp::QoS(1).transient_local());
  auto frozen_publisher = fixture->create_publisher<std_msgs::msg::Bool>(
    "/qualisys/mur620d/localization_frozen", rclcpp::QoS(1).transient_local());
  int map_requests = 0;
  int freeze_requests = 0;
  auto map_service = fixture->create_service<std_srvs::srv::SetBool>(
    "/qualisys/enable_map_output", [&map_requests](
      const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
      std::shared_ptr<std_srvs::srv::SetBool::Response> response) {
      ++map_requests;
      response->success = true;
      response->message = request->data ? "enabled" : "disabled";
    });
  auto freeze_service = fixture->create_service<std_srvs::srv::SetBool>(
    "/qualisys/mur620d/freeze_localization", [&freeze_requests](
      const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
      std::shared_ptr<std_srvs::srv::SetBool::Response> response) {
      ++freeze_requests;
      response->success = true;
      response->message = request->data ? "frozen" : "resumed";
    });
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(observer);
  executor.add_node(fixture);
  bool passed = false;
  for (int attempt = 0; attempt < 100; ++attempt) {
    geometry_msgs::msg::PoseStamped pose;
    pose.header.frame_id = "mocap";
    pose.pose.position.x = 1.25;
    pose.pose.position.y = -0.5;
    pose.pose.orientation.w = 1.0;
    pose_publisher->publish(pose);
    auto map_pose = pose;
    map_pose.header.frame_id = "map";
    map_pose_publisher->publish(map_pose);
    tf2_msgs::msg::TFMessage tf;
    geometry_msgs::msg::TransformStamped map_to_mocap;
    map_to_mocap.header.frame_id = "map";
    map_to_mocap.child_frame_id = "mocap";
    tf.transforms.push_back(map_to_mocap);
    tf_publisher->publish(tf);
    std_msgs::msg::Bool frozen;
    frozen.data = false;
    frozen_publisher->publish(frozen);
    executor.spin_some();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    if (controller.poseStatus().startsWith("Receiving") &&
      controller.frequencyHz() > 0.0 && controller.poseFrame() == "mocap" &&
      controller.pose().contains("x 1.250 m") && controller.mapStatus().contains("TF active")) {
      controller.setMapOutput(true);
      controller.setFrozen(true);
      for (int response_attempt = 0; response_attempt < 20; ++response_attempt) {
        executor.spin_some();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      const bool external_not_taken_over = !controller.startBridge() &&
        controller.bridgeState() == "Running · external bridge";
      controller.stopBridge();
      const bool fresh_pose_received = controller.dataAgeSeconds() >= 0.0 &&
        controller.dataAgeSeconds() < 0.5;
      const bool freeze_confirmed = controller.freezeStatus() == "Localization frozen" &&
        controller.frozen();
      bool managed_bridge_stopped = false;
      QTemporaryDir fake_tools;
      const auto fake_ros2_path = fake_tools.filePath("ros2");
      QFile fake_ros2(fake_ros2_path);
      const auto previous_path = qgetenv("PATH");
      const auto fake_script = QByteArray("#!/bin/sh\necho fake ros2 \"$@\"\nexec sleep 30\n");
      const bool fake_script_ready = fake_tools.isValid() && fake_ros2.open(QIODevice::WriteOnly) &&
        fake_ros2.write(fake_script) == fake_script.size() && fake_ros2.setPermissions(
          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
          QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
      fake_ros2.close();
      if (fake_script_ready) {
        qputenv("PATH", (fake_tools.path() + ":" + QString::fromLocal8Bit(previous_path)).toLocal8Bit());
        controller.setRobot("mur620b");
        controller.setBridgeSettings("qualisys-host", "qtm-host", 22333);
        const bool own_bridge_started = controller.startBridge();
        QElapsedTimer bridge_timer;
        bridge_timer.start();
        while (own_bridge_started && bridge_timer.elapsed() < 3000 &&
          !processes.log().contains("fake ros2 run match_mocap_ros2 qualisys_ssh_bridge")) {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        const bool own_command_configured = processes.log().contains(
          "ssh_host:=qualisys-host") && processes.log().contains("qtm_host:=qtm-host") &&
          processes.log().contains("qtm_port:=22333");
        controller.stopBridge();
        bridge_timer.restart();
        while (bridge_timer.elapsed() < 5000) {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
          bool still_running = false;
          for (const auto & row : processes.processes()) {
            const auto process = row.toMap();
            if (process.value("name").toString() == "Qualisys Mocap bridge") {
              still_running = process.value("state").toString() == "Running" ||
                process.value("state").toString() == "Stopping";
            }
          }
          if (!still_running) {
            managed_bridge_stopped = true;
            break;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        qputenv("PATH", previous_path);
        bridge_timer.restart();
        while (bridge_timer.elapsed() < 500) {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        managed_bridge_stopped = managed_bridge_stopped &&
          controller.bridgeState() == "Stopped";
        passed = own_bridge_started && own_command_configured && managed_bridge_stopped;
      }
      passed = passed && fresh_pose_received &&
        map_requests == 1 && freeze_requests == 1 &&
        freeze_confirmed &&
        external_not_taken_over;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
  executor.remove_node(observer);
  executor.remove_node(fixture);
  rclcpp::shutdown();
  qputenv("MUR_COMMAND_CENTER_SETTINGS", previous_settings_path);
  return passed;
}

bool run_hardware_status_contract_test()
{
  auto now = std::chrono::steady_clock::time_point{} + std::chrono::seconds(100);
  HardwareStatus status([&now]() { return now; });
  status.expectBattery("MuR", "/mur620d/bms_status/battery_state");
  status.expectBattery("MiR", "/mur620d/battery_state");
  auto lines = status.diagnostics("mur620d").join("\n");
  if (!lines.contains("MuR battery: not received") || !lines.contains("MiR battery: not received") ||
    !lines.contains("Hardware component lift: not received")) return false;

  status.updateBattery("MuR", "/mur620d/bms_status/battery_state", 81.2, 25.4, -1.3, 42.0, 50.0);
  status.updateBattery("MiR", "/mur620d/battery_state", 65.0, 24.8, -0.8, 31.0, 36.0);
  status.updateRemainingEstimate("/mur620d/battery_time_remaining", 7200);
  status.updateLift({"left_lift_joint", "right_lift_joint"}, {0.42, 0.51});
  lines = status.diagnostics("mur620d").join("\n");
  if (!lines.contains("MuR battery: 81.2% · current") || !lines.contains("25.4 V") ||
    !lines.contains("MiR battery: 65.0% · current") || !lines.contains("24.8 V") ||
    !lines.contains("runtime 2.0 h · reported estimate") ||
    !lines.contains("left_lift_joint: 0.420 m") || !lines.contains("right_lift_joint: 0.510 m")) {
    qWarning("Current hardware status was: %s", qPrintable(lines));
    return false;
  }

  now += std::chrono::seconds(4);
  lines = status.diagnostics("mur620d").join("\n");
  const bool stale = lines.contains("MuR battery: 81.2% · stale") &&
    lines.contains("MiR battery: 65.0% · stale") &&
    lines.contains("runtime 2.0 h · stale estimate") &&
    lines.contains("Hardware component lift: stale");
  if (!stale) qWarning("Stale hardware status was: %s", qPrintable(lines));
  return stale;
}

class FakeHardwareStartAdapter final : public HardwareStartAdapter
{
public:
  HardwarePreflightResult preflight;
  int preflight_calls{0};
  int launch_calls{0};
  int prepare_calls{0};
  int sync_calls{0};
  int build_calls{0};
  HardwareStartConfig last_config;

  void runPreflight(const HardwareStartConfig & config, PreflightCallback callback) override
  {
    ++preflight_calls;
    last_config = config;
    callback(preflight);
  }
  void startHardware(const HardwareStartConfig & config, ActionCallback callback) override
  {
    ++launch_calls;
    last_config = config;
    callback({true, "Fake launch accepted"});
  }
  void prepareAmcl(const HardwareStartConfig & config, ActionCallback callback) override
  {
    ++prepare_calls; last_config = config; callback({true, "Fake AMCL preparation"});
  }
  void syncSource(const HardwareStartConfig & config, ActionCallback callback) override
  {
    ++sync_calls;
    last_config = config;
    callback({true, "Fake source sync completed"});
  }
  void buildWorkspace(const HardwareStartConfig & config, ActionCallback callback) override
  {
    ++build_calls;
    last_config = config;
    callback({true, "Fake workspace build completed"});
  }
};

bool run_hardware_start_contract_test()
{
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  const auto settings_path = directory.filePath("settings.ini");
  auto adapter = std::make_shared<FakeHardwareStartAdapter>();
  adapter->preflight.host_ready = true;
  adapter->preflight.host_detail = "All required host checks passed";
  adapter->preflight.arms.insert("UR10_l", {"Ready", "Dashboard safety confirmed"});
  adapter->preflight.arms.insert("UR10_r", {"Ready", "Dashboard safety confirmed"});
  HardwareStartController controller(adapter, nullptr, settings_path);

  controller.setContext("Field test", "Hardware", "mur620d", 62, "mur620d", "Disconnected", "", "$HOME/colcon_ws");
  controller.runPreflight();
  if (controller.preflightReady() || controller.state() != "Blocked" || adapter->preflight_calls != 0) {
    return false;
  }
  controller.setContext("Field test", "Hardware", "mur620d", 62, "mur620d", "Connected", "/tmp/source-ws", "$HOME/colcon_ws");
  controller.setOption("launch_mir", true);
  controller.setOption("launch_cameras", true);
  controller.setOption("moveit_speed_percent", 35);
  controller.setLocalizationStatus("mir", "Ready", "MiR source adapter confirmed");

  auto unsafe_adapter = std::make_shared<FakeHardwareStartAdapter>();
  unsafe_adapter->preflight.host_ready = true;
  unsafe_adapter->preflight.arms.insert("UR10_l", {"Ready", "Dashboard safety confirmed"});
  // No reply for the right arm is an unknown safety state and must block launch.
  HardwareStartController unsafe(unsafe_adapter, nullptr, settings_path);
  unsafe.setContext("Unsafe test", "Hardware", "mur620d", 62, "mur620d", "Connected", "/tmp/source-ws", "$HOME/colcon_ws");
  unsafe.runPreflight();
  unsafe.startHardware();
  if (unsafe.preflightReady() || unsafe_adapter->launch_calls != 0 || unsafe.state() != "Blocked") {
    return false;
  }

  controller.runPreflight();
  if (!controller.preflightReady() || controller.state() != "Ready" || adapter->preflight_calls != 1) {
    return false;
  }
  controller.setContext("Field test", "Hardware", "mur620d", 62, "mur620d", "Disconnected", "/tmp/source-ws", "$HOME/colcon_ws");
  controller.startHardware();
  if (controller.preflightReady() || adapter->launch_calls != 0) return false;
  controller.setContext("Field test", "Hardware", "mur620d", 62, "mur620d", "Connected", "/tmp/source-ws", "$HOME/colcon_ws");
  controller.runPreflight();
  controller.startHardware();
  if (adapter->launch_calls != 1 || adapter->last_config.arms != QStringList({"UR10_l", "UR10_r"}) ||
    !adapter->last_config.launch_mir || !adapter->last_config.launch_cameras ||
    adapter->last_config.moveit_speed_percent != 35) return false;

  controller.syncSource();
  if (adapter->sync_calls != 1 || adapter->launch_calls != 1) return false;
  controller.buildWorkspace();
  if (adapter->build_calls != 1 || adapter->launch_calls != 1) return false;

  auto restored_adapter = std::make_shared<FakeHardwareStartAdapter>();
  HardwareStartController restored(restored_adapter, nullptr, settings_path);
  restored.setContext("Field test", "Hardware", "mur620d", 62, "mur620d", "Connected", "/tmp/source-ws", "$HOME/colcon_ws");
  if (!restored.options().value("launch_mir").toBool() ||
    !restored.options().value("launch_cameras").toBool() ||
    restored.options().value("moveit_speed_percent").toInt() != 35) return false;

  const auto fake_ssh = qEnvironmentVariable("MUR_FAKE_HARDWARE_SSH");
  if (!fake_ssh.isEmpty()) {
    SshHardwareStartAdapter ssh_adapter(nullptr, nullptr, fake_ssh);
    HardwareStartConfig config;
    config.preset = "Field test"; config.mode = "Hardware"; config.host = "lab-host";
    config.robot = "mur620a"; config.remote_workspace = "$HOME/colcon_ws";
    config.arms = QStringList() << "UR10_l" << "UR10_r";
    bool completed = false;
    HardwarePreflightResult result;
    ssh_adapter.runPreflight(config, [&completed, &result](const HardwarePreflightResult & value) {
      result = value; completed = true;
    });
    QElapsedTimer wait;
    wait.start();
    while (!completed && wait.elapsed() < 3000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    if (!completed || !result.host_ready || result.arms.value("UR10_l").state != "Ready" ||
      result.arms.value("UR10_r").state != "Unknown") return false;
  }
  return true;
}

bool run_localization_graph_contract_test()
{
  rclcpp::init(0, nullptr);
  auto observer = std::make_shared<rclcpp::Node>("localization_contract_observer");
  auto fixture = std::make_shared<rclcpp::Node>("localization_contract_fixture");
  QTemporaryDir directory;
  HardwareStartController controller(std::make_shared<FakeHardwareStartAdapter>(), nullptr, directory.filePath("settings.ini"));
  controller.setContext("Localization graph", "Hardware", "lab-host", 62, "mur620c", "Connected", "", "");
  controller.setNode(observer);
  auto source = fixture->create_publisher<std_msgs::msg::String>("/mur620c/localization_source", rclcpp::QoS(1).transient_local());
  auto pose = fixture->create_publisher<geometry_msgs::msg::PoseStamped>("/mur620c/mir_pose_stamped_simple", 10);
  auto amcl = fixture->create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>("/mur620c/localization/amcl_pose", 10);
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(observer);
  executor.add_node(fixture);
  bool observed = false;
  for (int i = 0; i < 100; ++i) {
    std_msgs::msg::String selected; selected.data = "mir"; source->publish(selected);
    geometry_msgs::msg::PoseStamped current; current.header.frame_id = "map"; current.header.stamp = fixture->now(); pose->publish(current);
    executor.spin_some(); QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    if (controller.activeLocalization().contains("mir") && controller.localizationState() == "Ready") { observed = true; break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  controller.setOption("localization_source", "amcl");
  for (int i = 0; i < 30; ++i) { executor.spin_some(); QCoreApplication::processEvents(QEventLoop::AllEvents, 10); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
  observed = observed && controller.localizationDetail().contains("AMCL adapter detected");
  for (int i = 0; i < 50; ++i) {
    std_msgs::msg::String selected; selected.data = "amcl"; source->publish(selected);
    geometry_msgs::msg::PoseStamped current; current.header.frame_id = "map"; current.header.stamp = fixture->now(); pose->publish(current);
    executor.spin_some(); QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    if (controller.activeLocalization().contains("amcl") && controller.localizationState() == "Ready") break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  observed = observed && controller.activeLocalization().contains("amcl") && controller.localizationState() == "Ready";
  for (int i = 0; i < 70; ++i) { executor.spin_some(); QCoreApplication::processEvents(QEventLoop::AllEvents, 10); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
  observed = observed && controller.localizationState() != "Ready" && controller.activeLocalization().contains("Unknown");
  controller.setContext("Localization graph", "Hardware", "lab-host", 62, "mur620d", "Connected", "", "");
  observed = observed && !controller.activeLocalization().contains("mir");
  controller.setNode({});
  executor.remove_node(observer); executor.remove_node(fixture); rclcpp::shutdown();
  return observed;
}

bool run_localization_selection_contract_test()
{
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  const auto settings_path = directory.filePath("settings.ini");
  auto adapter = std::make_shared<FakeHardwareStartAdapter>();
  adapter->preflight.host_ready = true;
  adapter->preflight.host_detail = "Host ready";
  adapter->preflight.arms.insert("UR10_l", {"Ready", "Dashboard safety confirmed"});
  adapter->preflight.arms.insert("UR10_r", {"Ready", "Dashboard safety confirmed"});

  HardwareStartController controller(adapter, nullptr, settings_path);
  controller.setContext("Localization", "Hardware", "lab-host", 62, "mur620c", "Connected", "/tmp/ws", "$HOME/colcon_ws");
  if (controller.options().value("localization_source").toString() != "mir") return false;
  controller.setOption("launch_mir", true);
  controller.setOption("localization_source", "mocap");
  if (controller.options().value("localization_source").toString() != "mocap") return false;
  controller.setLocalizationStatus("mocap", "Blocked", "Map pose is stale; /qualisys_map/mur620c/pose missing");
  if (controller.localizationState() != "Blocked" ||
    !controller.localizationDetail().contains("stale")) return false;
  controller.runPreflight();
  controller.startHardware();
  if (adapter->launch_calls != 0 || controller.state() != "Blocked" ||
    !controller.message().contains("Map pose is stale")) return false;

  controller.setLocalizationStatus("mocap", "Ready", "Fresh map pose and map → mocap TF");
  controller.setContext("Localization", "Hardware", "other-host", 62, "mur620d", "Connected", "/tmp/ws", "$HOME/colcon_ws");
  if (controller.localizationState() != "Unknown") return false;
  controller.setLocalizationStatus("mocap", "Ready", "Fresh map pose and map → mocap TF");
  controller.runPreflight();
  controller.startHardware();
  if (adapter->launch_calls != 1 || adapter->last_config.localization_source != "mocap") return false;

  controller.setOption("localization_source", "amcl");
  controller.prepareAmcl();
  if (adapter->prepare_calls != 1 || adapter->last_config.robot != "mur620d") return false;
  if (!controller.localizationDetail().contains("adapter") || controller.localizationState() != "Unknown") return false;
  controller.runPreflight();
  controller.startHardware();
  if (adapter->launch_calls != 1 || !controller.message().contains("adapter")) return false;
  controller.setOption("localization_source", "mocap");

  auto invalid = std::make_shared<FakeHardwareStartAdapter>();
  HardwareStartController invalid_source(invalid, nullptr, directory.filePath("invalid.ini"));
  invalid_source.setContext("Localization", "Hardware", "lab-host", 62, "mur620c", "Connected", "/tmp/ws", "$HOME/colcon_ws");
  invalid_source.setOption("localization_source", "somewhere");
  if (invalid_source.options().value("localization_source").toString() != "mir") return false;

  HardwareStartController restored(std::make_shared<FakeHardwareStartAdapter>(), nullptr, settings_path);
  restored.setContext("Localization", "Hardware", "lab-host", 62, "mur620c", "Connected", "/tmp/ws", "$HOME/colcon_ws");
  return restored.options().value("localization_source").toString() == "mocap";
}

bool run_portable_preset_contract_test()
{
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  const auto bundle_path = directory.filePath("profile.murpreset.json");
  const auto invalid_path = directory.filePath("invalid.json");
  {
    SessionController session;
    if (!session.saveProfile("Portable lab", "Hardware", "lab-host", "mur620b",
      "MuR data view", "/old/workspace", "/old/config.rviz", 87)) return false;
    if (!session.setMirEndpoint("/mur620b/cmd_vel", "geometry_msgs/msg/Twist") ||
      !session.setSimEndpoint("/mur620b/mobile_base_controller/cmd_vel") ||
      !session.setNavigationEndpoints("/mur620b/move_base_simple/goal", "/mur620b/move_base/cancel", "/mur620b/robot_pose") ||
      session.setMirEndpoint("/mur620d/cmd_vel", "geometry_msgs/msg/Twist") ||
      session.setSimEndpoint("/mur620b/bad-name")) return false;
    session.selectView("Logs");
    if (!session.exportPreset(bundle_path)) return false;
    QFile exported(bundle_path);
    if (!exported.open(QIODevice::ReadOnly)) return false;
    QJsonParseError error;
    const auto exported_bytes = exported.readAll();
    const auto document = QJsonDocument::fromJson(exported_bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto bundle = document.object();
    const auto profile = bundle.value("profile").toObject();
    if (bundle.value("version").toInt() != 1 || profile.value("name").toString() != "Portable lab" ||
      profile.value("mode").toString() != "Hardware" || profile.value("rosDomain").toInt() != 87 ||
      profile.value("visualization").toString() != "MuR data view" ||
      profile.contains("view") || profile.contains("lastView") || profile.contains("logFilter") ||
      profile.contains("workspacePath") || profile.contains("configPath") ||
      !profile.value("requiresWorkspaceMapping").toBool() ||
      !profile.value("requiresConfigMapping").toBool() ||
      exported_bytes.contains("/old/workspace") || exported_bytes.contains("/old/config.rviz") ||
      profile.contains("password") || bundle.contains("credentials")) return false;
    session.saveProfile("Temporary", "Simulation", "localhost", "mur620d", "RViz", "", "", 0);
    if (!session.importPreset(bundle_path, "/new/workspace", "/new/config.rviz") ||
      session.preset() != "Portable lab" || session.mode() != "Hardware" ||
      session.target() != "lab-host" || session.rosDomain() != 87 || session.robot() != "mur620b" ||
      session.visualization() != "MuR data view" || session.workspacePath() != "/new/workspace" ||
      session.configPath() != "/new/config.rviz" ||
      session.mirCommandTopic() != "/mur620b/cmd_vel" || session.mirCommandType() != "geometry_msgs/msg/Twist" ||
      session.simCommandTopic() != "/mur620b/mobile_base_controller/cmd_vel" ||
      session.navigationGoal() != "/mur620b/move_base_simple/goal" ||
      session.navigationCancel() != "/mur620b/move_base/cancel" ||
      session.navigationPose() != "/mur620b/robot_pose") return false;
    if (!session.importPreset(bundle_path, "", "") || !session.workspacePath().isEmpty() ||
      !session.configPath().isEmpty()) return false;
    if (session.importPreset(directory.filePath("missing.json"), "", "") || session.lastError().isEmpty()) return false;
  }
  QFile invalid(invalid_path);
  if (!invalid.open(QIODevice::WriteOnly)) return false;
  invalid.write("{\"format\":\"mur-command-center-preset\",\"version\":99}");
  invalid.close();
  SessionController restored;
  if (restored.mirCommandTopic() != "/mur620b/cmd_vel" ||
    restored.simCommandTopic() != "/mur620b/mobile_base_controller/cmd_vel" ||
    restored.navigationGoal() != "/mur620b/move_base_simple/goal") return false;
  restored.selectMode("Simulation");
  if (!restored.navigationGoal().isEmpty()) return false;
  restored.selectMode("Hardware");
  if (restored.navigationGoal() != "/mur620b/move_base_simple/goal") return false;
  return !restored.importPreset(invalid_path, "", "") &&
    restored.lastError().contains("Unsupported or invalid");
}

bool run_gazebo_preset_contract_test()
{
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  const auto settings_path = directory.filePath("settings.ini");
  const auto bundle_path = directory.filePath("preset.json");
  const auto had_settings = qEnvironmentVariableIsSet("MUR_COMMAND_CENTER_SETTINGS");
  const auto previous_settings = qEnvironmentVariable("MUR_COMMAND_CENTER_SETTINGS");
  qputenv("MUR_COMMAND_CENTER_SETTINGS", settings_path.toLocal8Bit());
  const bool passed = [&]() {
    SessionController session;
    if (session.gazeboViewOnSimulationStart()) return false;
    session.setGazeboViewOnSimulationStart(true);
    if (!session.gazeboViewOnSimulationStart() ||
      !session.saveProfile("Hardware only", "Hardware", "robot-host", "mur620b", "RViz", "", "", 62, false) ||
      session.gazeboViewOnSimulationStart()) return false;
    session.selectPreset("Local simulation");
    if (!session.gazeboViewOnSimulationStart() || !session.exportPreset(bundle_path)) return false;
    session.selectPreset("Temporary");
    if (!session.importPreset(bundle_path, "", "") || session.preset() != "Local simulation" ||
      !session.gazeboViewOnSimulationStart()) return false;
    return true;
  }();
  if (passed) {
    SessionController restored;
    if (restored.preset() != "Local simulation" || !restored.gazeboViewOnSimulationStart()) {
      if (had_settings) qputenv("MUR_COMMAND_CENTER_SETTINGS", previous_settings.toLocal8Bit());
      else qunsetenv("MUR_COMMAND_CENTER_SETTINGS");
      return false;
    }
  }
  if (had_settings) qputenv("MUR_COMMAND_CENTER_SETTINGS", previous_settings.toLocal8Bit());
  else qunsetenv("MUR_COMMAND_CENTER_SETTINGS");
  return passed;
}

bool run_rviz_session_contract_test()
{
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  const auto sim_path = directory.filePath("simulation.rviz");
  const auto hardware_path = directory.filePath("hardware.rviz");
  const auto override_path = directory.filePath("override.rviz");
  const auto empty_path = directory.filePath("empty.rviz");
  const auto invalid_path = directory.filePath("invalid.rviz");
  for (const auto & path : {sim_path, hardware_path, override_path}) {
    QFile config(path);
    if (!config.open(QIODevice::WriteOnly) || config.write("Visualization Manager:\n") < 1) return false;
  }
  QFile empty(empty_path);
  if (!empty.open(QIODevice::WriteOnly)) return false;
  empty.close();
  QFile invalid(invalid_path);
  if (!invalid.open(QIODevice::WriteOnly) || invalid.write("not an RViz display config\n") < 1) return false;
  invalid.close();

  SessionController session;
  QString error;
  if (!session.saveProfile("Simulation view", "Simulation", "localhost", "mur620d", "RViz", "", sim_path, session.rosDomain()) ||
    !session.useSimTime() || session.rvizConfigPath("") != sim_path ||
    !session.validateRvizProfile("", &error) || !error.isEmpty()) return false;
  const auto simulation_context = session.contextKey("");
  if (session.rvizConfigPath(override_path) != override_path ||
    !session.validateRvizProfile(override_path, &error)) return false;

  if (!session.saveProfile("Hardware view", "Hardware", "robot-host", "mur620b", "RViz", "", hardware_path, session.rosDomain()) ||
    session.useSimTime() || session.rvizConfigPath("") != hardware_path ||
    !session.validateRvizProfile("", &error)) return false;
  const auto hardware_context = session.contextKey("");
  if (simulation_context == hardware_context) return false;
  const auto hardware_host_context = session.contextKey("");
  session.selectTarget("another-robot-host");
  const auto alternate_host_context = session.contextKey("");
  if (hardware_host_context == alternate_host_context) return false;
  const auto previous_domain = session.rosDomain();
  session.selectRosDomain(89);
  const auto alternate_domain_context = session.contextKey("");
  if (alternate_domain_context == alternate_host_context) return false;
  session.selectRosDomain(previous_domain);

  if (!session.saveProfile("Missing view", "Simulation", "localhost", "mur620d", "RViz", "", "", session.rosDomain()) ||
    session.validateRvizProfile("", &error) || !error.contains("no RViz configuration")) return false;
  if (!session.saveProfile("Empty view", "Hardware", "localhost", "mur620d", "RViz", "", empty_path, session.rosDomain()) ||
    session.validateRvizProfile("", &error) || !error.contains("is empty")) return false;
  if (!session.saveProfile("Invalid view", "Hardware", "localhost", "mur620d", "RViz", "", invalid_path, session.rosDomain()) ||
    session.validateRvizProfile("", &error) || !error.contains("is invalid")) return false;
  if (!session.saveProfile("Unsupported view", "Hardware", "localhost", "mur620d", "MuR data view", "", hardware_path, session.rosDomain()) ||
    session.validateRvizProfile("", &error) || !error.contains("RViz is unavailable")) return false;

  rclcpp::init(0, nullptr);
  bool time_parameter_matches_profile = true;
  for (const auto & profile : {std::pair<std::string, std::string>{"sim", "Simulation"},
      {"hardware", "Hardware"}}) {
    if (!session.saveProfile("Time source", QString::fromStdString(profile.second), "localhost",
      "mur620d", "RViz", "", profile.first == "sim" ? sim_path : hardware_path,
      session.rosDomain())) return false;
    auto node = std::make_shared<rviz_common::ros_integration::RosNodeAbstraction>(
      "rviz_time_contract_" + profile.first);
    node->get_raw_node()->set_parameter(rclcpp::Parameter("use_sim_time", session.useSimTime()));
    time_parameter_matches_profile = time_parameter_matches_profile &&
      node->get_raw_node()->get_parameter("use_sim_time").as_bool() == session.useSimTime();
  }
  rclcpp::shutdown();
  if (!time_parameter_matches_profile) return false;
  return true;
}

bool run_remote_discovery_contract_test()
{
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  const auto old_domain = qgetenv("ROS_DOMAIN_ID");
  const auto old_range = qgetenv("ROS_AUTOMATIC_DISCOVERY_RANGE");
  const auto old_fastdds = qgetenv("FASTDDS_DEFAULT_PROFILES_FILE");
  const auto old_fastrtps = qgetenv("FASTRTPS_DEFAULT_PROFILES_FILE");
  const auto old_discovery_server = qgetenv("ROS_DISCOVERY_SERVER");
  const auto old_localhost_only = qgetenv("ROS_LOCALHOST_ONLY");
  const auto old_owned_profile = qgetenv("MUR_GUI_DDS_PROFILE_OWNED_PATH");
  const auto old_rmw = qgetenv("RMW_IMPLEMENTATION");
  const bool had_domain = qEnvironmentVariableIsSet("ROS_DOMAIN_ID");
  const bool had_range = qEnvironmentVariableIsSet("ROS_AUTOMATIC_DISCOVERY_RANGE");
  const bool had_fastdds = qEnvironmentVariableIsSet("FASTDDS_DEFAULT_PROFILES_FILE");
  const bool had_fastrtps = qEnvironmentVariableIsSet("FASTRTPS_DEFAULT_PROFILES_FILE");
  const bool had_discovery_server = qEnvironmentVariableIsSet("ROS_DISCOVERY_SERVER");
  const bool had_localhost_only = qEnvironmentVariableIsSet("ROS_LOCALHOST_ONLY");
  const bool had_owned_profile = qEnvironmentVariableIsSet("MUR_GUI_DDS_PROFILE_OWNED_PATH");
  const bool had_rmw = qEnvironmentVariableIsSet("RMW_IMPLEMENTATION");
  qputenv("ROS_AUTOMATIC_DISCOVERY_RANGE", "SUBNET");
  qunsetenv("ROS_DISCOVERY_SERVER");
  qunsetenv("ROS_LOCALHOST_ONLY");
  qunsetenv("MUR_GUI_DDS_PROFILE_OWNED_PATH");
  qunsetenv("RMW_IMPLEMENTATION");
  qunsetenv("FASTDDS_DEFAULT_PROFILES_FILE");
  qunsetenv("FASTRTPS_DEFAULT_PROFILES_FILE");
  bool passed = false;
  try {
    const auto route = configure_ros_environment("robot.lab.example", 87,
      {QHostAddress("10.8.0.21")}, directory.path());
    QFile profile(qEnvironmentVariable("FASTDDS_DEFAULT_PROFILES_FILE"));
    passed = qEnvironmentVariable("ROS_DOMAIN_ID") == "87" && profile.open(QIODevice::ReadOnly);
    const auto xml = QString::fromUtf8(profile.readAll());
    passed = passed && route.contains("direct DDS peer route") &&
      route.contains("UDP reachability is not yet confirmed") &&
      xml.contains("<address>10.8.0.21</address>") && xml.contains("<port>29160</port>") &&
      qEnvironmentVariable("FASTDDS_DEFAULT_PROFILES_FILE") ==
      qEnvironmentVariable("FASTRTPS_DEFAULT_PROFILES_FILE");
    configure_ros_environment("robot.lab.example", 87,
      {QHostAddress("10.8.0.22")}, directory.path());
    QFile refreshed(qEnvironmentVariable("FASTDDS_DEFAULT_PROFILES_FILE"));
    passed = passed && refreshed.open(QIODevice::ReadOnly);
    const auto refreshed_xml = QString::fromUtf8(refreshed.readAll());
    passed = passed && refreshed_xml.contains("<address>10.8.0.22</address>") &&
      !refreshed_xml.contains("<address>10.8.0.21</address>");
    configure_ros_environment("localhost", 87, {}, directory.path());
    passed = passed && !qEnvironmentVariableIsSet("FASTDDS_DEFAULT_PROFILES_FILE") &&
      !qEnvironmentVariableIsSet("FASTRTPS_DEFAULT_PROFILES_FILE");
    bool invalid_domain_rejected = false;
    try { configure_ros_environment("robot.lab.example", 233, {}, directory.path()); }
    catch (const std::invalid_argument &) { invalid_domain_rejected = true; }
    bool credentials_rejected = false;
    try { configure_ros_environment("user:secret@robot", 87, {}, directory.path()); }
    catch (const std::invalid_argument &) { credentials_rejected = true; }
    passed = passed && invalid_domain_rejected && credentials_rejected;
    if (passed) {
      rclcpp::init(0, nullptr);
      passed = rclcpp::contexts::get_global_default_context()->get_domain_id() == 87;
      rclcpp::shutdown();
    }
  } catch (...) { passed = false; }
  auto restore = [](const char * key, bool existed, const QByteArray & value) {
      if (existed) qputenv(key, value); else qunsetenv(key);
    };
  restore("ROS_DOMAIN_ID", had_domain, old_domain);
  restore("ROS_AUTOMATIC_DISCOVERY_RANGE", had_range, old_range);
  restore("FASTDDS_DEFAULT_PROFILES_FILE", had_fastdds, old_fastdds);
  restore("FASTRTPS_DEFAULT_PROFILES_FILE", had_fastrtps, old_fastrtps);
  restore("ROS_DISCOVERY_SERVER", had_discovery_server, old_discovery_server);
  restore("ROS_LOCALHOST_ONLY", had_localhost_only, old_localhost_only);
  restore("MUR_GUI_DDS_PROFILE_OWNED_PATH", had_owned_profile, old_owned_profile);
  restore("RMW_IMPLEMENTATION", had_rmw, old_rmw);
  return passed && !rclcpp::ok();
}

bool run_process_manager_contract_test()
{
  const auto fake_launcher = qEnvironmentVariable("MUR_FAKE_LAUNCHER");
  if (fake_launcher.isEmpty() || !QFileInfo::exists(fake_launcher)) return false;
  ProcessManager manager;
  if (!manager.startProcess("fake exit", fake_launcher, {"exit"})) return false;
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 5000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    const auto rows = manager.processes();
    if (!rows.isEmpty() && rows.first().toMap().value("state").toString() == "Failed") break;
  }
  auto rows = manager.processes();
  if (rows.size() != 1 || rows.first().toMap().value("state").toString() != "Failed" ||
    rows.first().toMap().value("exitCode").toInt() != 7 ||
    !manager.log().contains("fake stdout") || !manager.log().contains("fake stderr") ||
    !manager.log().contains("domain=47")) {
    qCritical("fake exit contract: rows=%s log=%s", qPrintable(QString::fromUtf8(
      QJsonDocument::fromVariant(rows).toJson())), qPrintable(manager.log()));
    return false;
  }

  if (!manager.startProcess("fake hold", fake_launcher, {"hold"})) return false;
  timer.restart();
  while (timer.elapsed() < 5000 && !manager.log().contains("fake hold ready")) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }
  if (!manager.log().contains("fake hold ready")) {
    qCritical("fake hold did not start: %s", qPrintable(manager.log()));
    return false;
  }
  manager.stopProcess("fake hold");
  timer.restart();
  while (timer.elapsed() < 5000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    rows = manager.processes();
    for (const auto & row : rows) {
      if (row.toMap().value("name").toString() == "fake hold" &&
        row.toMap().value("state").toString() == "Stopped") {
        if (!manager.startSimulation("mur620d")) return false;
        timer.restart();
        while (timer.elapsed() < 5000) {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
          for (const auto & simulation_row : manager.processes()) {
            if (simulation_row.toMap().value("name").toString() == "MUR620 simulation" &&
              simulation_row.toMap().value("state").toString() == "Exited") {
              if (!manager.log().contains(
                "mur_launch_sim', 'mur620.launch.py', 'robot_name:=mur620d', "
                "'include_gz:=true', 'gazebo_gui:=false', 'launch_rviz:=false'")) return false;
              break;
            }
          }
          if (manager.log().contains("fake simulation argv=")) break;
        }
        if (!manager.startProcess("fake simulation server", fake_launcher, {"hold"}) ||
          !manager.startGazeboView()) return false;
        timer.restart();
        while (timer.elapsed() < 5000 && !manager.log().contains("fake gz argv=")) {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        if (!manager.log().contains("fake gz argv=['sim', '-g']")) return false;
        manager.stopProcess("Gazebo world view");
        timer.restart();
        bool client_stopped = false;
        while (timer.elapsed() < 5000) {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
          for (const auto & row : manager.processes()) {
            const auto map = row.toMap();
            if (map.value("name").toString() == "Gazebo world view" &&
              map.value("state").toString() == "Stopped") client_stopped = true;
            if (map.value("name").toString() == "fake simulation server" &&
              map.value("state").toString() != "Running") return false;
          }
          if (client_stopped) break;
        }
        if (!client_stopped) return false;
        manager.stopProcess("fake simulation server");
        return true;
      }
    }
  }
  return false;
}

bool run_remote_process_manager_contract_test()
{
  const auto fake_ssh = qEnvironmentVariable("MUR_FAKE_SSH");
  if (fake_ssh.isEmpty() || !QFileInfo::exists(fake_ssh)) return false;
  ProcessManager manager(nullptr, fake_ssh);
  if (manager.startRemoteSimulation("bad host", 87, "mur620a") ||
    manager.startRemoteSimulation("sim-host", 233, "mur620a") ||
    manager.startRemoteSimulation("sim-host", 87, "bad;robot")) return false;
  if (!manager.startRemoteSimulation("sim-host", 87, "mur620a")) return false;

  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 5000 && !manager.log().contains("remote launch ready")) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }
  auto rows = manager.processes();
  if (rows.size() != 1 || rows.first().toMap().value("host").toString() != "sim-host" ||
    rows.first().toMap().value("state").toString() != "Running" ||
    !manager.log().contains("remote launch ready")) {
    qCritical("remote launch did not become visible: %s", qPrintable(manager.log()));
    return false;
  }
  manager.stopProcess("Remote MUR620 simulation");
  timer.restart();
  while (timer.elapsed() < 5000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    rows = manager.processes();
    if (rows.first().toMap().value("state").toString() == "Stopped") {
      if (!manager.log().contains("Stop signal sent to app-owned remote run") ||
        !manager.log().contains("remote launch stopped")) {
        qCritical("remote simulation stop output missing: %s", qPrintable(manager.log())); return false;
      }
      const auto stop_file = qEnvironmentVariable("MUR_FAKE_REMOTE_STOP_FILE");
      if (stop_file.isEmpty() || !QFile::remove(stop_file)) {
        qCritical("remote simulation stop marker could not be removed: %s exists=%d", qPrintable(stop_file), QFileInfo::exists(stop_file)); return false;
      }
      QVariantMap options{{"arm_l", false}, {"arm_r", true}, {"launch_mir", true},
        {"launch_cameras", true}, {"localization_source", "mocap"}, {"integrated_controller", true}, {"use_ft_sensor", false},
        {"require_wrench", true}, {"collision_avoidance", false}, {"collision_markers", true},
        {"zero_admittance", true}, {"launch_moveit", true}};
      const QVariantMap no_components{{"arm_l", false}, {"arm_r", false}, {"launch_mir", false}};
      if (manager.startRemoteHardware("lab host", 87, "mur620a", "$HOME/colcon_ws", options) ||
        manager.startRemoteHardware("lab-host", 233, "mur620a", "$HOME/colcon_ws", options) ||
        manager.startRemoteHardware("lab-host", 87, "mur620a", "bad;path", options) ||
        manager.startRemoteHardware("lab-host", 87, "mur620a", "$HOME/colcon_ws", no_components)) {
        qCritical("remote hardware validation accepted invalid config"); return false;
      }
      if (!manager.startRemoteHardware("lab-host", 87, "mur620a", "$HOME/colcon_ws", options)) {
        qCritical("remote hardware launch was rejected before SSH: log=%s", qPrintable(manager.log())); return false;
      }
      timer.restart();
      bool hardware_running = false;
      while (timer.elapsed() < 5000 && !hardware_running) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        rows = manager.processes();
        for (const auto & row : rows) {
          const auto map = row.toMap();
          if (map.value("name").toString() == "Hardware · mur620a" &&
            map.value("host").toString() == "lab-host" && map.value("state").toString() == "Running") {
            hardware_running = true;
          }
        }
      }
      if (!hardware_running) { qCritical("fake hardware process did not run: %s", qPrintable(manager.log())); return false; }
      manager.stopProcess("Hardware · mur620a");
      timer.restart();
      while (timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        rows = manager.processes();
        for (const auto & row : rows) {
          const auto map = row.toMap();
          if (map.value("name").toString() == "Hardware · mur620a" &&
            map.value("state").toString() == "Stopped") return true;
        }
      }
      qCritical("fake hardware process did not stop: %s", qPrintable(manager.log()));
      return false;
    }
  }
  qCritical("remote run failed to stop safely: %s", qPrintable(manager.log()));
  return false;
}

QScrollArea * dock_viewport(QWidget * content)
{
  auto * viewport = new QScrollArea;
  viewport->setFrameShape(QFrame::NoFrame);
  viewport->setWidgetResizable(true);
  viewport->setWidget(content);
  return viewport;
}

class PrototypeWindow : public QMainWindow
{
public:
  PrototypeWindow(const QString & config_path, const QString & context, const QString & qml_path,
    const QSize & desktop_size)
  : QMainWindow(), context_(context), config_override_(config_path), session_(new SessionController()),
    sim_base_jog_adapter_(std::make_shared<RosSimBaseJogAdapter>()),
    sim_base_jog_(new SimulationBaseJog(sim_base_jog_adapter_, this)),
    mir_hardware_adapter_(std::make_shared<RosMirHardwareAdapter>()),
    mir_hardware_control_(new MirHardwareControl(mir_hardware_adapter_, this)),
    goals_poses_adapter_(std::make_shared<RosGoalsPosesAdapter>()),
    goals_poses_(new GoalsPoses(goals_poses_adapter_,
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/poses.json", this)),
    mocap_(new MocapController(this)),
    ur_arm_jog_adapter_(std::make_shared<RosUrArmJogAdapter>()),
    ur_arm_jog_(new UrArmJog(ur_arm_jog_adapter_, this)),
    lift_adapter_(std::make_shared<RosLiftAdapter>()),
    lift_controller_(new LiftController(lift_adapter_, this))
  {
    setWindowTitle(QString("MuR Command Center · %1").arg(context_));
    const auto window_budget = (desktop_size - QSize(32, 48)).expandedTo(QSize(1, 1));
    setMinimumSize(QSize(840, 460).boundedTo(window_budget));
    resize(QSize(1440, 900).boundedTo(window_budget));

    auto * command_center = new QQuickWidget(this);
    command_center->setResizeMode(QQuickWidget::SizeRootObjectToView);
    command_center->rootContext()->setContextProperty("session", session_);
    command_center->rootContext()->setContextProperty("simBaseJog", sim_base_jog_);
    command_center->rootContext()->setContextProperty("goalsPoses", goals_poses_);
    command_center->rootContext()->setContextProperty("mirHardwareControl", mir_hardware_control_);
    mir_hardware_control_->setContext(session_->robot(), session_->mode(), session_->preset() + "|" + session_->target(), false,
      session_->mirCommandTopic(), session_->mirCommandType());
    command_center->rootContext()->setContextProperty("mocap", mocap_);
    command_center->rootContext()->setContextProperty("urArmJog", ur_arm_jog_);
    command_center->rootContext()->setContextProperty("liftController", lift_controller_);
    lift_controller_->setContext(session_->robot(), session_->mode(), session_->target(), false, session_->preset());
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
      if (state != Qt::ApplicationActive) lift_controller_->stop("application lost focus");
    });
    process_manager_ = new ProcessManager(this);
    mocap_->setProcessManager(process_manager_);
    motion_adapter_ = std::make_shared<SshUrMotionActionsAdapter>(ur_arm_jog_adapter_, process_manager_);
    motion_actions_ = new UrMotionActions(motion_adapter_, this);
    motion_actions_->setJog(ur_arm_jog_);
    freedrive_adapter_ = std::make_shared<RosUrFreedriveAdapter>(ur_arm_jog_adapter_);
    freedrive_ = new UrFreedrive(freedrive_adapter_, this);
    freedrive_->setJog(ur_arm_jog_); freedrive_->setMotion(motion_actions_);
    auto update_interlock = [this]() {
      ur_arm_jog_->setMotionBlocked(motion_actions_->busy() || freedrive_->busy());
      motion_actions_->setMotionBlocked(freedrive_->busy());
    };
    connect(motion_actions_, &UrMotionActions::changed, this, update_interlock);
    connect(freedrive_, &UrFreedrive::changed, this, update_interlock);
    connect(freedrive_, &UrFreedrive::changed, this, [this]() {
      if (pending_disconnect_ && !freedrive_->busy()) {
        pending_disconnect_ = false;
        QTimer::singleShot(0, this, [this]() { disconnect_ros(); if (pending_close_) { pending_close_ = false; close(); } });
      }
    });
    command_center->rootContext()->setContextProperty("urFreedrive", freedrive_);
    command_center->rootContext()->setContextProperty("urMotionActions", motion_actions_);
    command_center->rootContext()->setContextProperty("processes", process_manager_);
    log_store_ = new LogStore(this);
    process_manager_->setLogStore(log_store_);
    hardware_start_adapter_ = std::make_shared<SshHardwareStartAdapter>(process_manager_, log_store_);
    hardware_start_ = new HardwareStartController(hardware_start_adapter_, this);
    hardware_start_->setContext(session_->preset(), session_->mode(), session_->target(),
      session_->rosDomain(), session_->robot(), session_->connectionState(), session_->workspacePath(), "");
    command_center->rootContext()->setContextProperty("hardwareStart", hardware_start_);
    connect(hardware_start_, &HardwareStartController::changed, this, [this]() { update_motion_context(); });
    shutdown_adapter_ = std::make_shared<SshUrShutdownAdapter>();
    shutdown_ = new UrShutdown(shutdown_adapter_, this);
    auto inhibited = [this](const QString & robot, const QString & side) {
      return shutdown_->inhibited(session_->target(), robot, side);
    };
    ur_arm_jog_->setShutdownInterlock(inhibited);
    motion_actions_->setShutdownInterlock(inhibited);
    freedrive_->setShutdownInterlock(inhibited);
    hardware_start_->setShutdownInterlock([this]() {
      return shutdown_->busy() || shutdown_->anyInhibited(session_->target(), session_->robot());
    });
    shutdown_->setStartBusyHook([this]() {
      if (hardware_start_->busy()) return true;
      for (const auto & item : process_manager_->processes()) {
        const auto process = item.toMap();
        const auto state = process.value("state").toString();
        if (process.value("name").toString() == "Hardware · " + session_->robot() &&
            process.value("host").toString() == session_->target() &&
            (state == "Connecting" || state == "Starting" || state == "Running" || state == "Stopping" || state == "Stop unconfirmed" || state == "Stop failed")) return true;
      }
      return false;
    });
    shutdown_->setMotionHooks([this](const QString & robot, const QString & side) {
      if (ur_arm_jog_->robot() == robot && ur_arm_jog_->side() == side) ur_arm_jog_->stop("UR shutdown");
      if (motion_actions_->robot() == robot && motion_actions_->side() == side) motion_actions_->cancel();
      if (freedrive_->robot() == robot && freedrive_->side() == side) freedrive_->leave();
    }, [this](const QString & robot, const QString & side) {
      Q_UNUSED(robot); Q_UNUSED(side);
      return motion_actions_->busy() || freedrive_->busy();
    });
    command_center->rootContext()->setContextProperty("urShutdown", shutdown_);
    update_motion_context();
    connect(mocap_, &MocapController::changed, this, [this]() {
      if (session_->connectionState() != "Connected") return;
      const auto detail = mocap_->mapStatus();
      const bool ready = detail == "Map pose and map → mocap TF active";
      hardware_start_->setLocalizationStatus("mocap", ready ? "Ready" : "Waiting", detail);
    });
    log_store_->append("ROS /rosout", "WARN",
      "Live ROS logs are unavailable until this session connects to a ROS graph.");
    log_panel_ = new LogPanel(log_store_);
    command_center->rootContext()->setContextProperty("logs", log_panel_);
    command_center->setSource(QUrl::fromLocalFile(qml_path));
    setCentralWidget(command_center);
    connect(&ros_spin_timer_, &QTimer::timeout, this, &PrototypeWindow::spin_ros);
    ros_spin_timer_.setInterval(10);
    connect(&diagnosis_timer_, &QTimer::timeout, this, &PrototypeWindow::refresh_diagnosis);
    connect(session_, &SessionController::connectRequested, this, [this]() { connect_ros(); });
    connect(session_, &SessionController::disconnectRequested, this, [this]() { disconnect_ros(); });
    for (int index = 0; index < 8; ++index) {
      auto * shortcut = new QShortcut(QKeySequence(QString::number(index + 1)), this);
      shortcut->setContext(Qt::WidgetWithChildrenShortcut);
      connect(shortcut, &QShortcut::activated, this, [this, index]() {
        static const QStringList views = {"Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings", "Mocap"};
        session_->selectView(views[index]);
      });
    }
    connect(session_, &SessionController::changed, this, [this]() {
      ensure_session_context();
      if (hardware_start_) hardware_start_->setContext(session_->preset(), session_->mode(),
        session_->target(), session_->rosDomain(), session_->robot(), session_->connectionState(),
        session_->workspacePath(), "");
      update_motion_context();
      mocap_->setRobot(session_->robot());
      lift_controller_->setContext(session_->robot(), session_->mode(), session_->target(),
        session_->connectionState() == "Connected", session_->preset());
      ur_arm_jog_->setContext(session_->robot(), session_->mode(), session_context(),
        session_->connectionState() == "Connected");
      goals_poses_adapter_->setEndpoints(session_->robot(), session_->mode(), session_->navigationGoal(),
        session_->navigationCancel(), session_->navigationPose());
      goals_poses_->setConnected(session_->connectionState() == "Connected" &&
        (session_->mode() == "Hardware" || session_->mode() == "Simulation"));
      mir_hardware_control_->setContext(session_->robot(), session_->mode(), session_->preset() + "|" + session_->target(),
        session_->connectionState() == "Connected", session_->mirCommandTopic(), session_->mirCommandType());
      setWindowTitle(QString("MuR Command Center · %1 · %2 · %3")
        .arg(session_->mode(), session_->preset(), session_->robot()));
      if (rviz_dock_) rviz_dock_->setWindowTitle("Visualisierung · " + session_label());
      if (logs_dock_) logs_dock_->setWindowTitle("Logs · " + session_label());
      if (log_panel_) log_panel_->setSessionLabel(session_label());
      statusBar()->showMessage(QString("%1 · %2 · %3 · %4")
        .arg(session_->target(), session_->robot(), session_->mode(), session_->connectionState()));
    });

    logs_dock_ = new QDockWidget("Logs · " + session_label(), this);
    logs_dock_->setObjectName("logsDock");
    logs_dock_->setWidget(dock_viewport(log_panel_));
    logs_dock_->setAllowedAreas(Qt::AllDockWidgetAreas);
    logs_dock_->setFeatures(QDockWidget::DockWidgetClosable |
      QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    log_panel_->setDock(logs_dock_);
    log_panel_->setSessionLabel(session_label());
    addDockWidget(Qt::BottomDockWidgetArea, logs_dock_);
    resizeDocks({logs_dock_}, {qMin(220, height() / 3)}, Qt::Vertical);
    if (height() < 620) logs_dock_->hide();

    statusBar()->showMessage(QString("%1 · %2 · Disconnected · No processes started")
      .arg(session_->target(), session_->robot()));
  }

  void spin_ros()
  {
    if (ros_executor_) ros_executor_->spin_some();
  }

  void refresh_diagnosis()
  {
    if (!ensure_session_context()) return;
    if (session_monitor_) {
      session_->setSimulationServerAvailable(session_monitor_->simulationServerAvailable());
      session_->setDiagnostics(session_diagnostics());
    }
  }

  void update_motion_context()
  {
    if (!motion_actions_ || !hardware_start_) return;
    const auto options = hardware_start_->options();
    if (shutdown_) shutdown_->setContext(session_->mode(), session_->target(), session_->robot(),
      session_->connectionState(), options.value("remote_workspace").toString(),
      options.value("arm_l").toBool(), options.value("arm_r").toBool());
    freedrive_->setContext(session_->robot(), session_->mode(), session_context(),
      session_->connectionState() == "Connected", options.value("arm_l").toBool(), options.value("arm_r").toBool());
    motion_actions_->setContext(session_->preset(), session_->mode(), session_->target(),
      session_->rosDomain(), session_->robot(), session_->connectionState(), session_context(),
      options.value("remote_workspace").toString(), options.value("arm_l").toBool(),
      options.value("arm_r").toBool());
  }

  QString session_context() const
  {
    return session_->contextKey(config_override_);
  }

  QString session_label() const
  {
    return QString("%1 · %2 · %3 · %4 · domain %5")
      .arg(session_->mode(), session_->preset(), session_->target(), session_->robot())
      .arg(session_->rosDomain());
  }

  QStringList session_diagnostics() const
  {
    auto diagnostics = session_monitor_->diagnose(session_->target(), session_->robot(),
      session_->mode(), session_->rosDomain());
    diagnostics.prepend(QString("RViz: profile %1 · config %2")
      .arg(session_->preset(), session_->rvizConfigPath(config_override_)));
    diagnostics.prepend(QString("Time source: %1")
      .arg(session_->useSimTime() ? "simulation (/clock)" : "system time"));
    return diagnostics;
  }

  bool ensure_session_context()
  {
    if (!pending_context_.isEmpty() && session_->connectionState() == "Connecting" &&
      pending_context_ != session_context()) {
      pending_context_.clear();
      session_->setDiscoveryRoute(QString());
      session_->setDiagnostics({"Session context changed before RViz connected; connect again to use the selected profile"});
      session_->setConnectionState("Disconnected");
      return false;
    }
    if (rviz_node_ && !connected_context_.isEmpty() && connected_context_ != session_context()) {
      const auto old_context = connected_context_;
      if (freedrive_->busy()) { disconnect_ros(); return false; }
      disconnect_ros();
      session_->setDiagnostics({"Session context changed · RViz disconnected; connect to attach the selected profile",
        QString("Previous RViz context: %1").arg(old_context)});
      return false;
    }
    return true;
  }

  void connect_ros()
  {
    if (rviz_node_) return;
    QString profile_error;
    if (!session_->validateRvizProfile(config_override_, &profile_error)) {
      session_->setDiagnostics({"RViz unavailable · " + profile_error,
        "External processes were not started or stopped"});
      session_->setConnectionState("Failed: " + profile_error);
      return;
    }
    const QString host = session_->target().trimmed();
    const int domain = session_->rosDomain();
    if (!valid_remote_host(host)) {
      session_->setDiagnostics({"Host/network: invalid target host name"});
      session_->setConnectionState("Failed: invalid target host");
      return;
    }
    session_->setConnectionState("Connecting");
    pending_context_ = session_context();
    auto resolved = [this, host, domain](const QHostInfo & info) {
        if (session_->connectionState() != "Connecting") return;
        if (pending_context_.isEmpty() || pending_context_ != session_context()) {
          ensure_session_context();
          return;
        }
        QList<QHostAddress> addresses;
        for (const auto & address : info.addresses()) {
          if (address.protocol() == QAbstractSocket::IPv4Protocol) addresses.append(address);
        }
        if (host != "localhost" && host != "127.0.0.1" && addresses.isEmpty()) {
          session_->setDiscoveryRoute(QString("Host/network: DNS resolution failed for %1 (%2); ROS graph was not queried")
            .arg(host, info.errorString()));
          session_->setDiagnostics({"ROS graph: unavailable because the target host did not resolve"});
          session_->setConnectionState("Failed: host resolution failed");
          return;
        }
        try {
          const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/dds";
          session_->setDiscoveryRoute(configure_ros_environment(host, domain, addresses, directory));
          start_ros_connection();
        } catch (const std::exception & error) {
          session_->setDiagnostics({QString("Host/network setup failed · %1").arg(error.what()),
            "ROS graph: not queried"});
          session_->setConnectionState(QString("Failed: %1").arg(error.what()));
        }
      };
    if (host == "localhost" || host == "127.0.0.1" || host == "::1") {
      QHostInfo local;
      resolved(local);
    } else {
      QHostInfo::lookupHost(host, this, resolved);
    }
  }

  void start_ros_connection()
  {
    try {
      // DDS domain/profile are set by connect_ros() before this first init.
      if (!rclcpp::ok()) rclcpp::init(0, nullptr);
      ros_executor_ = std::make_unique<rclcpp::executors::SingleThreadedExecutor>();
      rviz_node_ = std::make_shared<rviz_common::ros_integration::RosNodeAbstraction>(
        "mur_rviz_docking_prototype");
      rviz_node_->get_raw_node()->set_parameter(
        rclcpp::Parameter("use_sim_time", session_->useSimTime()));
      sim_base_jog_adapter_->setNode(rviz_node_->get_raw_node());
      ur_arm_jog_adapter_->setNode(rviz_node_->get_raw_node());
      freedrive_adapter_->setNode(rviz_node_->get_raw_node());
      lift_adapter_->setNode(rviz_node_->get_raw_node());
      mir_hardware_adapter_->setNode(rviz_node_->get_raw_node());
      goals_poses_adapter_->setNode(rviz_node_->get_raw_node());
      mocap_->setNode(rviz_node_->get_raw_node());
      hardware_start_->setNode(rviz_node_->get_raw_node());
      goals_poses_->setConnected(session_->mode() == "Hardware" || session_->mode() == "Simulation");
      ros_executor_->add_node(rviz_node_->get_raw_node());
      session_monitor_ = std::make_unique<RosSessionMonitor>(rviz_node_->get_raw_node(), log_store_);
      log_store_->append("ROS /rosout", "INFO",
        "Subscribed to /rosout; only messages published on the connected ROS graph are available.");
      auto * frame = new rviz_common::VisualizationFrame(rviz_node_);
      frame->setApp(qApp);
      frame->setSplashPath("");
      frame->setHideButtonVisibility(false);
      frame->initialize(rviz_node_);
      rviz_dock_ = new QDockWidget("Visualisierung · " + session_label(), this);
      rviz_dock_->setObjectName("rvizVisualizationDock");
      rviz_dock_->setWidget(dock_viewport(frame));
      rviz_dock_->setAllowedAreas(Qt::AllDockWidgetAreas);
      rviz_dock_->setFeatures(QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
      addDockWidget(Qt::RightDockWidgetArea, rviz_dock_);
      connect(rviz_dock_, &QDockWidget::visibilityChanged, frame, [frame](bool visible) {
        if (visible) frame->setFocus(Qt::OtherFocusReason);
      });
      frame->loadDisplayConfig(session_->rvizConfigPath(config_override_));
      resizeDocks({rviz_dock_}, {width() / 3}, Qt::Horizontal);
      ros_spin_timer_.start(10);
      diagnosis_timer_.start(500);
      connected_context_ = pending_context_.isEmpty() ? session_context() : pending_context_;
      pending_context_.clear();
      session_->setConnectionState("Connected");
      session_->setDiagnostics(session_diagnostics());
    } catch (const std::exception & error) {
      session_monitor_.reset();
      session_->setSimulationServerAvailable(false);
      ros_spin_timer_.stop();
      diagnosis_timer_.stop();
      if (rviz_dock_) {
        removeDockWidget(rviz_dock_);
        delete rviz_dock_;
        rviz_dock_ = nullptr;
      }
      if (ros_executor_ && rviz_node_) ros_executor_->remove_node(rviz_node_->get_raw_node());
      ros_executor_.reset();
      rviz_node_.reset();
      sim_base_jog_adapter_->setNode({});
      ur_arm_jog_adapter_->setNode({});
      lift_adapter_->setNode({});
      mir_hardware_adapter_->setNode({});
      goals_poses_adapter_->setNode({});
      mocap_->setNode({});
      hardware_start_->setNode({});
      goals_poses_->setConnected(false);
      pending_context_.clear();
      connected_context_.clear();
      if (rclcpp::ok()) rclcpp::shutdown();
      session_->setDiagnostics({QString("Connection failed · %1").arg(error.what()),
        "Host/network: DDS configuration completed; graph reachability is not confirmed",
        "ROS graph: not queried",
        "External processes were not started or stopped"});
      session_->setConnectionState(QString("Failed: %1").arg(error.what()));
    }
  }

  void disconnect_ros()
  {
    if (freedrive_ && freedrive_->busy()) {
      pending_disconnect_ = true;
      freedrive_->leave();
      return;
    }
    sim_base_jog_->stop("ROS disconnected");
    freedrive_->leave();
    freedrive_adapter_->setNode({});
    ur_arm_jog_->stop("ROS disconnected");
    lift_controller_->stop("ROS disconnected");
    mir_hardware_control_->stop("ROS disconnected");
    ros_spin_timer_.stop();
    diagnosis_timer_.stop();
    if (rviz_dock_) {
      removeDockWidget(rviz_dock_);
      delete rviz_dock_;
      rviz_dock_ = nullptr;
    }
    session_monitor_.reset();
    mocap_->setNode({});
    hardware_start_->setNode({});
    session_->setSimulationServerAvailable(false);
    log_store_->append("ROS /rosout", "WARN", "ROS session disconnected; live /rosout messages are unavailable.");
    if (ros_executor_ && rviz_node_) ros_executor_->remove_node(rviz_node_->get_raw_node());
    ros_executor_.reset();
    rviz_node_.reset();
    sim_base_jog_adapter_->setNode({});
    ur_arm_jog_adapter_->setNode({});
    lift_adapter_->setNode({});
    mir_hardware_adapter_->setNode({});
    goals_poses_adapter_->setNode({});
    goals_poses_->setConnected(false);
    pending_context_.clear();
    connected_context_.clear();
    if (rclcpp::ok()) rclcpp::shutdown();
    session_->setDiscoveryRoute(QString());
    session_->setDiagnostics({"Disconnected · external ROS launches and processes were left running"});
    session_->setConnectionState("Disconnected");
  }

protected:
  void closeEvent(QCloseEvent * event) override
  {
    if (shutdown_ && shutdown_->busy()) { event->ignore(); return; }
    if (freedrive_ && freedrive_->busy()) {
      pending_close_ = true;
      pending_disconnect_ = true;
      freedrive_->leave();
      event->ignore();
      return;
    }
    sim_base_jog_->stop("window closed");
    freedrive_->leave();
    ur_arm_jog_->stop("window closed");
    motion_actions_->cancel();
    lift_controller_->stop("window closed");
    mir_hardware_control_->stop("window closed");
    if (rviz_node_) disconnect_ros();
    event->accept();
  }

private:
  QString context_;
  QString config_override_;
  bool pending_disconnect_{false};
  bool pending_close_{false};
  QString pending_context_;
  QString connected_context_;
  SessionController * session_;
  std::shared_ptr<RosSimBaseJogAdapter> sim_base_jog_adapter_;
  SimulationBaseJog * sim_base_jog_;
  std::shared_ptr<RosMirHardwareAdapter> mir_hardware_adapter_;
  MirHardwareControl * mir_hardware_control_;
  std::shared_ptr<RosGoalsPosesAdapter> goals_poses_adapter_;
  GoalsPoses * goals_poses_;
  std::shared_ptr<SshHardwareStartAdapter> hardware_start_adapter_;
  HardwareStartController * hardware_start_{nullptr};
  MocapController * mocap_;
  std::shared_ptr<RosUrArmJogAdapter> ur_arm_jog_adapter_;
  UrArmJog * ur_arm_jog_;
  std::shared_ptr<SshUrShutdownAdapter> shutdown_adapter_;
  UrShutdown * shutdown_{nullptr};
  std::shared_ptr<SshUrMotionActionsAdapter> motion_adapter_;
  UrMotionActions * motion_actions_{nullptr};
  std::shared_ptr<RosUrFreedriveAdapter> freedrive_adapter_;
  UrFreedrive * freedrive_{nullptr};
  std::shared_ptr<RosLiftAdapter> lift_adapter_;
  LiftController * lift_controller_;
  ProcessManager * process_manager_{nullptr};
  LogStore * log_store_{nullptr};
  LogPanel * log_panel_{nullptr};
  std::shared_ptr<rviz_common::ros_integration::RosNodeAbstraction> rviz_node_;
  std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> ros_executor_;
  std::unique_ptr<RosSessionMonitor> session_monitor_;
  QDockWidget * rviz_dock_{nullptr};
  QDockWidget * logs_dock_{nullptr};
  QTimer ros_spin_timer_;
  QTimer diagnosis_timer_;
};

bool run_connection_error_layout_contract_test(const QString & qml_path, const QSize & desktop_size)
{
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  const auto missing_config = directory.filePath(QString(190, 'x') + ".rviz");
  SessionController fixture;
  if (!fixture.saveProfile("Missing RViz file", "Simulation", "localhost", "mur620d", "RViz",
    "", missing_config, 62)) return false;
  fixture.selectView("Overview");

  PrototypeWindow window("", "Layout check", qml_path, desktop_size);
  window.show();
  if (window.width() > desktop_size.width() || window.height() > desktop_size.height()) {
    qCritical("Startup window %dx%d exceeds desktop %dx%d", window.width(), window.height(),
      desktop_size.width(), desktop_size.height());
    return false;
  }
  auto * view = qobject_cast<QQuickWidget *>(window.centralWidget());
  if (!view || !view->rootObject()) return false;
  const auto unload = qScopeGuard([view]() { view->setSource(QUrl()); });
  auto * session = qobject_cast<SessionController *>(view->rootContext()->contextProperty("session").value<QObject *>());
  if (!session) return false;
  const QStringList pages{"Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings", "Mocap"};
  const auto find_item = [](auto & self, QQuickItem * item, const auto & matches) -> QQuickItem * {
    if (matches(item)) return item;
    for (auto * child : item->childItems()) {
      if (auto * found = self(self, child, matches)) return found;
    }
    return nullptr;
  };
  const auto find_button = [&](const QString & text) {
    return find_item(find_item, view->quickWindow()->contentItem(), [&](auto * item) {
      return item->isVisible() && item->property("text").toString() == text &&
        item->metaObject()->indexOfSignal("clicked()") >= 0;
    });
  };
  const auto fits = [&](QQuickItem * item) {
    auto * root = view->rootObject();
    return item && item->isVisible() && item->isEnabled() &&
      root->boundingRect().contains(item->mapRectToItem(root, item->boundingRect()));
  };
  const auto reach_button = [&](const QString & text) -> QQuickItem * {
    QCoreApplication::processEvents();
    auto * button = find_button(text);
    if (button && !fits(button)) {
      button->forceActiveFocus(Qt::TabFocusReason);
      QCoreApplication::processEvents();
    }
    return fits(button) ? button : nullptr;
  };
  const auto header_fits = [&]() {
    if (!fits(find_button("Connect")) || !fits(find_button("New preset"))) {
      qCritical("Connect or New preset is outside the visible window");
      return false;
    }
    for (const auto & page : pages) {
      if (!fits(find_button(page))) {
        qCritical("Navigation button '%s' is outside the visible window", qPrintable(page));
        return false;
      }
    }
    for (const auto * name : {"modeSelector", "presetSelector", "targetSelector", "robotSelector"}) {
      if (!fits(view->rootObject()->findChild<QQuickItem *>(name))) {
        qCritical("Session control '%s' is outside the visible window", name);
        return false;
      }
    }
    return true;
  };
  QCoreApplication::processEvents();
  if (!header_fits()) return false;
  auto * logs_dock = window.findChild<QDockWidget *>("logsDock");
  if (!logs_dock || (desktop_size.height() <= 540 && logs_dock->isVisible())) {
    qCritical("Unexpected startup log dock visibility");
    return false;
  }
  session->connectToTarget();
  if (!session->connectionState().startsWith("Failed:") ||
    !session->connectionState().contains(missing_config) || rclcpp::ok()) return false;
  QCoreApplication::processEvents();
  if (!header_fits()) return false;
  for (const auto & page : pages) {
    auto * button = find_button(page);
    if (!QMetaObject::invokeMethod(button, "clicked") || session->view() != page) {
      qCritical("Couldn't open '%s' from the navigation", qPrintable(page));
      return false;
    }
  }
  session->selectView("Logs");
  auto * open_logs = reach_button("Open log dock");
  if (!open_logs || !QMetaObject::invokeMethod(open_logs, "clicked")) {
    qCritical("Couldn't open the log dock from the Logs page");
    return false;
  }
  QCoreApplication::processEvents();
  if (auto * workspace = view->rootObject()->findChild<QQuickItem *>("workspaceViewport")) {
    workspace->setProperty("contentY", 0);
  }
  if (!logs_dock->isVisible() || logs_dock->height() > window.height() / 2 || !header_fits()) {
    qCritical("The log dock (%d px high) does not fit in the %d px window",
      logs_dock->height(), window.height());
    return false;
  }
  logs_dock->hide();
  QCoreApplication::processEvents();
  if (desktop_size.height() <= 540) {
    session->selectView("Mocap");
    QCoreApplication::processEvents();
    auto * page = find_item(find_item, view->rootObject(), [](auto * item) {
      return item->objectName() == "mocapPage";
    });
    auto * control = find_button("Freeze localization");
    if (!page || !control || page->property("contentHeight").toReal() <= page->height()) {
      qCritical("Expected scrollable Mocap controls on the compact desktop");
      return false;
    }
    const auto button_center = control->mapRectToItem(page, control->boundingRect()).center().y();
    const auto max_scroll = page->property("contentHeight").toReal() - page->height();
    page->setProperty("contentY", qBound(0.0, button_center - page->height() / 2, max_scroll));
    QCoreApplication::processEvents();
    if (!page->boundingRect().contains(control->mapRectToItem(page, control->boundingRect()))) {
      qCritical("The Mocap control cannot be reached by scrolling");
      return false;
    }
  }
  session->selectView("Settings");
  auto * edit = reach_button("Edit profile");
  if (!edit || !QMetaObject::invokeMethod(edit, "clicked")) {
    qCritical("Couldn't open Edit profile from Settings");
    return false;
  }
  QCoreApplication::processEvents();
  if (!fits(find_button("Save profile")) || !fits(find_button("Cancel"))) {
    qCritical("Profile dialog actions are outside the visible window");
    return false;
  }
  return QMetaObject::invokeMethod(find_button("Cancel"), "clicked");
}
}

int main(int argc, char ** argv)
{
  QApplication app(argc, argv);
  app.setApplicationName("MuR RViz Docking Prototype");

  QString config_path;
  QString qml_path;
  QString context = "Simulation · local · domain " +
    qEnvironmentVariable("ROS_DOMAIN_ID", "unset");
  const auto arguments = app.arguments();
  for (int i = 1; i < arguments.size(); ++i) {
    if (arguments[i] == "--config" && i + 1 < arguments.size()) {
      config_path = arguments[++i];
    } else if (arguments[i] == "--qml" && i + 1 < arguments.size()) {
      qml_path = arguments[++i];
    } else if (arguments[i] == "--contract-test-session") {
      // Exercised below before the GUI/ROS host is created.
    } else if (arguments[i] == "--contract-test-ros-session-monitor") {
      // Exercises the public monitor output against a live in-process ROS graph.
    } else if (arguments[i] == "--contract-test-hardware-status") {
      // Exercises missing, current, and stale telemetry through an injected clock and message seam.
    } else if (arguments[i] == "--contract-test-lift-controller") {
      // Exercises height freshness, selected-side routing, bounds, and lift stop through a fake adapter.
    } else if (arguments[i] == "--contract-test-hardware-start") {
      // Exercises safety-gated hardware start, profile persistence, and explicit sync/build actions via a fake adapter.
    } else if (arguments[i] == "--contract-test-localization-graph") {
      // Exercises live localization source and data through a local ROS graph.
    } else if (arguments[i] == "--contract-test-localization-source") {
      // Exercises persistent source selection, availability gating and next-start routing.
    } else if (arguments[i] == "--contract-test-portable-presets") {
      // Exercises portable preset serialization through the session interface.
    } else if (arguments[i] == "--contract-test-gazebo-preset") {
      // Verifies the persistent per-profile Gazebo auto-open preference.
    } else if (arguments[i] == "--contract-test-rviz-session") {
      // Checks active-profile RViz selection, time source, and unavailable profile errors.
    } else if (arguments[i] == "--contract-test-connection-error-layout") {
      // Checks that a failed Connect leaves the navigation reachable.
    } else if (arguments[i] == "--contract-test-sim-base-jog") {
      // Exercises hold-to-run behavior against an isolated fake ROS adapter.
    } else if (arguments[i] == "--contract-test-mir-hardware-control") {
      // Exercises hardware jog and MiR light actions through a fake ROS adapter.
    } else if (arguments[i] == "--contract-test-goals-poses") {
      // Exercises pose CRUD, stale-pose rejection, and selected-robot routing through a fake adapter.
    } else if (arguments[i] == "--contract-test-mocap") {
      // Exercises Qualisys status, map output, freeze/resume, and external bridge ownership with a local ROS graph.
    } else if (arguments[i] == "--contract-test-ur-arm-jog") {
      // Exercises UR readiness gating and hold-to-run arm motion through an injected fake adapter.
    } else if (arguments[i] == "--contract-test-ur-shutdown") {
      continue;
    } else if (arguments[i] == "--contract-test-ur-freedrive") {
      // Exercises freedrive through a fake adapter.
    } else if (arguments[i] == "--contract-test-ur-motion-actions") {
      // Exercises selected-arm motion actions with a fake adapter.
    } else if (arguments[i] == "--contract-test-remote-discovery") {
      // Exercises DDS environment setup without initializing ROS.
    } else if (arguments[i] == "--contract-test-process-manager") {
      // Exercises process ownership and captured output through a fake executable.
    } else if (arguments[i] == "--contract-test-remote-process-manager") {
      // Exercises SSH ownership handshakes and exact-run stop through a fake ssh executable.
    } else if (arguments[i] == "--contract-test-log-store") {
      // Exercises filtering, exports, redaction, and process-to-log source selection.
    } else if (arguments[i] == "--context" && i + 1 < arguments.size()) {
      context = arguments[++i];
    } else if (arguments[i] == "--help" || arguments[i] == "-h") {
      qInfo("Usage: mur_rviz_docking_prototype [--config FILE.rviz] [--context LABEL] [--qml FILE.qml]");
      return 0;
    } else {
      qCritical("Unknown or incomplete argument: %s", qPrintable(arguments[i]));
      return 2;
    }
  }
  if (!config_path.isEmpty() && !QFileInfo::exists(config_path)) {
    qCritical("RViz configuration does not exist: %s", qPrintable(config_path));
    return 2;
  }
  if (qml_path.isEmpty()) {
    qml_path = QString::fromLocal8Bit(std::getenv("MUR_COMMAND_CENTER_QML"));
  }
  if (qml_path.isEmpty()) {
    try {
      qml_path = QString::fromStdString(
        ament_index_cpp::get_package_share_directory("match_mur_gui") + "/prototype/qml/Main.qml");
    } catch (const std::exception &) {
      // Keep --qml available for local builds before the package is installed.
    }
  }
  if (qml_path.isEmpty() || !QFileInfo::exists(qml_path)) {
    qCritical("QML UI path is missing; build/install match_mur_gui or pass --qml FILE.qml");
    return 2;
  }

  app.setOrganizationName("MATCH");
  app.setApplicationName("MuRCommandCenter");
  std::unique_ptr<QTemporaryDir> contract_config;
  if (!arguments.filter(QRegularExpression("^--contract-test-")).isEmpty()) {
    contract_config = std::make_unique<QTemporaryDir>();
    if (!contract_config->isValid() ||
      !qputenv("XDG_CONFIG_HOME", contract_config->path().toLocal8Bit()) ||
      !qputenv("XDG_DATA_HOME", contract_config->filePath("data").toLocal8Bit()) ||
      !qputenv("MUR_COMMAND_CENTER_SETTINGS", contract_config->filePath("session.ini").toLocal8Bit())) {
      qCritical("Could not isolate command center contract test settings");
      return 1;
    }
  }
  if (arguments.contains("--contract-test-session")) {
    if (!run_session_contract_test()) {
      qCritical("Session contract failed");
      return 1;
    }
    qInfo("Session contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-ros-session-monitor")) {
    if (!run_ros_session_monitor_contract_test()) {
      qCritical("ROS session monitor contract failed");
      return 1;
    }
    qInfo("ROS session monitor contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-hardware-status")) {
    if (!run_hardware_status_contract_test()) {
      qCritical("Hardware status contract failed");
      return 1;
    }
    qInfo("Hardware status contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-lift-controller")) {
    if (!run_lift_controller_contract_test()) {
      qCritical("Lift controller contract failed");
      return 1;
    }
    qInfo("Lift controller contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-hardware-start")) {
    if (!run_hardware_start_contract_test()) {
      qCritical("Hardware start contract failed");
      return 1;
    }
    qInfo("Hardware start contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-localization-graph")) {
    return run_localization_graph_contract_test() ? 0 : 1;
  }
  if (arguments.contains("--contract-test-localization-source")) {
    if (!run_localization_selection_contract_test()) {
      qCritical("Localization source selection contract failed");
      return 1;
    }
    qInfo("Localization source selection contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-portable-presets")) {
    if (!run_portable_preset_contract_test()) {
      qCritical("Portable preset contract failed");
      return 1;
    }
    qInfo("Portable preset contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-gazebo-preset")) {
    if (!run_gazebo_preset_contract_test()) {
      qCritical("Gazebo preset contract failed");
      return 1;
    }
    qInfo("Gazebo preset contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-rviz-session")) {
    if (!run_rviz_session_contract_test()) {
      qCritical("RViz session contract failed");
      return 1;
    }
    qInfo("RViz session contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-connection-error-layout")) {
    // 1080p work areas with space reserved for the taskbar, at 100% and 200% scaling.
    return run_connection_error_layout_contract_test(qml_path, QSize(1920, 1040)) &&
      run_connection_error_layout_contract_test(qml_path, QSize(960, 500)) ? 0 : 1;
  }
  if (arguments.contains("--contract-test-sim-base-jog")) {
    if (!run_sim_base_jog_contract_test()) {
      qCritical("Simulation base jog contract failed");
      return 1;
    }
    qInfo("Simulation base jog contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-ur-arm-jog")) {
    if (!run_ur_arm_jog_contract_test()) {
      qCritical("UR arm jog contract failed");
      return 1;
    }
    qInfo("UR arm jog contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-ur-shutdown")) {
    return run_ur_shutdown_contract_test() ? 0 : 1;
  }
  if (arguments.contains("--contract-test-ur-freedrive")) return run_ur_freedrive_contract_test() ? 0 : 1;
  if (arguments.contains("--contract-test-ur-motion-actions")) {
    if (!run_ur_motion_actions_contract_test()) {
      qCritical("UR motion actions contract failed");
      return 1;
    }
    qInfo("UR motion actions contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-mir-hardware-control")) {
    if (!run_mir_hardware_control_contract_test()) {
      qCritical("MiR hardware control contract failed");
      return 1;
    }
    qInfo("MiR hardware control contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-goals-poses")) {
    if (!run_goals_poses_contract_test()) {
      qCritical("Goals and poses contract failed");
      return 1;
    }
    qInfo("Goals and poses contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-mocap")) {
    if (!run_mocap_controller_contract_test()) {
      qCritical("Mocap controller contract failed");
      return 1;
    }
    qInfo("Mocap controller contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-remote-discovery")) {
    if (!run_remote_discovery_contract_test()) {
      qCritical("Remote discovery contract failed");
      return 1;
    }
    qInfo("Remote discovery contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-process-manager")) {
    if (!run_process_manager_contract_test()) {
      qCritical("Process manager contract failed");
      return 1;
    }
    qInfo("Process manager contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-remote-process-manager")) {
    if (!run_remote_process_manager_contract_test()) {
      qCritical("Remote process manager contract failed");
      return 1;
    }
    qInfo("Remote process manager contract passed");
    return 0;
  }
  if (arguments.contains("--contract-test-log-store")) {
    if (!run_log_store_contract_test()) {
      qCritical("Log store contract failed");
      return 1;
    }
    qInfo("Log store contract passed");
    return 0;
  }
  auto * screen = app.screenAt(QCursor::pos());
  if (!screen) screen = app.primaryScreen();
  const auto desktop = screen->availableGeometry();
  PrototypeWindow window(config_path, context, qml_path, desktop.size());
  window.move(desktop.center() - QPoint(window.width() / 2, window.height() / 2));
  window.show();
  const int result = app.exec();
  if (rclcpp::ok()) rclcpp::shutdown();
  return result;
}

#include "rviz_docking_main.moc"
