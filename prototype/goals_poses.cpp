#include "goals_poses.hpp"

#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QUrl>

namespace
{
QString robot_root(const QString & robot)
{
  static const QRegularExpression safe_robot("^[A-Za-z0-9_]+$");
  return safe_robot.match(robot).hasMatch() ? "/" + robot : QString();
}

double yaw_from_quaternion(const geometry_msgs::msg::Quaternion & q)
{
  return std::atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
}

bool read_pose_array(const QByteArray & bytes, QJsonArray * array, QString * error)
{
  QJsonParseError parse_error;
  const auto document = QJsonDocument::fromJson(bytes, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    *error = QString("Invalid pose JSON: %1").arg(parse_error.errorString());
    return false;
  }
  if (document.isArray()) *array = document.array();
  else if (document.isObject() && document.object().value("poses").isArray()) {
    *array = document.object().value("poses").toArray();
  } else {
    *error = "Pose file must be a JSON array or an object with a poses array";
    return false;
  }
  for (const auto & value : *array) {
    if (!value.isObject()) { *error = "Pose entries must be objects"; return false; }
    const auto pose = value.toObject();
    if (pose.value("name").toString().trimmed().isEmpty() ||
      pose.value("frame_id").toString().trimmed().isEmpty() ||
      !pose.value("x").isDouble() || !pose.value("y").isDouble() ||
      (!pose.value("yaw_deg").isDouble() && !pose.value("yaw").isDouble())) {
      *error = "Each pose needs a name, frame_id, x, y and yaw_deg";
      return false;
    }
    const double yaw = pose.value("yaw_deg").isDouble() ? pose.value("yaw_deg").toDouble() : pose.value("yaw").toDouble();
    if (!std::isfinite(pose.value("x").toDouble()) || !std::isfinite(pose.value("y").toDouble()) ||
      !std::isfinite(yaw)) { *error = "Pose coordinates must be finite numbers"; return false; }
  }
  return true;
}
}

RosGoalsPosesAdapter::RosGoalsPosesAdapter(rclcpp::Node::SharedPtr node) : node_(std::move(node)) {}
void RosGoalsPosesAdapter::setNode(rclcpp::Node::SharedPtr node)
{
  goals_.clear(); cancels_.clear(); poses_.clear(); latest_poses_.clear();
  nav2_clients_.clear(); nav2_goals_.clear(); nav2_cancels_.clear(); sim_poses_.clear(); latest_sim_poses_.clear();
  node_ = std::move(node);
}
bool RosGoalsPosesAdapter::connected() const { return static_cast<bool>(node_); }

