#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>

#include <QApplication>
#include <QDockWidget>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QLabel>
#include <QMainWindow>
#include <QQmlContext>
#include <QQuickWidget>
#include <QShortcut>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QTemporaryDir>
#include <QStringList>
#include <QStatusBar>
#include <QTimer>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <cstdlib>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"
#include "rviz_common/ros_integration/ros_node_abstraction.hpp"
#include "rviz_common/visualization_frame.hpp"
#include "ros_session_monitor.hpp"
#include "process_manager.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{
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
  int rosDomain() const { return qEnvironmentVariableIntValue("ROS_DOMAIN_ID"); }
  QString robot() const { return value("robot", "mur620d"); }
  QString mode() const { return value("mode", preset() == "Local simulation" ? "Simulation" : "Hardware"); }
  QString visualization() const { return value("visualization", "RViz"); }
  QString workspacePath() const { return value("workspacePath", ""); }
  QString configPath() const { return value("configPath", ""); }
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
    if (!settings_->contains(group + "robot")) settings_->setValue(group + "robot", "mur620d");
    if (!settings_->contains(group + "visualization")) settings_->setValue(group + "visualization", "RViz");
    if (!settings_->contains(group + "workspacePath")) settings_->setValue(group + "workspacePath", "");
    if (!settings_->contains(group + "configPath")) settings_->setValue(group + "configPath", "");
    if (!settings_->contains(group + "mode")) {
      settings_->setValue(group + "mode", clean == "Local simulation" ? "Simulation" : "Hardware");
    }
    settings_->setValue("activePreset", clean);
    settings_->sync();
    emit changed();
  }
  void selectTarget(const QString &target) { setValue("target", target); }
  void selectRobot(const QString &robot) { setValue("robot", robot); }
  void selectMode(const QString &mode)
  {
    if (mode == "Simulation" || mode == "Hardware") setValue("mode", mode);
  }
  Q_INVOKABLE bool saveProfile(const QString &name, const QString &mode, const QString &target,
    const QString &robot, const QString &visualization, const QString &workspace_path,
    const QString &config_path)
  {
    const auto clean_name = name.trimmed();
    if (clean_name.isEmpty() || clean_name.contains('/') || clean_name.contains('\\') ||
      (mode != "Simulation" && mode != "Hardware") || target.trimmed().isEmpty() ||
      robot.trimmed().isEmpty() || (visualization != "RViz" && visualization != "MuR data view")) {
      setError("Profile needs a name, Simulation or Hardware mode, target, robot, and a supported visualization.");
      return false;
    }
    const auto group = QString("presets/%1/").arg(clean_name);
    settings_->setValue(group + "mode", mode);
    settings_->setValue(group + "target", target.trimmed());
    settings_->setValue(group + "robot", robot.trimmed());
    settings_->setValue(group + "visualization", visualization);
    settings_->setValue(group + "workspacePath", workspace_path.trimmed());
    settings_->setValue(group + "configPath", config_path.trimmed());
    settings_->setValue("activePreset", clean_name);
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
    profile.insert("robot", robot());
    profile.insert("visualization", visualization());
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
    const auto robot = profile.value("robot").toString().trimmed();
    const auto visualization = profile.value("visualization").toString();
    if (bundle.value("format").toString() != "mur-command-center-preset" ||
      bundle.value("version").toInt(-1) != 1 || name.isEmpty() ||
      (mode != "Simulation" && mode != "Hardware") || target.isEmpty() || robot.isEmpty() ||
      (visualization != "RViz" && visualization != "MuR data view") ||
      !profile.value("requiresWorkspaceMapping").isBool() ||
      !profile.value("requiresConfigMapping").isBool()) {
      setError("Unsupported or invalid preset bundle. Expected version 1 with a complete profile.");
      return false;
    }
    // Bundles contain only mapping-required flags; machine-local absolute paths never leave this host.
    const auto workspace = profile.value("requiresWorkspaceMapping").toBool() ? workspace_mapping.trimmed() : QString();
    const auto config = profile.value("requiresConfigMapping").toBool() ? config_mapping.trimmed() : QString();
    return saveProfile(name, mode, target, robot, visualization, workspace, config);
  }
  void selectView(const QString &view)
  {
    static const QStringList valid = {"Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings"};
    if (valid.contains(view)) setValue("view", view);
  }
  void connectToTarget() { if (connection_state_ == "Disconnected" || connection_state_.startsWith("Failed")) emit connectRequested(); }
  void disconnectFromTarget() { emit disconnectRequested(); }
  void setDiagnostics(const QStringList &diagnostics) { diagnostics_ = diagnostics; emit changed(); }
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
  QStringList diagnostics_{"Not connected · choose Connect to inspect the existing ROS graph"};
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
  auto wrong_type = fixture->create_publisher<std_msgs::msg::String>(
    "/mur620d/mobile_base_controller/cmd_vel", 10);
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
    executor.spin_some();
    const auto hardware = monitor.diagnose("localhost", "mur620d", "Hardware", 0);
    if (hardware.join("\n").contains("drive_controller: active")) {
      const auto simulation = monitor.diagnose("localhost", "mur620d", "Simulation", 0).join("\n");
      passed = hardware.join("\n").contains("remote node(s) discovered") &&
        hardware.join("\n").contains("wrong type on /mur620d/mobile_base_controller/cmd_vel") &&
        hardware.join("\n").contains("Clock: not required in Hardware mode") &&
        simulation.contains("Clock: missing (/clock has no publisher)");
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  executor.remove_node(observer);
  executor.remove_node(fixture);
  rclcpp::shutdown();
  return passed;
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
      "MuR data view", "/old/workspace", "/old/config.rviz")) return false;
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
      profile.value("mode").toString() != "Hardware" ||
      profile.value("visualization").toString() != "MuR data view" ||
      profile.contains("view") || profile.contains("lastView") || profile.contains("logFilter") ||
      profile.contains("workspacePath") || profile.contains("configPath") ||
      !profile.value("requiresWorkspaceMapping").toBool() ||
      !profile.value("requiresConfigMapping").toBool() ||
      exported_bytes.contains("/old/workspace") || exported_bytes.contains("/old/config.rviz") ||
      profile.contains("password") || bundle.contains("credentials")) return false;
    session.saveProfile("Temporary", "Simulation", "localhost", "mur620d", "RViz", "", "");
    if (!session.importPreset(bundle_path, "/new/workspace", "/new/config.rviz") ||
      session.preset() != "Portable lab" || session.mode() != "Hardware" ||
      session.target() != "lab-host" || session.robot() != "mur620b" ||
      session.visualization() != "MuR data view" || session.workspacePath() != "/new/workspace" ||
      session.configPath() != "/new/config.rviz") return false;
    if (!session.importPreset(bundle_path, "", "") || !session.workspacePath().isEmpty() ||
      !session.configPath().isEmpty()) return false;
    if (session.importPreset(directory.filePath("missing.json"), "", "") || session.lastError().isEmpty()) return false;
  }
  QFile invalid(invalid_path);
  if (!invalid.open(QIODevice::WriteOnly)) return false;
  invalid.write("{\"format\":\"mur-command-center-preset\",\"version\":99}");
  invalid.close();
  SessionController restored;
  return !restored.importPreset(invalid_path, "", "") &&
    restored.lastError().contains("Unsupported or invalid");
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
              return manager.log().contains(
                "mur_launch_sim', 'mur620.launch.py', 'robot_name:=mur620d', "
                "'include_gz:=true', 'gazebo_gui:=false', 'launch_rviz:=false'");
            }
          }
        }
        return false;
      }
    }
  }
  return false;
}

