#include "simulation_base_jog.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

namespace
{
constexpr double kMaxLinearMps = 0.15;
constexpr double kMaxAngularRadps = 0.4;
constexpr auto kCommandLifespan = std::chrono::milliseconds(250);
const std::string kTwistStampedType = "geometry_msgs/msg/TwistStamped";

std::string topicFor(const QString & robot)
{
  return "/" + robot.toStdString() + "/mobile_base_controller/cmd_vel";
}

std::string frameFor(const QString & robot)
{
  return robot.toStdString() + "/base_link";
}

class FakeSimBaseJogAdapter final : public SimBaseJogAdapter
{
public:
  bool connected() const override { return graph_connected; }
  SimBaseEndpoint inspect(const std::string &) override { return {type, subscribers}; }
  void publish(const std::string & topic, const std::string &, double linear, double angular) override
  {
    commands.push_back({topic, linear, angular});
  }

  bool graph_connected{true};
  std::string type{kTwistStampedType};
  size_t subscribers{1};
  struct Command { std::string topic; double linear; double angular; };
  std::vector<Command> commands;
};

void waitForEvents(int milliseconds)
{
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec();
}
}

RosSimBaseJogAdapter::RosSimBaseJogAdapter(rclcpp::Node::SharedPtr node)
: node_(std::move(node))
{
}

void RosSimBaseJogAdapter::setNode(rclcpp::Node::SharedPtr node)
{
  node_ = std::move(node);
  if (!node_) publishers_.clear();
}

bool RosSimBaseJogAdapter::connected() const
{
  return node_ && rclcpp::ok(node_->get_node_base_interface()->get_context());
}

SimBaseEndpoint RosSimBaseJogAdapter::inspect(const std::string & topic)
{
  if (!connected()) return {};
  const auto graph = node_->get_topic_names_and_types();
  const auto found = graph.find(topic);
  if (found == graph.end() || found->second.size() != 1) return {};
  return {found->second.front(), static_cast<size_t>(node_->count_subscribers(topic))};
}

void RosSimBaseJogAdapter::publish(const std::string & topic, const std::string & frame,
  double linear_x, double angular_z)
{
  if (!connected()) return;
  auto publisher = publishers_.find(topic);
  if (publisher == publishers_.end()) {
    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).lifespan(rclcpp::Duration(kCommandLifespan));
    auto created = node_->create_publisher<geometry_msgs::msg::TwistStamped>(topic, qos);
    publisher = publishers_.emplace(topic, std::move(created)).first;
  }
  geometry_msgs::msg::TwistStamped message;
  message.header.stamp = node_->now();
  message.header.frame_id = frame;
  message.twist.linear.x = linear_x;
  message.twist.angular.z = angular_z;
  publisher->second->publish(message);
}

SimulationBaseJog::SimulationBaseJog(std::shared_ptr<SimBaseJogAdapter> adapter, QObject * parent)
: QObject(parent), adapter_(std::move(adapter))
{
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, &SimulationBaseJog::tick);
}

void SimulationBaseJog::setContext(
  const QString & robot, const QString & mode, const QString & session_key, bool connected)
{
  const auto selected = robot.trimmed();
  const bool valid_robot = !selected.isEmpty() && !selected.contains('/');
  if (selected != robot_ || mode != context_mode_ || session_key != context_key_ ||
    connected != context_connected_) {
    stop("session context changed");
    enabled_ = false;
    robot_ = valid_robot ? selected : QString();
    context_mode_ = mode;
    context_key_ = session_key;
    context_connected_ = connected;
    setStatus("Simulation jog disabled · confirm the new session before enabling");
    emit changed();
  }
  if (!valid_robot) robot_.clear();
}

void SimulationBaseJog::setEnabled(bool enabled)
{
  const bool can_enable = !robot_.isEmpty() && context_mode_ == "Simulation" &&
    context_connected_ && adapter_ && adapter_->connected();
  if (enabled && !can_enable) {
    enabled_ = false;
    setStatus("Select one connected simulation robot before enabling jog");
  } else {
    enabled_ = enabled;
    setStatus(enabled ? "Enabled · hold a direction to jog" : "Simulation jog disabled");
  }
  if (!enabled_) stop("disabled");
  emit changed();
}

void SimulationBaseJog::setLinearSpeed(double speed)
{
  if (holding_) stop("speed changed");
  linear_speed_ = std::isfinite(speed) ? std::clamp(speed, 0.01, kMaxLinearMps) : 0.01;
  emit changed();
}

void SimulationBaseJog::setAngularSpeed(double speed)
{
  if (holding_) stop("speed changed");
  angular_speed_ = std::isfinite(speed) ? std::clamp(speed, 0.05, kMaxAngularRadps) : 0.05;
  emit changed();
}