bool RosGoalsPosesAdapter::sendGoal(const QString & robot, const QString & mode, const QString & frame, double x, double y,
  double yaw, QString * detail)
{
  const auto root = robot_root(robot);
  if (!node_ || root.isEmpty()) { *detail = "No ROS connection or invalid selected robot"; return false; }
  if (mode == "Simulation") {
    const auto action = root.toStdString() + "/navigate_to_pose";
    auto & client = nav2_clients_[robot.toStdString()];
    if (!client) client = rclcpp_action::create_client<NavigateToPose>(node_, action);
    if (!client->action_server_is_ready()) {
      *detail = QString("Simulation goal not sent: Nav2 action server unavailable on %1").arg(QString::fromStdString(action));
      return false;
    }
    NavigateToPose::Goal goal;
    goal.pose.header.frame_id = frame.toStdString();
    goal.pose.header.stamp = node_->get_clock()->now();
    goal.pose.pose.position.x = x; goal.pose.pose.position.y = y;
    goal.pose.pose.orientation.z = std::sin(yaw / 2.0); goal.pose.pose.orientation.w = std::cos(yaw / 2.0);
    rclcpp_action::Client<NavigateToPose>::SendGoalOptions options;
    options.goal_response_callback = [this, robot](const NavigateGoalHandle::SharedPtr & handle) {
      if (!handle) { emit operationCompleted(QString("Nav2 rejected goal for %1").arg(robot), true); return; }
      nav2_goals_[robot.toStdString()] = handle;
      emit operationCompleted(QString("Nav2 accepted goal for selected robot %1").arg(robot), false);
    };
    options.result_callback = [this, robot](const NavigateGoalHandle::WrappedResult & result) {
      nav2_goals_.erase(robot.toStdString());
      if (result.code == rclcpp_action::ResultCode::SUCCEEDED) emit operationCompleted(QString("Nav2 reached goal for %1").arg(robot), false);
      else if (result.code == rclcpp_action::ResultCode::CANCELED) emit operationCompleted(QString("Nav2 canceled goal for %1").arg(robot), false);
      else emit operationCompleted(QString("Nav2 goal failed for %1").arg(robot), true);
    };
    client->async_send_goal(goal, options);
    *detail = QString("Nav2 goal requested for selected robot %1 via %2").arg(robot, QString::fromStdString(action));
    return true;
  }
  if (mode != "Hardware") { *detail = "Goal not sent: unsupported profile mode"; return false; }
  const auto topic = root.toStdString() + "/move_base_simple/goal";
  const auto graph = node_->get_node_graph_interface();
  const auto discovered = graph->get_topic_names_and_types();
  const auto endpoint = discovered.find(topic);
  if (endpoint == discovered.end() ||
    std::find(endpoint->second.begin(), endpoint->second.end(), "geometry_msgs/msg/PoseStamped") == endpoint->second.end() ||
    graph->count_subscribers(topic) == 0) {
    *detail = QString("Goal not sent: no verified PoseStamped receiver on %1").arg(QString::fromStdString(topic));
    return false;
  }
  auto & publisher = goals_[topic];
  if (!publisher) publisher = node_->create_publisher<geometry_msgs::msg::PoseStamped>(topic, 10);
  geometry_msgs::msg::PoseStamped message;
  message.header.frame_id = frame.toStdString();
  message.header.stamp = node_->get_clock()->now();
  message.pose.position.x = x; message.pose.position.y = y;
  message.pose.orientation.z = std::sin(yaw / 2.0); message.pose.orientation.w = std::cos(yaw / 2.0);
  publisher->publish(message);
  *detail = QString("Goal sent to %1 in %2").arg(QString::fromStdString(topic), frame);
  return true;
}

bool RosGoalsPosesAdapter::cancel(const QString & robot, const QString & mode, QString * detail)
{
  const auto root = robot_root(robot);
  if (!node_ || root.isEmpty()) { *detail = "No ROS connection or invalid selected robot"; return false; }
  if (mode == "Simulation") {
    auto client_it = nav2_clients_.find(robot.toStdString());
    auto goal_it = nav2_goals_.find(robot.toStdString());
    if (client_it == nav2_clients_.end() || goal_it == nav2_goals_.end()) {
      *detail = QString("No GUI-owned Nav2 goal to cancel for selected robot %1").arg(robot);
      return false;
    }
    nav2_cancels_[robot.toStdString()] = client_it->second->async_cancel_goal(goal_it->second);
    *detail = QString("Nav2 cancel requested for selected robot %1").arg(robot);
    return true;
  }
  if (mode != "Hardware") { *detail = "Navigation cancel unavailable: unsupported profile mode"; return false; }
  const auto topic = root.toStdString() + "/move_base/cancel";
  const auto graph = node_->get_node_graph_interface();
  const auto discovered = graph->get_topic_names_and_types();
  const auto endpoint = discovered.find(topic);
  if (endpoint == discovered.end() ||
    std::find(endpoint->second.begin(), endpoint->second.end(), "actionlib_msgs/msg/GoalID") == endpoint->second.end() ||
    graph->count_subscribers(topic) == 0) {
    *detail = QString("Navigation cancel unavailable: no verified receiver on %1").arg(QString::fromStdString(topic));
    return false;
  }
  auto & publisher = cancels_[topic];
  if (!publisher) publisher = node_->create_publisher<actionlib_msgs::msg::GoalID>(topic, 10);
  publisher->publish(actionlib_msgs::msg::GoalID{});
  *detail = QString("Cancel sent for selected robot %1").arg(robot);
  return true;
}

