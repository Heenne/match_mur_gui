#include "mir_hardware_control.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QEventLoop>
#include <QRegularExpression>
#include <QTimer>

namespace
{
constexpr double kMaxLinearMps = 0.15;
constexpr double kMaxAngularRadps = 0.4;
constexpr auto kCommandLifespan = std::chrono::milliseconds(250);
const std::string kTwistType = "geometry_msgs/msg/Twist";
const std::string kTwistStampedType = "geometry_msgs/msg/TwistStamped";
const std::string kTriggerType = "std_srvs/srv/Trigger";
const std::string kColorType = "mir_srvs/srv/ColorRGB";

std::string topicFor(const QString & robot, const QString & suffix)
{
  return "/" + robot.toStdString() + "/" + suffix.toStdString();
}

std::string frameFor(const QString & robot)
{
  return robot.toStdString() + "/base_link";
}

class FakeMirHardwareAdapter final : public MirHardwareAdapter
{
public:
  struct Command
  {
    std::string name;
    bool stamped{false};
    double linear{0.0};
    double angular{0.0};
    int red{0};
    int green{0};
    int blue{0};
  };

  bool connected() const override { return graph_connected; }
  MirEndpoint inspectTopic(const std::string & topic) override
  {
    const auto found = topics.find(topic);
    return found == topics.end() ? MirEndpoint{} : found->second;
  }
  void publishTwist(const std::string & topic, bool stamped, const std::string &,
    double linear, double angular) override
  {
    commands.push_back({topic, stamped, linear, angular});
  }
  bool hasService(const std::string & service, const std::string & type) override
  {
    const auto found = services.find(service);
    return found != services.end() && found->second == type;
  }
  void callTrigger(const std::string & service, Result result) override
  {
    ++service_calls;
    commands.push_back({service});
    result(trigger_success, trigger_message);
  }
  void setColor(const std::string & service, int red, int green, int blue, Result result) override
  {
    ++service_calls;
    Command command;
    command.name = service;
    command.red = red;
    command.green = green;
    command.blue = blue;
    commands.push_back(command);
    result(color_success, color_message);
  }

  bool graph_connected{true};
  bool trigger_success{true};
  bool color_success{true};
  QString trigger_message{"effect applied"};
  QString color_message{"color accepted"};
  size_t service_calls{0};
  std::unordered_map<std::string, MirEndpoint> topics;
  std::unordered_map<std::string, std::string> services;
  std::vector<Command> commands;
};

void waitForEvents(int milliseconds)
{
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec();
}
}

RosMirHardwareAdapter::RosMirHardwareAdapter(rclcpp::Node::SharedPtr node)
: node_(std::move(node))
{
}

void RosMirHardwareAdapter::setNode(rclcpp::Node::SharedPtr node)
{
  node_ = std::move(node);
  if (!node_) {
    twist_publishers_.clear();
    stamped_publishers_.clear();
  }
}

bool RosMirHardwareAdapter::connected() const
{
  return node_ && rclcpp::ok(node_->get_node_base_interface()->get_context());
}

MirEndpoint RosMirHardwareAdapter::inspectTopic(const std::string & topic)
{
  if (!connected()) return {};
  const auto graph = node_->get_topic_names_and_types();
  const auto found = graph.find(topic);
  if (found == graph.end() || found->second.size() != 1) return {};
  return {found->second.front(), static_cast<size_t>(node_->count_subscribers(topic))};
}

void RosMirHardwareAdapter::publishTwist(const std::string & topic, bool stamped,
  const std::string & frame, double linear_x, double angular_z)
{
  if (!connected()) return;
  auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).lifespan(rclcpp::Duration(kCommandLifespan));
  if (stamped) {
    auto publisher = stamped_publishers_.find(topic);
    if (publisher == stamped_publishers_.end()) {
      publisher = stamped_publishers_.emplace(topic,
        node_->create_publisher<geometry_msgs::msg::TwistStamped>(topic, qos)).first;
    }
    geometry_msgs::msg::TwistStamped message;
    message.header.stamp = node_->now();
    message.header.frame_id = frame;
    message.twist.linear.x = linear_x;
    message.twist.angular.z = angular_z;
    publisher->second->publish(message);
    return;
  }

  auto publisher = twist_publishers_.find(topic);
  if (publisher == twist_publishers_.end()) {
    publisher = twist_publishers_.emplace(topic,
      node_->create_publisher<geometry_msgs::msg::Twist>(topic, qos)).first;
  }
  geometry_msgs::msg::Twist message;
  message.linear.x = linear_x;
  message.angular.z = angular_z;
  publisher->second->publish(message);
}