class PrototypeWindow : public QMainWindow
{
public:
  PrototypeWindow(const QString & config_path, const QString & context, const QString & qml_path)
  : QMainWindow(), context_(context), config_path_(config_path), session_(new SessionController())
  {
    setWindowTitle(QString("MuR Command Center · %1").arg(context_));
    resize(1440, 900);
    setMinimumSize(840, 560);

    auto * command_center = new QQuickWidget(this);
    command_center->setResizeMode(QQuickWidget::SizeRootObjectToView);
    command_center->rootContext()->setContextProperty("session", session_);
    process_manager_ = new ProcessManager(this);
    command_center->rootContext()->setContextProperty("processes", process_manager_);
    command_center->setSource(QUrl::fromLocalFile(qml_path));
    setCentralWidget(command_center);
    connect(&ros_spin_timer_, &QTimer::timeout, this, &PrototypeWindow::spin_ros);
    ros_spin_timer_.setInterval(10);
    connect(&diagnosis_timer_, &QTimer::timeout, this, &PrototypeWindow::refresh_diagnosis);
    connect(session_, &SessionController::connectRequested, this, [this]() { connect_ros(); });
    connect(session_, &SessionController::disconnectRequested, this, [this]() { disconnect_ros(); });
    for (int index = 0; index < 7; ++index) {
      auto * shortcut = new QShortcut(QKeySequence(QString::number(index + 1)), this);
      shortcut->setContext(Qt::WidgetWithChildrenShortcut);
      connect(shortcut, &QShortcut::activated, this, [this, index]() {
        static const QStringList views = {"Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings"};
        session_->selectView(views[index]);
      });
    }
    connect(session_, &SessionController::changed, this, [this]() {
      setWindowTitle(QString("MuR Command Center · %1 · %2 · %3")
        .arg(session_->mode(), session_->preset(), session_->robot()));
      statusBar()->showMessage(QString("%1 · %2 · %3 · %4 · No processes started")
        .arg(session_->target(), session_->robot(), session_->mode(), session_->connectionState()));
    });

    auto * log_view = new QLabel(
      "Prototype log view\n\n"
      "This dock is independent of RViz. Replace it with the session log view\n"
      "when the host shell is integrated into the MuR application.");
    log_view->setWordWrap(true);
    auto * logs = new QDockWidget("Logs · " + context_, this);
    logs->setObjectName("logsDock");
    logs->setWidget(log_view);
    logs->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::BottomDockWidgetArea, logs);

