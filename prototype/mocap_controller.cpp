#include "mocap_controller.hpp"

#include "process_manager.hpp"

#include <algorithm>
#include <cmath>

#include <QRegularExpression>
#include <QSettings>

namespace
{
const QString bridge_process_name = "Qualisys Mocap bridge";

bool valid_host(const QString & host)
{
  static const QRegularExpression pattern("^[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?$");
  return host == "localhost" || pattern.match(host).hasMatch();
}

double yaw_from_quaternion(const geometry_msgs::msg::Quaternion & orientation)
{
  const double sin_yaw = 2.0 * (orientation.w * orientation.z + orientation.x * orientation.y);
  const double cos_yaw = 1.0 - 2.0 * (orientation.y * orientation.y + orientation.z * orientation.z);
  return std::atan2(sin_yaw, cos_yaw) * 180.0 / M_PI;
}

std::unique_ptr<QSettings> application_settings()
{
  const auto test_path = qEnvironmentVariable("MUR_COMMAND_CENTER_SETTINGS");
  if (!test_path.isEmpty()) {
    return std::make_unique<QSettings>(test_path, QSettings::IniFormat);
  }
  return std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
    "MATCH", "MuRCommandCenter");
}
}

MocapController::MocapController(QObject * parent) : QObject(parent)
{
  auto settings = application_settings();
  ssh_host_ = settings->value("mocap/sshHost", ssh_host_).toString();
  qtm_host_ = settings->value("mocap/qtmHost", qtm_host_).toString();
  qtm_port_ = settings->value("mocap/qtmPort", qtm_port_).toInt();
  refresh_timer_.setInterval(250);
  connect(&refresh_timer_, &QTimer::timeout, this, &MocapController::refresh);
  refresh_timer_.start();
}

QString MocapController::pose() const
{
  return pose_text_;
}

void MocapController::setNode(rclcpp::Node::SharedPtr node)
{
  node_ = std::move(node);
  resetSubscriptions();
  if (!node_) {
    have_pose_ = false;
    have_map_pose_ = false;
    have_map_tf_ = false;
    pose_text_ = "No pose received";
    pose_frame_.clear();
    data_age_seconds_ = -1.0;
    frequency_hz_ = 0.0;
    pose_status_ = "Disconnected · connect to inspect Qualisys";
    map_status_ = "Not connected";
    freeze_status_ = "Not connected";
    frozen_ = false;
    emit changed();
    return;
  }
  pose_status_ = "Waiting for /qualisys/" + robot_ + "/pose";
  map_status_ = "Waiting for map output";
  freeze_status_ = "Waiting for freeze status";
  resetSubscriptions();
  emit changed();
}

void MocapController::setProcessManager(ProcessManager * manager)
{
  process_manager_ = manager;
  updateProcessState();
}

void MocapController::setRobot(const QString & robot)
{
  static const QRegularExpression valid_robot("^[A-Za-z][A-Za-z0-9_]*$");
  if (!valid_robot.match(robot).hasMatch() || robot_ == robot) return;
  robot_ = robot;
  have_pose_ = false;
  have_map_pose_ = false;
  have_map_tf_ = false;
  pose_text_ = "No pose received";
  pose_frame_.clear();
  data_age_seconds_ = -1.0;
  frequency_hz_ = 0.0;
  map_status_ = node_ ? "Waiting for map output" : "Not connected";
  freeze_status_ = node_ ? "Waiting for freeze status" : "Not connected";
  frozen_ = false;
  pose_intervals_.clear();
  resetSubscriptions();
  emit changed();
}

void MocapController::setBridgeSettings(const QString & ssh_host, const QString & qtm_host, int qtm_port)
{
  const auto ssh = ssh_host.trimmed();
  const auto qtm = qtm_host.trimmed();
  if (!valid_host(ssh) || !valid_host(qtm) || qtm_port < 1 || qtm_port > 65535) return;
  ssh_host_ = ssh;
  qtm_host_ = qtm;
  qtm_port_ = qtm_port;
  auto settings = application_settings();
  settings->setValue("mocap/sshHost", ssh_host_);
  settings->setValue("mocap/qtmHost", qtm_host_);
  settings->setValue("mocap/qtmPort", qtm_port_);
  settings->sync();
  emit changed();
}