bool RosMirHardwareAdapter::hasService(const std::string & service, const std::string & type)
{
  if (!connected()) return false;
  const auto graph = node_->get_service_names_and_types();
  const auto found = graph.find(service);
  return found != graph.end() && found->second.size() == 1 && found->second.front() == type;
}

void RosMirHardwareAdapter::callTrigger(const std::string & service, Result result)
{
  if (!connected() || !hasService(service, kTriggerType)) {
    result(false, "service unavailable or incompatible");
    return;
  }
  auto client = node_->create_client<std_srvs::srv::Trigger>(service);
  if (!client->service_is_ready()) {
    result(false, "service unavailable");
    return;
  }
  auto request = std::make_shared<std_srvs::srv::Trigger::Request>();
  client->async_send_request(request, [client, result = std::move(result)](
      rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture future) {
      try {
        const auto response = future.get();
        result(response->success, QString::fromStdString(response->message));
      } catch (const std::exception & error) {
        result(false, QString("service call failed: %1").arg(error.what()));
      }
    });
}

void RosMirHardwareAdapter::setColor(const std::string & service, int red, int green, int blue,
  Result result)
{
  if (!connected() || !hasService(service, kColorType)) {
    result(false, "service unavailable or incompatible");
    return;
  }
  auto client = node_->create_client<mir_srvs::srv::ColorRGB>(service);
  if (!client->service_is_ready()) {
    result(false, "service unavailable");
    return;
  }
  auto request = std::make_shared<mir_srvs::srv::ColorRGB::Request>();
  request->red = static_cast<uint8_t>(std::clamp(red, 0, 255));
  request->green = static_cast<uint8_t>(std::clamp(green, 0, 255));
  request->blue = static_cast<uint8_t>(std::clamp(blue, 0, 255));
  client->async_send_request(request, [client, result = std::move(result)](
      rclcpp::Client<mir_srvs::srv::ColorRGB>::SharedFuture future) {
      try {
        const auto response = future.get();
        result(response->success, response->success ? "color accepted" : "service rejected color");
      } catch (const std::exception & error) {
        result(false, QString("service call failed: %1").arg(error.what()));
      }
    });
}

MirHardwareControl::MirHardwareControl(std::shared_ptr<MirHardwareAdapter> adapter, QObject * parent)
: QObject(parent), adapter_(std::move(adapter))
{
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, &MirHardwareControl::tick);
  light_timeout_timer_.setSingleShot(true);
  light_timeout_timer_.setInterval(5000);
  connect(&light_timeout_timer_, &QTimer::timeout, this, [this]() {
    ++light_request_id_;
    setLightStatus("Failed · MiR light service timed out without a response");
  });
}

void MirHardwareControl::setContext(const QString & robot, const QString & mode,
  const QString & target, bool connected)
{
  const auto selected = robot.trimmed();
  static const QRegularExpression robot_segment("^[A-Za-z_][A-Za-z0-9_]*$");
  const bool valid_robot = robot_segment.match(selected).hasMatch() && !target.trimmed().isEmpty();
  const bool next_hardware_mode = mode == "Hardware";
  if (selected != robot_ || mode != (hardware_mode_ ? "Hardware" : "Simulation") ||
    target != target_ || connected != context_connected_) {
    stop("session context changed");
    light_timeout_timer_.stop();
    ++light_request_id_;
    armed_ = false;
    robot_ = valid_robot ? selected : QString();
    target_ = target.trimmed();
    context_key_ = mode + "|" + target_ + "|" + robot_;
    hardware_mode_ = next_hardware_mode;
    context_connected_ = connected;
    setStatus("MiR jog disarmed · confirm the new hardware session before enabling");
    setLightStatus("MiR light services are available only in a connected hardware session");
    emit changed();
  }
  if (!valid_robot) robot_.clear();
}

