#include <memory>
#include <string>
#include <stdexcept>

#include <QApplication>
#include <QDockWidget>
#include <QFileInfo>
#include <QLabel>
#include <QMainWindow>
#include <QQmlContext>
#include <QQuickWidget>
#include <QShortcut>
#include <QSettings>
#include <QStringList>
#include <QStatusBar>
#include <QTimer>
#include <cstdlib>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"
#include "rviz_common/ros_integration/ros_node_abstraction.hpp"
#include "rviz_common/visualization_frame.hpp"

namespace
{
class SessionController : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString preset READ preset NOTIFY changed)
  Q_PROPERTY(QString target READ target NOTIFY changed)
  Q_PROPERTY(QString robot READ robot NOTIFY changed)
  Q_PROPERTY(QString mode READ mode NOTIFY changed)
  Q_PROPERTY(QString view READ view NOTIFY changed)
  Q_PROPERTY(QString connectionState READ connectionState NOTIFY changed)
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
  QString robot() const { return value("robot", "mur620d"); }
  QString mode() const { return value("mode", preset() == "Local simulation" ? "Simulation" : "Hardware"); }
  QString view() const { return value("view", "Overview"); }
  QString connectionState() const { return connection_state_; }
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
  void selectView(const QString &view)
  {
    static const QStringList valid = {"Overview", "Visualization", "Robot", "Processes", "Diagnosis", "Logs", "Settings"};
    if (valid.contains(view)) setValue("view", view);
  }
  void connectToTarget() { emit connectRequested(); }
  void setConnectionState(const QString &state) { connection_state_ = state; emit changed(); }

signals:
  void changed();
  void connectRequested();

private:
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
};

bool run_session_contract_test()
{
  {
    SessionController session;
    if (session.preset() != "Local simulation" || session.mode() != "Simulation" ||
      session.view() != "Overview" || session.target() != "localhost" ||
      session.robot() != "mur620d" || session.connectionState() != "Disconnected") return false;
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
      rclcpp::ok()) return false;
    restored.selectPreset("Local simulation");
    if (restored.view() != "Visualization" || restored.mode() != "Hardware" ||
      restored.target() != "sim-host" || restored.robot() != "mur620b") return false;
  }
  return !rclcpp::ok();
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
    command_center->setSource(QUrl::fromLocalFile(qml_path));
    setCentralWidget(command_center);
    connect(&ros_spin_timer_, &QTimer::timeout, this, &PrototypeWindow::spin_ros);
    connect(session_, &SessionController::connectRequested, this, [this]() { connect_ros(); });
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
      session_->setConnectionState("Connected");
    } catch (const std::exception & error) {
      session_->setConnectionState(QString("Failed: %1").arg(error.what()));
    }
  }

private:
  QString context_;
  QString config_path_;
  SessionController * session_;
  std::shared_ptr<rviz_common::ros_integration::RosNodeAbstraction> rviz_node_;
  std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> ros_executor_;
  QDockWidget * rviz_dock_{nullptr};
  QTimer ros_spin_timer_;
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
  PrototypeWindow window(config_path, context, qml_path);
  window.show();
  const int result = app.exec();
  if (rclcpp::ok()) rclcpp::shutdown();
  return result;
}

#include "rviz_docking_main.moc"