    statusBar()->showMessage(QString("%1 · %2 · Disconnected · No processes started")
      .arg(session_->target(), session_->robot()));
  }

  void spin_ros()
  {
    if (ros_executor_) ros_executor_->spin_some();
  }

  void refresh_diagnosis()
  {
    if (session_monitor_) {
      session_->setDiagnostics(session_monitor_->diagnose(session_->target(), session_->robot(),
        session_->mode(), session_->rosDomain()));
    }
  }

  void connect_ros()
  {
    if (rviz_node_) return;
    session_->setConnectionState("Connecting");
    try {
      if (!rclcpp::ok()) rclcpp::init(0, nullptr);
      ros_executor_ = std::make_unique<rclcpp::executors::SingleThreadedExecutor>();
      rviz_node_ = std::make_shared<rviz_common::ros_integration::RosNodeAbstraction>(
        "mur_rviz_docking_prototype");
      ros_executor_->add_node(rviz_node_->get_raw_node());
      session_monitor_ = std::make_unique<RosSessionMonitor>(rviz_node_->get_raw_node());
      auto * frame = new rviz_common::VisualizationFrame(rviz_node_);
      frame->setApp(qApp);
      frame->setSplashPath("");
      frame->setHideButtonVisibility(false);
      frame->initialize(rviz_node_);
      rviz_dock_ = new QDockWidget("Visualisierung · " + context_, this);
      rviz_dock_->setObjectName("rvizVisualizationDock");
      rviz_dock_->setWidget(frame);
      rviz_dock_->setAllowedAreas(Qt::AllDockWidgetAreas);
      rviz_dock_->setFeatures(QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
      addDockWidget(Qt::RightDockWidgetArea, rviz_dock_);
      connect(rviz_dock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) rviz_dock_->widget()->setFocus(Qt::OtherFocusReason);
      });
      if (!config_path_.isEmpty()) frame->loadDisplayConfig(config_path_);
      ros_spin_timer_.start(10);
      diagnosis_timer_.start(500);
      session_->setConnectionState("Connected");
      session_->setDiagnostics(session_monitor_->diagnose(session_->target(), session_->robot(),
        session_->mode(), session_->rosDomain()));
    } catch (const std::exception & error) {
      session_monitor_.reset();
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
      if (rclcpp::ok()) rclcpp::shutdown();
      session_->setDiagnostics({QString("Connection failed · %1").arg(error.what()),
        "External processes were not started or stopped"});
      session_->setConnectionState(QString("Failed: %1").arg(error.what()));
    }
  }

  void disconnect_ros()
  {
    ros_spin_timer_.stop();
    diagnosis_timer_.stop();
    if (rviz_dock_) {
      removeDockWidget(rviz_dock_);
      delete rviz_dock_;
      rviz_dock_ = nullptr;
    }
    session_monitor_.reset();
    if (ros_executor_ && rviz_node_) ros_executor_->remove_node(rviz_node_->get_raw_node());
    ros_executor_.reset();
    rviz_node_.reset();
    if (rclcpp::ok()) rclcpp::shutdown();
    session_->setDiagnostics({"Disconnected · external ROS launches and processes were left running"});
    session_->setConnectionState("Disconnected");
  }

private:
  QString context_;
  QString config_path_;
  SessionController * session_;
  ProcessManager * process_manager_{nullptr};
  std::shared_ptr<rviz_common::ros_integration::RosNodeAbstraction> rviz_node_;
  std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> ros_executor_;
  std::unique_ptr<RosSessionMonitor> session_monitor_;
  QDockWidget * rviz_dock_{nullptr};
  QTimer ros_spin_timer_;
  QTimer diagnosis_timer_;
};
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
    } else if (arguments[i] == "--contract-test-portable-presets") {
      // Exercises portable preset serialization through the session interface.
    } else if (arguments[i] == "--contract-test-process-manager") {
      // Exercises process ownership and captured output through a fake executable.
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
  if (arguments.contains("--contract-test-portable-presets")) {
    if (!run_portable_preset_contract_test()) {
      qCritical("Portable preset contract failed");
      return 1;
    }
    qInfo("Portable preset contract passed");
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
  PrototypeWindow window(config_path, context, qml_path);
  window.show();
  const int result = app.exec();
  if (rclcpp::ok()) rclcpp::shutdown();
  return result;
}

#include "rviz_docking_main.moc"