void MirHardwareControl::setArmed(bool armed)
{
  const bool can_arm = !robot_.isEmpty() && hardware_mode_ && context_connected_ &&
    adapter_ && adapter_->connected();
  if (armed && !can_arm) {
    armed_ = false;
    setStatus("Cannot arm · select and connect one Hardware robot first");
  } else {
    armed_ = armed;
    setStatus(armed ? "Armed · hold a direction to jog the selected MiR" : "MiR jog disarmed");
  }
  if (!armed_) stop("disarmed");
  emit changed();
}

void MirHardwareControl::setLinearSpeed(double speed)
{
  if (holding_) stop("speed changed");
  linear_speed_ = std::isfinite(speed) ? std::clamp(speed, 0.01, kMaxLinearMps) : 0.01;
  emit changed();
}

void MirHardwareControl::setAngularSpeed(double speed)
{
  if (holding_) stop("speed changed");
  angular_speed_ = std::isfinite(speed) ? std::clamp(speed, 0.05, kMaxAngularRadps) : 0.05;
  emit changed();
}

bool MirHardwareControl::press(const QString & direction)
{
  if (!armed_ || !context_connected_ || !hardware_mode_ || robot_.isEmpty()) {
    setStatus("Jog disarmed · explicitly arm it for one connected Hardware robot");
    return false;
  }
  if (holding_) stop("direction changed");
  if (direction == "forward") { linear_x_ = linear_speed_; angular_z_ = 0.0; }
  else if (direction == "backward") { linear_x_ = -linear_speed_; angular_z_ = 0.0; }
  else if (direction == "left") { linear_x_ = 0.0; angular_z_ = angular_speed_; }
  else if (direction == "right") { linear_x_ = 0.0; angular_z_ = -angular_speed_; }
  else return false;
  if (!selectEndpoint() || !send(linear_x_, angular_z_)) {
    clearHold();
    setStatus(QString("Unavailable · no compatible Twist subscriber on %1/cmd_vel or %1/cmd_vel_stamped")
      .arg(QString::fromStdString("/" + robot_.toStdString())));
    return false;
  }
  holding_ = true;
  timer_.start();
  setStatus(QString("Jogging · /%1 · linear ≤ %2 m/s · angular ≤ %3 rad/s")
    .arg(robot_).arg(kMaxLinearMps).arg(kMaxAngularRadps));
  emit changed();
  return true;
}

void MirHardwareControl::release()
{
  stop("released");
}

void MirHardwareControl::stop(const QString & reason)
{
  if (holding_) {
    const bool stopped = send(0.0, 0.0);
    clearHold();
    setStatus(stopped ? "Stopped · " + reason : "Stop unconfirmed · ROS endpoint unavailable");
  } else {
    clearHold();
    if (!reason.isEmpty() && armed_) setStatus("Stopped · " + reason);
  }
  emit changed();
}

void MirHardwareControl::tick()
{
  if (!holding_) { timer_.stop(); return; }
  if (!armed_ || !context_connected_ || !hardware_mode_ || !adapter_ ||
    !adapter_->connected() || !send(linear_x_, angular_z_)) {
    if (adapter_ && adapter_->connected() && !command_topic_.empty()) {
      try { adapter_->publishTwist(command_topic_, command_stamped_, frameFor(robot_), 0.0, 0.0); }
      catch (const std::exception &) {}
    }
    clearHold();
    armed_ = false;
    setStatus("Stopped and disarmed · selected hardware graph or verified MiR command endpoint was lost");
    emit changed();
  }
}

bool MirHardwareControl::selectEndpoint()
{
  command_topic_.clear();
  if (!adapter_ || !adapter_->connected() || robot_.isEmpty()) return false;
  try {
    const auto plain = topicFor(robot_, "cmd_vel");
    const auto stamped = topicFor(robot_, "cmd_vel_stamped");
    const auto plain_endpoint = adapter_->inspectTopic(plain);
    const auto stamped_endpoint = adapter_->inspectTopic(stamped);
    const bool plain_ok = plain_endpoint.type == kTwistType && plain_endpoint.subscribers > 0;
    const bool stamped_ok = stamped_endpoint.type == kTwistStampedType && stamped_endpoint.subscribers > 0;
    if (plain_ok == stamped_ok) return false;
    command_stamped_ = stamped_ok;
    command_topic_ = stamped_ok ? stamped : plain;
    return true;
  } catch (const std::exception &) { return false; }
}