bool RosGoalsPosesAdapter::currentPose(const QString & robot, const QString & mode, double * x, double * y, double * yaw,
  QString * frame, double * ageSeconds, QString * detail)
{
  const auto root = robot_root(robot);
  if (!node_ || root.isEmpty()) { *detail = "No ROS connection or invalid selected robot"; return false; }
  if (mode == "Simulation") {
    const auto topic = root.toStdString() + "/ground_truth/pose";
    const auto graph = node_->get_node_graph_interface();
    const auto topics = graph->get_topic_names_and_types();
    const auto endpoint = topics.find(topic);
    if (endpoint == topics.end() ||
      std::find(endpoint->second.begin(), endpoint->second.end(), "geometry_msgs/msg/PoseStamped") == endpoint->second.end() ||
      graph->count_publishers(topic) == 0) {
      *detail = QString("Simulation pose unavailable: no verified PoseStamped source on %1").arg(QString::fromStdString(topic));
      return false;
    }
    auto found = latest_sim_poses_.find(robot.toStdString());
    if (!sim_poses_.count(topic)) {
      sim_poses_[topic] = node_->create_subscription<geometry_msgs::msg::PoseStamped>(topic, 10,
        [this, robot](geometry_msgs::msg::PoseStamped::ConstSharedPtr message) {
          latest_sim_poses_[robot.toStdString()] = {*message, std::chrono::steady_clock::now()};
        });
      *detail = QString("Waiting for simulation ground truth on %1").arg(QString::fromStdString(topic));
      return false;
    }
    if (found == latest_sim_poses_.end()) { *detail = QString("No simulation ground truth received on %1").arg(QString::fromStdString(topic)); return false; }
    *ageSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - found->second.received).count();
    *frame = QString::fromStdString(found->second.pose.header.frame_id).trimmed();
    if (*ageSeconds > 2.0) { *detail = QString("Simulation pose is stale (%1 s old)").arg(*ageSeconds, 0, 'f', 1); return false; }
    if (frame->isEmpty()) { *detail = "Simulation pose rejected: ground-truth frame is empty"; return false; }
    *x = found->second.pose.pose.position.x; *y = found->second.pose.pose.position.y;
    *yaw = yaw_from_quaternion(found->second.pose.pose.orientation);
    *detail = QString("Current pose from %1 · frame %2 · %3 s old").arg(QString::fromStdString(topic), *frame).arg(*ageSeconds, 0, 'f', 1);
    return true;
  }
  if (mode != "Hardware") { *detail = "Current pose unavailable: unsupported profile mode"; return false; }
  const auto topic = root.toStdString() + "/robot_pose";
  const auto graph = node_->get_node_graph_interface();
  const auto topics = graph->get_topic_names_and_types();
  const auto endpoint = topics.find(topic);
  if (endpoint == topics.end() ||
    std::find(endpoint->second.begin(), endpoint->second.end(), "geometry_msgs/msg/Pose") == endpoint->second.end() ||
    graph->count_publishers(topic) == 0) {
    *detail = QString("Current MiR pose unavailable: no verified Pose source on %1").arg(QString::fromStdString(topic));
    return false;
  }
  auto found = latest_poses_.find(robot.toStdString());
  if (!poses_.count(topic)) {
    poses_[topic] = node_->create_subscription<geometry_msgs::msg::Pose>(topic, 10,
      [this, robot](geometry_msgs::msg::Pose::ConstSharedPtr message) {
        latest_poses_[robot.toStdString()] = {*message, std::chrono::steady_clock::now()};
      });
    *detail = QString("Waiting for current pose on %1").arg(QString::fromStdString(topic));
    return false;
  }
  if (found == latest_poses_.end()) { *detail = QString("No current pose received on %1").arg(QString::fromStdString(topic)); return false; }
  *ageSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - found->second.received).count();
  *frame = "map"; // The profile's MiR robot_pose contract is expressed in map.
  if (*ageSeconds > 2.0) { *detail = QString("Current pose is stale (%1 s old)").arg(*ageSeconds, 0, 'f', 1); return false; }
  *x = found->second.pose.position.x; *y = found->second.pose.position.y;
  *yaw = yaw_from_quaternion(found->second.pose.orientation);
  *detail = QString("Current pose from %1 · map · %2 s old").arg(QString::fromStdString(topic)).arg(*ageSeconds, 0, 'f', 1);
  return true;
}