bool MocapController::startBridge()
{
  if (!node_ || !process_manager_) {
    bridge_state_ = "Connect to the ROS graph before starting the bridge";
    emit changed();
    return false;
  }
  updateProcessState();
  if (bridge_state_ == "Running · external bridge" || bridge_state_ == "Starting · app-owned bridge" ||
    bridge_state_ == "Running · app-owned bridge") return false;
  if (!valid_host(ssh_host_) || !valid_host(qtm_host_) || qtm_port_ < 1 || qtm_port_ > 65535) {
    bridge_state_ = "Invalid SSH/QTM host or port";
    emit changed();
    return false;
  }
  const QStringList args{
    "run", "match_mocap_ros2", "qualisys_ssh_bridge", "--ros-args",
    "-p", "ssh_host:=" + ssh_host_, "-p", "qtm_host:=" + qtm_host_,
    "-p", "qtm_port:=" + QString::number(qtm_port_),
    "-p", "publish_map_pose:=true", "-p", "publish_robot_tf:=true"};
  const bool started = process_manager_->startProcess(bridge_process_name, "ros2", args);
  if (!started) {
    updateProcessState();
  } else {
    bridge_state_ = "Starting · app-owned bridge";
  }
  emit changed();
  return started;
}

void MocapController::stopBridge()
{
  if (!process_manager_) return;
  updateProcessState();
  if (bridge_state_ == "Running · app-owned bridge" || bridge_state_ == "Starting · app-owned bridge") {
    process_manager_->stopProcess(bridge_process_name);
    bridge_state_ = "Stopping · app-owned bridge";
  }
  emit changed();
}

void MocapController::setMapOutput(bool enabled)
{
  if (!map_output_client_ || !map_output_client_->service_is_ready() || map_output_request_pending_) {
    map_status_ = "Map-output service unavailable";
    emit changed();
    return;
  }
  map_output_request_pending_ = true;
  map_status_ = enabled ? "Enabling map output…" : "Disabling map output…";
  auto request = std::make_shared<std_srvs::srv::SetBool::Request>();
  request->data = enabled;
  map_output_client_->async_send_request(request,
    [this, enabled](rclcpp::Client<std_srvs::srv::SetBool>::SharedFuture future) {
      map_output_request_pending_ = false;
      try {
        const auto response = future.get();
        map_status_ = response->success ? (enabled ? "Map output enabled" : "Map output disabled") :
          QString("Map output request failed · %1").arg(QString::fromStdString(response->message));
      } catch (const std::exception & error) {
        map_status_ = QString("Map output request failed · %1").arg(error.what());
      }
      emit changed();
    });
  emit changed();
}

void MocapController::setFrozen(bool frozen)
{
  if (!freeze_client_ || !freeze_client_->service_is_ready() || freeze_request_pending_) {
    freeze_status_ = "Freeze service unavailable";
    emit changed();
    return;
  }
  freeze_request_pending_ = true;
  freeze_status_ = frozen ? "Freezing localization…" : "Resuming localization…";
  auto request = std::make_shared<std_srvs::srv::SetBool::Request>();
  request->data = frozen;
  freeze_client_->async_send_request(request,
    [this, frozen](rclcpp::Client<std_srvs::srv::SetBool>::SharedFuture future) {
      freeze_request_pending_ = false;
      try {
        const auto response = future.get();
        if (response->success) {
          frozen_ = frozen;
          freeze_status_ = frozen ? "Localization frozen" : "Live localization resumed";
        } else {
          freeze_status_ = QString("Localization state unchanged · %1")
            .arg(QString::fromStdString(response->message));
        }
      } catch (const std::exception & error) {
        freeze_status_ = QString("Freeze request failed · %1").arg(error.what());
      }
      emit changed();
    });
  emit changed();
}

void MocapController::refresh()
{
  if (have_pose_) {
    data_age_seconds_ = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - last_pose_received_).count();
    if (data_age_seconds_ > 0.5) pose_status_ = "Stale · no fresh Qualisys pose for " +
      QString::number(data_age_seconds_, 'f', 1) + " s";
    else pose_status_ = "Receiving · " + robot_;
  }
  if (have_map_pose_ && std::chrono::duration<double>(
      std::chrono::steady_clock::now() - last_map_pose_received_).count() > 0.5) {
    map_status_ = "Map pose stale · no fresh map output";
  }
  if (node_ && !have_map_pose_) {
    const auto graph = node_->get_node_graph_interface();
    const auto topic = "/qualisys_map/" + robot_.toStdString() + "/pose";
    map_status_ = graph->count_publishers(topic) > 0 ?
      "Map pose publisher found · waiting for data" : "No map pose publisher · map output unavailable or disabled";
  }
  updateProcessState();
  emit changed();
}