bool MirHardwareControl::send(double linear_x, double angular_z)
{
  if (!adapter_ || !adapter_->connected() || command_topic_.empty()) return false;
  try {
    const auto endpoint = adapter_->inspectTopic(command_topic_);
    if (endpoint.type != (command_stamped_ ? kTwistStampedType : kTwistType) ||
      endpoint.subscribers == 0) return false;
    adapter_->publishTwist(command_topic_, command_stamped_, frameFor(robot_), linear_x, angular_z);
    return true;
  } catch (const std::exception &) { return false; }
}

void MirHardwareControl::rainbowStart()
{
  callLight("rainbow_start", QString::fromStdString(kTriggerType), false);
}

void MirHardwareControl::rainbowStop()
{
  callLight("rainbow_stop", QString::fromStdString(kTriggerType), false);
}

void MirHardwareControl::matchColor()
{
  callLight("match_color", QString::fromStdString(kTriggerType), false);
}

void MirHardwareControl::setSolidColor(int red, int green, int blue)
{
  callLight("solid_color", QString::fromStdString(kColorType), true, red, green, blue);
}

void MirHardwareControl::callLight(const QString & suffix, const QString & type, bool solid,
  int red, int green, int blue)
{
  if (!robot_.isEmpty() && hardware_mode_ && context_connected_ && adapter_ && adapter_->connected()) {
    const auto service = serviceName("RGB_control/" + suffix);
    try {
      if (adapter_->hasService(service, type.toStdString())) {
        setLightStatus("Requesting " + suffix + " · " + QString::fromStdString(service));
        const auto request_context = context_key_;
        const auto request_id = ++light_request_id_;
        light_timeout_timer_.start();
        auto result = [this, suffix, request_context, request_id](bool success, const QString & detail) {
          if (request_context != context_key_ || request_id != light_request_id_) return;
          light_timeout_timer_.stop();
          setLightStatus((success ? "Applied " : "Failed ") + suffix + " · " + detail);
        };
        if (solid) adapter_->setColor(service,
          std::clamp(red, 0, 255), std::clamp(green, 0, 255), std::clamp(blue, 0, 255),
          std::move(result));
        else adapter_->callTrigger(service, std::move(result));
        return;
      }
    } catch (const std::exception & error) {
      light_timeout_timer_.stop();
      setLightStatus(QString("Failed · %1").arg(error.what()));
      return;
    }
    setLightStatus(QString("Unavailable · %1 (%2)")
      .arg(QString::fromStdString(service), type));
    return;
  }
  setLightStatus("Unavailable · connect to one Hardware robot first");
}

void MirHardwareControl::clearHold()
{
  timer_.stop();
  holding_ = false;
  linear_x_ = 0.0;
  angular_z_ = 0.0;
  emit changed();
}

void MirHardwareControl::setStatus(const QString & status)
{
  status_ = status;
  emit changed();
}

void MirHardwareControl::setLightStatus(const QString & status)
{
  light_status_ = status;
  emit changed();
}

std::string MirHardwareControl::serviceName(const QString & suffix) const
{
  return topicFor(robot_, suffix);
}