QString RosGoalsPosesAdapter::adapterStatus(const QString & robot, const QString & mode) const
{
  const auto root = robot_root(robot);
  if (!node_ || root.isEmpty()) return "Disconnected · no goal adapter active";
  if (mode == "Simulation") {
    auto * self = const_cast<RosGoalsPosesAdapter *>(this);
    auto & client = self->nav2_clients_[robot.toStdString()];
    if (!client) client = rclcpp_action::create_client<NavigateToPose>(node_, root.toStdString() + "/navigate_to_pose");
    return QString("Simulation profile · Nav2 NavigateToPose · %1 · ground truth %2/ground_truth/pose")
      .arg(client->action_server_is_ready() ? "ready" : "waiting for verified action server", root);
  }
  if (mode == "Hardware") {
    const auto topic = root.toStdString() + "/move_base_simple/goal";
    const auto graph = node_->get_node_graph_interface();
    const auto topics = graph->get_topic_names_and_types();
    const auto it = topics.find(topic);
    const bool ready = it != topics.end() &&
      std::find(it->second.begin(), it->second.end(), "geometry_msgs/msg/PoseStamped") != it->second.end() &&
      graph->count_subscribers(topic) > 0;
    return QString("Hardware profile · MiR PoseStamped goal topic · %1 · cancel %2/move_base/cancel")
      .arg(ready ? "ready" : "waiting for verified goal receiver", root);
  }
  return "Unsupported profile · navigation actions disabled";
}
void RosGoalsPosesAdapter::poll()
{
  for (auto it = nav2_cancels_.begin(); it != nav2_cancels_.end();) {
    if (it->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { ++it; continue; }
    const auto robot = QString::fromStdString(it->first);
    try {
      const auto response = it->second.get();
      if (response->return_code == action_msgs::srv::CancelGoal::Response::ERROR_NONE) {
        emit operationCompleted(QString("Nav2 cancel accepted for selected robot %1").arg(robot), false);
      } else {
        emit operationCompleted(QString("Nav2 did not accept cancel for selected robot %1").arg(robot), true);
      }
    } catch (const std::exception & error) {
      emit operationCompleted(QString("Nav2 cancel failed for %1: %2").arg(robot, error.what()), true);
    }
    it = nav2_cancels_.erase(it);
  }
}

GoalsPoses::GoalsPoses(std::shared_ptr<GoalsPosesAdapter> adapter, QString storage_path, QObject * parent)
: QObject(parent), adapter_(std::move(adapter)), storage_path_(std::move(storage_path))
{
  current_pose_timer_.setInterval(50);
  connect(&current_pose_timer_, &QTimer::timeout, this, [this]() {
    double x = 0, y = 0, yaw = 0, age = 0; QString frame, detail;
    if (adapter_ && adapter_->currentPose(pending_robot_, pending_mode_, &x, &y, &yaw, &frame, &age, &detail)) {
      current_pose_timer_.stop(); busy_ = false;
      if (!std::isfinite(age) || age > 2.0 || frame.trimmed().isEmpty()) { setStatus("Current pose rejected · missing, stale, or frame is unknown"); return; }
      setStatus(detail);
      save(pending_pose_name_, frame, x, y, yaw * 180.0 / M_PI);
      return;
    }
    if (++current_pose_attempts_ >= 40) {
      current_pose_timer_.stop(); setStatus(detail.isEmpty() ? "Current pose unavailable" : detail);
    }
  });
  adapter_poll_timer_.setInterval(100);
  connect(&adapter_poll_timer_, &QTimer::timeout, this, [this]() { if (adapter_) adapter_->poll(); });
  adapter_poll_timer_.start();
  if (adapter_) connect(adapter_.get(), &GoalsPosesAdapter::operationCompleted, this,
    [this](const QString & detail, bool error) { setStatus(detail); if (error) emit changed(); });
  reload();
}

QVariantList GoalsPoses::poses() const
{
  QVariantList result;
  for (const auto & pose : poses_) result.push_back(pose.toObject().toVariantMap());
  return result;
}
void GoalsPoses::setConnected(bool connected)
{
  connected_ = connected && adapter_ && adapter_->connected();
  if (!connected_) setStatus("Disconnected · goals require a connected Simulation or Hardware profile");
  else setStatus("Connected · navigation adapter selected by profile · runtime endpoints are checked before use");
}
void GoalsPoses::setStatus(const QString & text) { status_ = text; busy_ = false; emit changed(); }

void GoalsPoses::reload()
{
  QFile file(storage_path_);
  if (!file.exists()) { poses_ = {}; setStatus("Pose file is ready · " + storage_path_); return; }
  if (!file.open(QIODevice::ReadOnly)) { poses_ = {}; setStatus("Could not read poses: " + file.errorString()); return; }
  QString error;
  if (!read_pose_array(file.readAll(), &poses_, &error)) { poses_ = {}; setStatus(error); return; }
  setStatus(QString("Loaded %1 saved pose(s)").arg(poses_.size()));
}

bool GoalsPoses::importFile(const QString & path_or_url)
{
  const QUrl url(path_or_url);
  const QString path = url.isLocalFile() ? url.toLocalFile() : path_or_url;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) { setStatus("Could not read pose import: " + file.errorString()); return false; }
  QJsonArray incoming;
  QString error;
  if (!read_pose_array(file.readAll(), &incoming, &error)) { setStatus("Pose import rejected · " + error); return false; }
  QJsonArray normalized;
  for (const auto & value : incoming) {
    const auto source = value.toObject();
    const double imported_yaw = source.value("yaw_deg").isDouble() ? source.value("yaw_deg").toDouble() : source.value("yaw").toDouble();
    const QString name = source.value("name").toString().trimmed();
    const QString frame = source.value("frame_id").toString().trimmed();
    const double x = source.value("x").toDouble(), y = source.value("y").toDouble();
    if (!validPose(name, frame, x, y, imported_yaw, &error)) {
      setStatus(QString("Pose import rejected · %1: %2").arg(name.isEmpty() ? "unnamed pose" : name, error));
      return false;
    }
    normalized.append(QJsonObject{{"name", name}, {"frame_id", frame}, {"x", x}, {"y", y}, {"yaw_deg", imported_yaw}});
  }
  const auto previous = poses_;
  int merged = 0;
  for (const auto & value : normalized) {
    const auto name = value.toObject().value("name").toString();
    bool replaced = false;
    for (int index = 0; index < poses_.size(); ++index) {
      if (poses_[index].toObject().value("name").toString() == name) {
        poses_[index] = value; replaced = true; break;
      }
    }
    if (!replaced) poses_.append(value);
    ++merged;
  }
  if (!write()) { poses_ = previous; emit changed(); return false; }
  setStatus(QString("Imported %1 pose(s) from %2").arg(merged).arg(QFileInfo(path).fileName()));
  return true;
}