void MocapController::resetSubscriptions()
{
  pose_subscription_.reset();
  map_pose_subscription_.reset();
  static_tf_subscription_.reset();
  frozen_subscription_.reset();
  map_output_client_.reset();
  freeze_client_.reset();
  if (!node_) return;
  const auto robot = robot_.toStdString();
  pose_subscription_ = node_->create_subscription<geometry_msgs::msg::PoseStamped>(
    "/qualisys/" + robot + "/pose", rclcpp::QoS(10), [this](geometry_msgs::msg::PoseStamped::ConstSharedPtr message) {
      acceptPose(*message, false);
    });
  map_pose_subscription_ = node_->create_subscription<geometry_msgs::msg::PoseStamped>(
    "/qualisys_map/" + robot + "/pose", rclcpp::QoS(10), [this](geometry_msgs::msg::PoseStamped::ConstSharedPtr message) {
      acceptPose(*message, true);
    });
  static_tf_subscription_ = node_->create_subscription<tf2_msgs::msg::TFMessage>(
    "/tf_static", rclcpp::QoS(1).transient_local(), [this](tf2_msgs::msg::TFMessage::ConstSharedPtr message) {
      for (const auto & transform : message->transforms) {
        if (transform.header.frame_id == "map" && transform.child_frame_id == "mocap") {
          have_map_tf_ = true;
          map_status_ = have_map_pose_ ? "Map pose and map → mocap TF active" : "Map → mocap TF active · waiting for map pose";
          emit changed();
          return;
        }
      }
    });
  frozen_subscription_ = node_->create_subscription<std_msgs::msg::Bool>(
    "/qualisys/" + robot + "/localization_frozen", rclcpp::QoS(1).transient_local(),
    [this](std_msgs::msg::Bool::ConstSharedPtr message) {
      frozen_ = message->data;
      freeze_status_ = frozen_ ? "Localization frozen" : "Live localization";
      emit changed();
    });
  map_output_client_ = node_->create_client<std_srvs::srv::SetBool>("/qualisys/enable_map_output");
  freeze_client_ = node_->create_client<std_srvs::srv::SetBool>(
    "/qualisys/" + robot + "/freeze_localization");
}

void MocapController::acceptPose(const geometry_msgs::msg::PoseStamped & message, bool map_pose)
{
  const auto now = std::chrono::steady_clock::now();
  if (map_pose) {
    have_map_pose_ = true;
    last_map_pose_received_ = now;
    if (message.header.frame_id != "map") {
      map_status_ = "Map output has wrong frame · " + QString::fromStdString(message.header.frame_id);
    } else {
      map_status_ = have_map_tf_ ? "Map pose and map → mocap TF active" : "Map pose active · map → mocap TF not confirmed";
    }
    emit changed();
    return;
  }
  if (have_pose_) {
    const double interval = std::chrono::duration<double>(now - last_pose_received_).count();
    if (interval > 0.0 && interval < 2.0) {
      pose_intervals_.push_back(interval);
      if (pose_intervals_.size() > 20) pose_intervals_.erase(pose_intervals_.begin());
      double mean = 0.0;
      for (const double sample : pose_intervals_) mean += sample;
      frequency_hz_ = 1.0 / (mean / static_cast<double>(pose_intervals_.size()));
    }
  }
  have_pose_ = true;
  last_pose_received_ = now;
  data_age_seconds_ = 0.0;
  pose_frame_ = QString::fromStdString(message.header.frame_id);
  const auto & position = message.pose.position;
  pose_text_ = QString("x %1 m · y %2 m · z %3 m · yaw %4°")
    .arg(position.x, 0, 'f', 3).arg(position.y, 0, 'f', 3).arg(position.z, 0, 'f', 3)
    .arg(yaw_from_quaternion(message.pose.orientation), 0, 'f', 1);
  pose_status_ = "Receiving · " + robot_;
  emit changed();
}

bool MocapController::externalBridgeDetected() const
{
  if (!node_) return false;
  const auto graph = node_->get_node_graph_interface();
  const auto nodes = graph->get_node_names();
  return std::find(nodes.begin(), nodes.end(), "qualisys_ssh_bridge") != nodes.end() ||
    graph->count_publishers("/qualisys/" + robot_.toStdString() + "/pose") > 0;
}

void MocapController::updateProcessState()
{
  bool app_owned = false;
  if (process_manager_) {
    for (const auto & value : process_manager_->processes()) {
      const auto process = value.toMap();
      if (process.value("name").toString() != bridge_process_name) continue;
      const auto state = process.value("state").toString();
      if (state == "Running") {
        app_owned = true;
        bridge_state_ = "Running · app-owned bridge";
      } else if (state == "Starting" || state == "Stopping") {
        app_owned = true;
        bridge_state_ = state + " · app-owned bridge";
      } else if (state == "Failed") {
        bridge_state_ = "Failed · app-owned bridge";
      } else {
        bridge_state_ = "Stopped";
      }
      break;
    }
  }
  if (app_owned || !node_) return;
  if (externalBridgeDetected()) bridge_state_ = "Running · external bridge";
  else if (bridge_state_ != "Failed · app-owned bridge") bridge_state_ = "Stopped";
}