bool SimulationBaseJog::press(const QString & direction)
{
  if (!enabled_ || !context_connected_ || context_mode_ != "Simulation" || robot_.isEmpty()) {
    setStatus("Jog is disabled; explicitly enable it for one connected simulation robot");
    return false;
  }
  if (holding_) stop("direction changed");
  if (direction == "forward") { linear_x_ = linear_speed_; angular_z_ = 0.0; }
  else if (direction == "backward") { linear_x_ = -linear_speed_; angular_z_ = 0.0; }
  else if (direction == "left") { linear_x_ = 0.0; angular_z_ = angular_speed_; }
  else if (direction == "right") { linear_x_ = 0.0; angular_z_ = -angular_speed_; }
  else return false;
  if (!send(linear_x_, angular_z_)) {
    clearHold();
    setStatus("Unavailable · expected TwistStamped subscriber on " + QString::fromStdString(topicFor(robot_)));
    return false;
  }
  holding_ = true;
  timer_.start();
  setStatus(QString("Jogging · linear ≤ %1 m/s · angular ≤ %2 rad/s")
    .arg(kMaxLinearMps).arg(kMaxAngularRadps));
  emit changed();
  return true;
}

void SimulationBaseJog::release()
{
  stop("released");
}

void SimulationBaseJog::stop(const QString & reason)
{
  if (holding_) send(0.0, 0.0);
  clearHold();
  if (!reason.isEmpty() && enabled_) setStatus("Stopped · " + reason);
  emit changed();
}

void SimulationBaseJog::tick()
{
  if (!holding_) { timer_.stop(); return; }
  if (!enabled_ || !context_connected_ || context_mode_ != "Simulation" ||
    !adapter_ || !adapter_->connected() || !send(linear_x_, angular_z_)) {
    clearHold();
    setStatus("Stopped · simulation graph or verified controller endpoint was lost");
    emit changed();
  }
}

bool SimulationBaseJog::send(double linear_x, double angular_z)
{
  if (!adapter_ || !adapter_->connected() || robot_.isEmpty()) return false;
  try {
    const auto topic = topicFor(robot_);
    const auto endpoint = adapter_->inspect(topic);
    if (endpoint.type != kTwistStampedType || endpoint.subscribers < 1) return false;
    adapter_->publish(topic, frameFor(robot_), linear_x, angular_z);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

void SimulationBaseJog::clearHold()
{
  timer_.stop();
  holding_ = false;
  linear_x_ = 0.0;
  angular_z_ = 0.0;
  emit changed();
}

void SimulationBaseJog::setStatus(const QString & status)
{
  status_ = status;
  emit changed();
}

bool run_sim_base_jog_contract_test()
{
  auto adapter = std::make_shared<FakeSimBaseJogAdapter>();
  SimulationBaseJog jog(adapter);
  jog.setContext("", "Simulation", "Local simulation|localhost", true);
  jog.setEnabled(true);
  if (jog.enabled() || jog.press("forward") || !adapter->commands.empty()) return false;
  jog.setContext("mur620a", "Simulation", "Local simulation|localhost", true);
  if (jog.press("forward") || !adapter->commands.empty()) return false;
  jog.setEnabled(true);
  jog.setLinearSpeed(10.0);
  jog.setAngularSpeed(-1.0);
  if (jog.linearSpeed() != kMaxLinearMps || jog.angularSpeed() != 0.05) return false;
  jog.setAngularSpeed(10.0);
  if (jog.angularSpeed() != kMaxAngularRadps) return false;
  jog.setLinearSpeed(std::numeric_limits<double>::quiet_NaN());
  jog.setAngularSpeed(std::numeric_limits<double>::infinity());
  if (jog.linearSpeed() != 0.01 || jog.angularSpeed() != 0.05) return false;
  jog.setLinearSpeed(10.0);
  if (!jog.press("forward") || adapter->commands.size() != 1) return false;
  const auto topic = "/mur620a/mobile_base_controller/cmd_vel";
  if (adapter->commands.back().topic != topic || adapter->commands.back().linear != kMaxLinearMps) return false;
  waitForEvents(120);
  if (adapter->commands.size() < 2 || adapter->commands.back().linear != kMaxLinearMps) return false;
  jog.release();
  if (jog.holding() || adapter->commands.back().linear != 0.0 || adapter->commands.back().angular != 0.0) return false;
  const auto stopped_count = adapter->commands.size();
  waitForEvents(150);
  if (adapter->commands.size() != stopped_count) return false;

  if (!jog.press("left")) return false;
  jog.stop("focus lost");
  if (jog.holding() || adapter->commands.back().angular != 0.0) return false;
  if (!jog.press("forward")) return false;
  jog.setContext("mur620b", "Simulation", "Local simulation|localhost", true);
  if (jog.enabled() || jog.holding() || adapter->commands.back().linear != 0.0) return false;

  jog.setContext("mur620a", "Simulation", "Lab simulation|localhost", true);
  jog.setEnabled(true);
  adapter->type = "geometry_msgs/msg/Twist";
  if (jog.press("forward")) return false;
  adapter->type = kTwistStampedType;
  adapter->subscribers = 0;
  if (jog.press("forward")) return false;
  adapter->subscribers = 1;
  adapter->graph_connected = false;
  if (jog.press("forward")) return false;
  adapter->graph_connected = true;
  jog.setContext("mur620a", "Hardware", "Lab simulation|localhost", true);
  if (jog.enabled() || jog.press("forward")) return false;
  return true;
}