bool GoalsPoses::write()
{
  const QFileInfo info(storage_path_);
  if (!QDir().mkpath(info.absolutePath())) { setStatus("Could not create pose directory"); return false; }
  QSaveFile file(storage_path_);
  if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(QJsonObject{{"poses", poses_}}).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
    setStatus("Could not save poses: " + file.errorString()); return false;
  }
  setStatus(QString("Saved %1 pose(s) · %2").arg(poses_.size()).arg(storage_path_));
  return true;
}

bool GoalsPoses::validPose(const QString & name, const QString & frame, double x, double y, double yawDegrees,
  QString * error) const
{
  static const QRegularExpression frame_pattern("^[A-Za-z][A-Za-z0-9_/]*$");
  if (name.trimmed().isEmpty()) *error = "Pose name is required";
  else if (frame.trimmed().isEmpty() || !frame_pattern.match(frame.trimmed()).hasMatch()) *error = "Enter a valid named frame";
  else if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(yawDegrees) ||
    std::abs(x) > 1000.0 || std::abs(y) > 1000.0 || yawDegrees < -180.0 || yawDegrees > 180.0) *error = "Coordinates are outside the supported range";
  return error->isEmpty();
}

bool GoalsPoses::save(const QString & name, const QString & frame, double x, double y, double yawDegrees)
{
  QString error;
  if (!validPose(name, frame, x, y, yawDegrees, &error)) { setStatus(error); return false; }
  QJsonObject value{{"name", name.trimmed()}, {"frame_id", frame.trimmed()}, {"x", x}, {"y", y}, {"yaw_deg", yawDegrees}};
  for (int i = 0; i < poses_.size(); ++i) if (poses_[i].toObject().value("name").toString() == name.trimmed()) { poses_[i] = value; return write(); }
  poses_.append(value); return write();
}
bool GoalsPoses::remove(const QString & name)
{
  for (int i = 0; i < poses_.size(); ++i) if (poses_[i].toObject().value("name").toString() == name) { poses_.removeAt(i); return write(); }
  setStatus("Select a saved pose to delete"); return false;
}

