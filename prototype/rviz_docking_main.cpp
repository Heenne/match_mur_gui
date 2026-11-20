#include <memory>
#include <string>

#include <QApplication>
#include <QDockWidget>
#include <QFileInfo>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QTimer>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"
#include "rviz_common/ros_integration/ros_node_abstraction.hpp"
#include "rviz_common/visualization_frame.hpp"

namespace
{
class PrototypeWindow : public QMainWindow
{
public:
  PrototypeWindow(const QString & config_path, const QString & context)
  : QMainWindow(), context_(context)
  {
    setWindowTitle(QString("MuR · RViz docking prototype · %1").arg(context_));
    resize(1440, 900);

    auto * introduction = new QLabel(
      "Native RViz VisualizationFrame embedded in the Visualisierung dock.\n"
      "Use the dock title bar to float it, then dock it again.\n"
      "The diagnostic log dock remains alongside RViz.");
    introduction->setAlignment(Qt::AlignCenter);
    setCentralWidget(introduction);

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

    rviz_node_ = std::make_shared<rviz_common::ros_integration::RosNodeAbstraction>(
      "mur_rviz_docking_prototype");
    ros_executor_.add_node(rviz_node_->get_raw_node());
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
      if (visible) {
        rviz_dock_->widget()->setFocus(Qt::OtherFocusReason);
      }
    });

    if (!config_path.isEmpty()) {
      frame->loadDisplayConfig(config_path);
    }
    statusBar()->showMessage(
      QString("Context: %1 | ROS_DOMAIN_ID: %2 | RViz dock is attached")
        .arg(context_, qEnvironmentVariable("ROS_DOMAIN_ID", "unset")));
  }

  void spin_ros()
  {
    ros_executor_.spin_some();
  }

private:
  QString context_;
  std::shared_ptr<rviz_common::ros_integration::RosNodeAbstraction> rviz_node_;
  rclcpp::executors::SingleThreadedExecutor ros_executor_;
  QDockWidget * rviz_dock_{nullptr};
};
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  QApplication app(argc, argv);
  app.setApplicationName("MuR RViz Docking Prototype");

  QString config_path;
  QString context = "Simulation · local · domain " +
    qEnvironmentVariable("ROS_DOMAIN_ID", "unset");
  const auto arguments = app.arguments();
  for (int i = 1; i < arguments.size(); ++i) {
    if (arguments[i] == "--config" && i + 1 < arguments.size()) {
      config_path = arguments[++i];
    } else if (arguments[i] == "--context" && i + 1 < arguments.size()) {
      context = arguments[++i];
    } else if (arguments[i] == "--help" || arguments[i] == "-h") {
      qInfo("Usage: mur_rviz_docking_prototype [--config FILE.rviz] [--context LABEL]");
      rclcpp::shutdown();
      return 0;
    } else {
      qCritical("Unknown or incomplete argument: %s", qPrintable(arguments[i]));
      rclcpp::shutdown();
      return 2;
    }
  }
  if (!config_path.isEmpty() && !QFileInfo::exists(config_path)) {
    qCritical("RViz configuration does not exist: %s", qPrintable(config_path));
    rclcpp::shutdown();
    return 2;
  }

  PrototypeWindow window(config_path, context);
  window.show();
  QTimer ros_spin_timer;
  QObject::connect(&ros_spin_timer, &QTimer::timeout, [&window]() {
    window.spin_ros();
  });
  ros_spin_timer.start(10);
  const int result = app.exec();
  rclcpp::shutdown();
  return result;
}