bool run_mir_hardware_control_contract_test()
{
#define VERIFY_MIR(condition) do { if (!(condition)) { qCritical("MiR control assertion failed at line %d", __LINE__); return false; } } while (false)
  auto adapter = std::make_shared<FakeMirHardwareAdapter>();
  MirHardwareControl control(adapter);
  control.setContext("mur620d", "Hardware", "lab-host", true);
  VERIFY_MIR(!control.armed() && !control.press("forward") && adapter->commands.empty());
  control.setArmed(true);
  VERIFY_MIR(control.armed() && !control.press("forward") && adapter->commands.empty());
  adapter->topics["/mur620d/cmd_vel"] = {kTwistType, 1};
  control.setArmed(true);
  control.setLinearSpeed(0.8);
  control.setAngularSpeed(-0.2);
  VERIFY_MIR(control.armed() && control.linearSpeed() == kMaxLinearMps && control.angularSpeed() == 0.05);
  VERIFY_MIR(control.press("forward") && adapter->commands.size() == 1);
  VERIFY_MIR(adapter->commands.back().name == "/mur620d/cmd_vel" && !adapter->commands.back().stamped &&
    adapter->commands.back().linear == kMaxLinearMps);
  waitForEvents(120);
  VERIFY_MIR(adapter->commands.size() >= 2 && adapter->commands.back().linear == kMaxLinearMps);
  adapter->topics["/mur620d/cmd_vel_stamped"] = {kTwistStampedType, 1};
  adapter->topics.erase("/mur620d/cmd_vel");
  waitForEvents(120);
  VERIFY_MIR(!control.holding() && !control.armed() &&
    adapter->commands.back().name == "/mur620d/cmd_vel" && adapter->commands.back().linear == 0.0);
  control.setArmed(true);

  adapter->topics.erase("/mur620d/cmd_vel");
  adapter->topics["/mur620d/cmd_vel_stamped"] = {kTwistStampedType, 1};
  VERIFY_MIR(control.press("left") && adapter->commands.back().stamped &&
    adapter->commands.back().name == "/mur620d/cmd_vel_stamped");
  control.setArmed(false);
  VERIFY_MIR(!control.holding() && adapter->commands.back().angular == 0.0);

  control.setArmed(true);
  adapter->topics["/mur620d/cmd_vel_stamped"].subscribers = 0;
  VERIFY_MIR(!control.press("forward") && !control.holding());
  adapter->topics["/mur620d/cmd_vel_stamped"].subscribers = 1;
  adapter->topics["/mur620d/cmd_vel_stamped"].type = "geometry_msgs/msg/Twist";
  VERIFY_MIR(!control.press("forward"));
  adapter->topics["/mur620d/cmd_vel_stamped"].type = kTwistStampedType;
  VERIFY_MIR(!control.press("invalid"));
  VERIFY_MIR(control.press("forward"));
  control.setContext("mur620b", "Hardware", "lab-host", true);
  VERIFY_MIR(!control.armed() && !control.holding() && adapter->commands.back().linear == 0.0);
  adapter->topics["/mur620d/cmd_vel_stamped"].subscribers = 1;
  adapter->services["/mur620b/RGB_control/rainbow_start"] = kTriggerType;
  adapter->services["/mur620b/RGB_control/rainbow_stop"] = kTriggerType;
  adapter->services["/mur620b/RGB_control/match_color"] = kTriggerType;
  adapter->services["/mur620b/RGB_control/solid_color"] = kColorType;
  const auto prior_calls = adapter->service_calls;
  control.rainbowStart();
  VERIFY_MIR(adapter->service_calls == prior_calls + 1 && control.lightStatus().contains("Applied"));
  control.rainbowStop();
  control.matchColor();
  control.setSolidColor(-2, 300, 128);
  VERIFY_MIR(adapter->service_calls == prior_calls + 4 &&
    control.lightStatus().contains("Applied") && adapter->commands.back().red == 0 &&
    adapter->commands.back().green == 255 && adapter->commands.back().blue == 128);
  adapter->trigger_success = false;
  adapter->trigger_message = "lamp controller rejected effect";
  control.matchColor();
  VERIFY_MIR(control.lightStatus().contains("Failed") && control.lightStatus().contains("rejected effect"));
  adapter->services["/mur620b/RGB_control/rainbow_start"] = "std_srvs/srv/SetBool";
  const auto unavailable_count = adapter->service_calls;
  control.rainbowStart();
  VERIFY_MIR(adapter->service_calls == unavailable_count && control.lightStatus().contains("Unavailable"));

  control.setContext("bad;namespace", "Hardware", "lab-host", true);
  control.setArmed(true);
  const auto commands_before_invalid_namespace = adapter->commands.size();
  VERIFY_MIR(!control.armed() && !control.press("forward") &&
    adapter->commands.size() == commands_before_invalid_namespace);
  control.setContext("mur620b", "Simulation", "localhost", true);
  const auto before_disallowed = adapter->commands.size();
  control.setArmed(true);
  VERIFY_MIR(!control.armed() && !control.press("forward") && adapter->commands.size() == before_disallowed);
  control.setContext("mur620b", "Hardware", "lab-host", true);
  control.setArmed(true);
  adapter->graph_connected = false;
  VERIFY_MIR(!control.press("forward"));
  control.rainbowStop();
  VERIFY_MIR(control.lightStatus().contains("Unavailable"));
#undef VERIFY_MIR
  return true;
}