bool GoalsPoses::send(const QString & robot, const QString & mode, const QString & name,
  const QString & frame, double x, double y, double yawDegrees)
{
  QString error;
  if (!validPose(name.isEmpty() ? "goal" : name, frame, x, y, yawDegrees, &error)) { setStatus(error); return false; }
  if ((mode != "Hardware" && mode != "Simulation") || !connected_ || !adapter_) { setStatus("Goal not sent · connect the selected profile first"); return false; }
  QString detail;
  const bool ok = adapter_->sendGoal(robot, mode, frame.trimmed(), x, y, yawDegrees * M_PI / 180.0, &detail);
  setStatus(detail);
  return ok;
}
bool GoalsPoses::sendSaved(const QString & robot, const QString & mode, const QString & name)
{
  for (const auto & item : poses_) {
    const auto pose = item.toObject();
    if (pose.value("name").toString() == name) return send(robot, mode, name, pose.value("frame_id").toString(),
      pose.value("x").toDouble(), pose.value("y").toDouble(), pose.value("yaw_deg").toDouble(pose.value("yaw").toDouble()));
  }
  setStatus("Select a saved pose first"); return false;
}
bool GoalsPoses::saveCurrent(const QString & robot, const QString & mode, const QString & name)
{
  if ((mode != "Hardware" && mode != "Simulation") || !connected_ || !adapter_) { setStatus("Current pose unavailable · connect the selected profile first"); return false; }
  double x = 0, y = 0, yaw = 0, age = 0; QString frame, detail;
  if (!adapter_->currentPose(robot, mode, &x, &y, &yaw, &frame, &age, &detail)) {
    pending_robot_ = robot; pending_pose_name_ = name.trimmed().isEmpty() ? "current_pose" : name.trimmed(); pending_mode_ = mode;
    current_pose_attempts_ = 0; setStatus("Waiting for a fresh pose from the selected robot…");
    busy_ = true; emit changed(); current_pose_timer_.start(); return true;
  }
  if (!std::isfinite(age) || age > 2.0 || frame.trimmed().isEmpty()) { setStatus("Current pose rejected · missing, stale, or frame is unknown"); return false; }
  setStatus(detail);
  return save(name, frame, x, y, yaw * 180.0 / M_PI);
}
void GoalsPoses::cancelSelected(const QString & robot, const QString & mode)
{
  if ((mode != "Hardware" && mode != "Simulation") || !connected_ || !adapter_) { setStatus("Navigation cancel unavailable · connect the selected profile first"); return; }
  QString detail;
  adapter_->cancel(robot, mode, &detail);
  setStatus(detail); // The adapter reports success only after a verified selected-robot receiver was found.
}
QString GoalsPoses::adapterStatus(const QString & robot, const QString & mode) const
{
  return connected_ && adapter_ ? adapter_->adapterStatus(robot, mode) : "Disconnected · connect to inspect profile navigation adapter";
}

bool run_goals_poses_contract_test()
{
  class Fake final : public GoalsPosesAdapter {
  public:
    bool connected() const override { return true; }
    bool sendGoal(const QString & robot, const QString & mode, const QString & frame, double x, double y, double yaw, QString * detail) override {
      sent_mode = mode;
      sent_robot = robot; sent_frame = frame; sent_x = x; sent_y = y; sent_yaw = yaw; *detail = "fake goal accepted"; return true;
    }
    bool cancel(const QString & robot, const QString & mode, QString * detail) override { cancelled_robot = robot; cancelled_mode = mode; *detail = "fake cancel accepted"; return true; }
    bool currentPose(const QString &, const QString & mode, double * x, double * y, double * yaw, QString * frame, double * age, QString * detail) override {
      current_mode = mode;
      *x = 1.25; *y = -2.5; *yaw = 0.5; *frame = current_frame; *age = current_age; *detail = "fake current pose"; return current_ok;
    }
    QString adapterStatus(const QString &, const QString & mode) const override { return mode + " fake"; }
    void poll() override {}
    QString sent_mode, sent_robot, sent_frame, cancelled_mode, cancelled_robot, current_mode, current_frame{"map"}; double sent_x{0}, sent_y{0}, sent_yaw{0}, current_age{0.2}; bool current_ok{true};
  };
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  auto adapter = std::make_shared<Fake>();
  const auto path = directory.filePath("poses.json");
  QFile fixture(path);
  if (!fixture.open(QIODevice::WriteOnly) || fixture.write("[{\"name\":\"dock\",\"frame_id\":\"map\",\"x\":1,\"y\":2,\"yaw_deg\":90}]") < 0) return false;
  fixture.close();
  GoalsPoses goals(adapter, path);
  goals.setConnected(true);
  if (goals.poses().size() != 1 || !goals.save("home", "map", 0, 0, 0) || goals.poses().size() != 2) return false;
  if (goals.save("bad", "", 0, 0, 0)) return false;
  if (goals.send("mur620b", "Other", "blocked", "map", 0, 0, 0) || !adapter->sent_mode.isEmpty()) return false;
  if (!goals.sendSaved("mur620b", "Simulation", "dock") || adapter->sent_robot != "mur620b" || adapter->sent_mode != "Simulation" || adapter->sent_frame != "map" || std::abs(adapter->sent_yaw - M_PI/2) > 1e-6) return false;
  goals.cancelSelected("mur620b", "Simulation");
  if (adapter->cancelled_robot != "mur620b" || adapter->cancelled_mode != "Simulation") return false;
  if (!goals.saveCurrent("mur620b", "Simulation", "live") || goals.poses().size() != 3 || adapter->current_mode != "Simulation") return false;
  adapter->current_age = 3.0;
  if (goals.saveCurrent("mur620b", "Simulation", "stale") || goals.poses().size() != 3) return false;
  adapter->current_age = 0.2; adapter->current_frame.clear();
  if (goals.saveCurrent("mur620b", "Simulation", "bad_frame") || goals.poses().size() != 3) return false;
  const auto object_path = directory.filePath("object_poses.json");
  QFile object_file(object_path);
  if (!object_file.open(QIODevice::WriteOnly) || object_file.write("{\"poses\":[{\"name\":\"dock\",\"frame_id\":\"map\",\"x\":4,\"y\":5,\"yaw_deg\":45},{\"name\":\"legacy\",\"frame_id\":\"map\",\"x\":2,\"y\":3,\"yaw_deg\":45}]}") < 0) return false;
  object_file.close();
  GoalsPoses imported(adapter, directory.filePath("saved.json"));
  if (!imported.save("dock", "map", 0, 0, 0) || !imported.importFile(QUrl::fromLocalFile(object_path).toString()) ||
    imported.poses().size() != 2 || imported.poses()[0].toMap().value("x").toDouble() != 4 ||
    imported.poses()[1].toMap().value("name").toString() != "legacy") return false;
  const auto array_path = directory.filePath("array_poses.json");
  QFile array_file(array_path);
  if (!array_file.open(QIODevice::WriteOnly) || array_file.write("[{\"name\":\"array_pose\",\"frame_id\":\"map\",\"x\":6,\"y\":7,\"yaw_deg\":90}]") < 0) return false;
  array_file.close();
  if (!imported.importFile(array_path) || imported.poses().size() != 3) return false;
  const auto invalid_path = directory.filePath("invalid_poses.json");
  QFile invalid_file(invalid_path);
  if (!invalid_file.open(QIODevice::WriteOnly) || invalid_file.write("{\"poses\":[{\"name\":\"good\",\"frame_id\":\"map\",\"x\":1,\"y\":2,\"yaw_deg\":3},{\"name\":\"bad\",\"frame_id\":\"map\",\"x\":1001,\"y\":0,\"yaw_deg\":0}]}") < 0) return false;
  invalid_file.close();
  if (imported.importFile(invalid_path) || imported.poses().size() != 3) return false;
  return goals.remove("home") && goals.poses().size() == 2;
}
