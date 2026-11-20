#include "ur_arm_jog.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <controller_manager_msgs/srv/list_controllers.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <ur_dashboard_msgs/msg/robot_mode.hpp>
#include <ur_dashboard_msgs/msg/safety_mode.hpp>
#include <ur_dashboard_msgs/srv/get_loaded_program.hpp>
#include <ur_dashboard_msgs/srv/is_in_remote_control.hpp>
#include <ur_dashboard_msgs/srv/is_program_running.hpp>

namespace
{
constexpr auto kCommandLifespan = std::chrono::milliseconds(250);
constexpr double kMaxLinearMps = 0.05;
constexpr double kMaxAngularRadps = 0.20;
const std::string kTwistStampedType = "geometry_msgs/msg/TwistStamped";

std::string arm_namespace(const std::string & robot, const std::string & side)
{
  return "/" + robot + "/UR10_" + side;
}

std::string command_topic(const std::string & robot, const std::string & side)
{
  return arm_namespace(robot, side) +
    "/integrated_cartesian_admittance_controller/equilibrium_twist_cmd";
}

class FakeUrArmJogAdapter final : public UrArmJogAdapter
{
public:
  bool connected() const override { return graph_connected; }
  UrArmReadiness inspect(const std::string &, const std::string &) override
  {
    auto result = readiness;
    result.connected = graph_connected;
    return result;
  }
  void publish(const std::string & robot, const std::string & side,
    const std::array<double, 6> & twist) override
  {
    commands.push_back({robot, side, twist});
  }

  struct Command
  {
    std::string robot;
    std::string side;
    std::array<double, 6> twist;
  };
  bool graph_connected{true};
  UrArmReadiness readiness;

  FakeUrArmJogAdapter()
  {
    readiness.external_control_detail = "program state missing";
    readiness.remote_control_detail = "remote mode missing";
    readiness.reverse_interface_detail = "robot mode missing";
    readiness.controller_detail = "controller status missing";
    readiness.joint_state_detail = "joint state missing";
    readiness.command_endpoint_detail = "command subscriber missing";
  }
  std::vector<Command> commands;
};

void waitForEvents(int milliseconds)
{
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec();
}
}

bool UrArmReadiness::ready() const
{
  return connected && external_control && reverse_interface &&
    remote_control && cartesian_controller_active && joint_state_fresh && command_endpoint_ready;
}

QString UrArmReadiness::summary(const QString & side) const
{
  return QString("%1 readiness\nExternal Control program: %2\nRemote control: %3\nReverse interface/driver: %4\nCartesian controller: %5\nJoint data: %6\nCommand endpoint: %7")
    .arg(side,
      connected ? external_control_detail : "ROS session disconnected",
      remote_control_detail,
      reverse_interface_detail,
      controller_detail,
      joint_state_detail,
      command_endpoint_detail);
}

UrArmJog::UrArmJog(std::shared_ptr<UrArmJogAdapter> adapter, QObject * parent)
: QObject(parent), adapter_(std::move(adapter))
{
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, &UrArmJog::tick);
}

void UrArmJog::setContext(
  const QString & robot, const QString & mode, const QString & session_key, bool connected)
{
  const auto selected = robot.trimmed();
  const bool valid_robot = !selected.isEmpty() && !selected.contains('/');
  if (selected != robot_ || mode != mode_ || session_key != context_key_ ||
    connected != context_connected_) {
    stop("session context changed");
    armed_ = false;
    robot_ = valid_robot ? selected : QString();
    mode_ = mode;
    context_key_ = session_key;
    context_connected_ = connected;
    refresh();
  }
  if (!valid_robot) robot_.clear();
}

void UrArmJog::selectArm(const QString & side)
{
  const auto normalized = side.trimmed().toLower();
  if (normalized != "l" && normalized != "r") {
    setStatus("Choose left or right UR arm");
    return;
  }
  if (normalized == side_) return;
  stop("arm selection changed");
  armed_ = false;
  side_ = normalized;
  refresh();
}

void UrArmJog::setArmed(bool armed)
{
  refresh();
  const bool can_arm = armed && mode_ == "Hardware" && context_connected_ &&
    !robot_.isEmpty() && readiness_ok_;
  if (armed && !can_arm) {
    armed_ = false;
    setStatus("Cannot arm · selected hardware arm is not ready");
  } else {
    if (!armed) stop("disarmed");
    armed_ = can_arm;
    setStatus(armed_ ? "Armed · hold a direction to jog selected UR arm" : "Arm jog disarmed");
  }
  emit changed();
}

void UrArmJog::setLinearSpeed(double speed)
{
  if (holding_) stop("speed changed");
  linear_speed_ = std::isfinite(speed) ? std::clamp(speed, 0.001, kMaxLinearMps) : 0.001;
  emit changed();
}

void UrArmJog::setAngularSpeed(double speed)
{
  if (holding_) stop("speed changed");
  angular_speed_ = std::isfinite(speed) ? std::clamp(speed, 0.01, kMaxAngularRadps) : 0.01;
  emit changed();
}

void UrArmJog::refresh()
{
  UrArmReadiness state;
  if (adapter_ && adapter_->connected() && mode_ == "Hardware" && context_connected_ &&
    !robot_.isEmpty()) {
    try {
      state = adapter_->inspect(robot_.toStdString(), side_.toStdString());
    } catch (const std::exception &) {
      state = {};
    }
  }
  readiness_ok_ = state.ready();
  readiness_ = state.summary(selectedArm());
  if (holding_ && !readiness_ok_) stop("arm readiness lost");
  if (!readiness_ok_ && armed_) {
    armed_ = false;
    setStatus("Disarmed · arm readiness lost");
  }
  emit changed();
}

bool UrArmJog::press(const QString & axis)
{
  if (!armed_ || mode_ != "Hardware" || !context_connected_ || robot_.isEmpty()) {
    setStatus("Jog disabled · explicitly arm a ready UR arm in Hardware mode");
    return false;
  }
  refresh();
  if (!readiness_ok_) {
    setStatus("Jog blocked · External Control, reverse interface, controller and fresh joint data are required");
    return false;
  }
  if (holding_) {
    send({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    clearHold();
  }
  twist_.fill(0.0);
  const bool linear = axis.size() == 3 && axis.startsWith("t");
  const bool angular = axis.size() == 3 && axis.startsWith("r");
  if (!linear && !angular) return false;
  const QChar coordinate = axis.at(1).toLower();
  const int index = coordinate == 'x' ? (linear ? 0 : 3) :
    coordinate == 'y' ? (linear ? 1 : 4) : coordinate == 'z' ? (linear ? 2 : 5) : -1;
  if (index < 0 || (axis.endsWith('-') == axis.endsWith('+'))) return false;
  const double speed = linear ? linear_speed_ : angular_speed_;
  twist_[static_cast<size_t>(index)] = axis.endsWith('-') ? -speed : speed;
  if (!send(twist_)) {
    twist_.fill(0.0);
    setStatus("Jog blocked · TwistStamped subscriber unavailable on the selected arm controller");
    return false;
  }
  holding_ = true;
  timer_.start();
  setStatus(QString("Jogging %1 · %2 · ≤ %3 m/s / %4 rad/s")
    .arg(selectedArm(), axis).arg(kMaxLinearMps).arg(kMaxAngularRadps));
  emit changed();
  return true;
}

void UrArmJog::release()
{
  if (holding_) send({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
  clearHold();
  if (armed_) setStatus("Armed · released; selected arm stopped");
  emit changed();
}

void UrArmJog::stop(const QString & reason)
{
  if (holding_) send({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
  clearHold();
  armed_ = false;
  if (!reason.isEmpty()) setStatus("Stopped · " + reason + " · disarmed");
  emit changed();
}

void UrArmJog::tick()
{
  if (!holding_) { timer_.stop(); return; }
  refresh();
  if (!armed_ || !readiness_ok_ || !send(twist_)) {
    clearHold();
    setStatus("Stopped · arm readiness or TwistStamped endpoint was lost");
    emit changed();
  }
}

bool UrArmJog::send(const std::array<double, 6> & twist)
{
  if (!adapter_ || !adapter_->connected() || robot_.isEmpty()) return false;
  try {
    const auto endpoint = adapter_->inspect(robot_.toStdString(), side_.toStdString());
    const bool zero_command = std::all_of(twist.begin(), twist.end(),
      [](double value) { return std::abs(value) <= 1e-12; });
    if (!endpoint.command_endpoint_ready || (!zero_command && !endpoint.ready())) return false;
    adapter_->publish(robot_.toStdString(), side_.toStdString(), twist);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

void UrArmJog::clearHold()
{
  timer_.stop();
  holding_ = false;
  twist_.fill(0.0);
  emit changed();
}

void UrArmJog::setStatus(const QString & status)
{
  status_ = status;
  emit changed();
}

bool run_ur_arm_jog_contract_test()
{
  auto adapter = std::make_shared<FakeUrArmJogAdapter>();
  UrArmJog jog(adapter);
  jog.setContext("mur620b", "Hardware", "Lab|mur-host|87", true);
  jog.setArmed(true);
  if (jog.ready() || jog.armed()) return false;

  adapter->readiness.external_control = true;
  adapter->readiness.remote_control = true;
  adapter->readiness.reverse_interface = true;
  adapter->readiness.cartesian_controller_active = true;
  adapter->readiness.command_endpoint_ready = true;
  adapter->readiness.joint_state_fresh = false;
  jog.refresh();
  jog.setArmed(true);
  if (jog.ready() || jog.armed() || jog.press("tx+")) return false;

  adapter->readiness.remote_control = false;
  adapter->readiness.command_endpoint_ready = false;
  adapter->readiness.joint_state_fresh = true;
  jog.refresh();
  if (jog.ready()) return false;
  adapter->readiness.remote_control = true;
  jog.refresh();
  if (jog.ready()) return false;
  adapter->readiness.command_endpoint_ready = true;
  jog.refresh();
  if (!jog.ready()) return false;
  adapter->readiness.command_endpoint_ready = false;
  jog.refresh();
  jog.setArmed(true);
  if (jog.ready() || jog.armed()) return false;
  adapter->readiness.command_endpoint_ready = true;
  jog.refresh();
  if (!jog.ready()) return false;
  jog.setLinearSpeed(100.0);
  jog.setAngularSpeed(-5.0);
  if (jog.linearSpeed() != kMaxLinearMps || jog.angularSpeed() != 0.01) return false;
  jog.setAngularSpeed(10.0);
  if (jog.angularSpeed() != kMaxAngularRadps) return false;
  jog.setAngularSpeed(0.01);
  jog.setArmed(true);
  if (!jog.armed() || !jog.press("ty-")) return false;
  const auto & first = adapter->commands.back();
  if (first.robot != "mur620b" || first.side != "r" || first.twist[1] != -kMaxLinearMps) return false;
  waitForEvents(120);
  if (adapter->commands.size() < 2 || adapter->commands.back().twist[1] != -kMaxLinearMps) return false;
  jog.release();
  if (jog.holding() || adapter->commands.back().twist != std::array<double, 6>{}) return false;

  if (!jog.press("rz+")) return false;
  if (adapter->commands.back().twist[5] != 0.01) return false;
  jog.stop("focus lost");
  if (jog.holding() || adapter->commands.back().twist != std::array<double, 6>{}) return false;

  jog.setArmed(true);
  if (!jog.press("tx+")) return false;
  const auto count_before_arm_switch = adapter->commands.size();
  jog.selectArm("l");
  if (jog.armed() || jog.holding() || adapter->commands.size() != count_before_arm_switch + 1 ||
    adapter->commands.back().side != "r" || adapter->commands.back().twist != std::array<double, 6>{}) return false;
  jog.setArmed(true);
  if (!jog.armed()) return false;
  if (!jog.press("tx-")) return false;

  jog.setContext("mur620b", "Simulation", "Lab|mur-host|87", true);
  if (jog.armed() || jog.holding() || jog.press("tx+") ||
    adapter->commands.back().side != "l" || adapter->commands.back().twist != std::array<double, 6>{}) return false;
  jog.setContext("mur620b", "Hardware", "Lab|mur-host|87", true);
  adapter->readiness.joint_state_fresh = false;
  jog.refresh();
  jog.setArmed(true);
  if (jog.armed() || jog.press("tx+")) return false;
  adapter->readiness.joint_state_fresh = true;
  jog.refresh();
  jog.setArmed(true);
  if (!jog.press("tx+")) return false;
  adapter->readiness.joint_state_fresh = false;
  waitForEvents(120);
  if (jog.armed() || jog.holding() || adapter->commands.back().twist != std::array<double, 6>{}) return false;
  return true;
}

class RosUrArmJogAdapter::State
{
public:
  struct Arm
  {
    bool program_known{false};
    bool program_running{false};
    std::chrono::steady_clock::time_point program_received{};
    bool remote_control_known{false};
    bool remote_control{false};
    std::chrono::steady_clock::time_point remote_control_received{};
    bool robot_running{false};
    bool robot_mode_known{false};
    std::chrono::steady_clock::time_point robot_mode_received{};
    bool safe_mode{false};
    bool safety_known{false};
    std::chrono::steady_clock::time_point safety_received{};
    bool joint_state_known{false};
    size_t joint_count{0};
    std::chrono::steady_clock::time_point joint_received{};
    bool controller_known{false};
    bool controller_active{false};
    QString controller_state{"waiting for controller manager"};
    std::chrono::steady_clock::time_point controller_received{};
    bool program_pending{false};
    bool remote_control_pending{false};
    bool loaded_program_known{false};
    QString loaded_program;
    bool loaded_program_pending{false};
    bool controllers_pending{false};
    std::chrono::steady_clock::time_point program_requested{};
    std::chrono::steady_clock::time_point remote_control_requested{};
    std::chrono::steady_clock::time_point loaded_program_received{};
    std::chrono::steady_clock::time_point loaded_program_requested{};
    std::chrono::steady_clock::time_point controllers_requested{};
    rclcpp::Client<ur_dashboard_msgs::srv::IsProgramRunning>::SharedPtr program_client;
    rclcpp::Client<ur_dashboard_msgs::srv::IsInRemoteControl>::SharedPtr remote_control_client;
    rclcpp::Client<ur_dashboard_msgs::srv::GetLoadedProgram>::SharedPtr loaded_program_client;
    rclcpp::Client<controller_manager_msgs::srv::ListControllers>::SharedPtr controllers_client;
    rclcpp::Subscription<ur_dashboard_msgs::msg::RobotMode>::SharedPtr robot_mode_sub;
    rclcpp::Subscription<ur_dashboard_msgs::msg::SafetyMode>::SharedPtr safety_sub;
  };

  std::map<std::string, Arm> arms;
  std::map<std::string, rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr>
    joint_states_subs;

  void ensureArm(const rclcpp::Node::SharedPtr & node,
    const std::string & robot, const std::string & side)
  {
    const auto key = arm_namespace(robot, side);
    auto & arm = arms[key];
    if (!arm.robot_mode_sub) {
      const auto robot_mode_topic = key + "/io_and_status_controller/robot_mode";
      arm.robot_mode_sub = node->create_subscription<ur_dashboard_msgs::msg::RobotMode>(
        robot_mode_topic, rclcpp::SensorDataQoS(),
        [&arm](ur_dashboard_msgs::msg::RobotMode::ConstSharedPtr message) {
          arm.robot_mode_known = true;
          arm.robot_running = message->mode == ur_dashboard_msgs::msg::RobotMode::RUNNING;
          arm.robot_mode_received = std::chrono::steady_clock::now();
        });
      const auto safety_topic = key + "/io_and_status_controller/safety_mode";
      arm.safety_sub = node->create_subscription<ur_dashboard_msgs::msg::SafetyMode>(
        safety_topic, rclcpp::SensorDataQoS(),
        [&arm](ur_dashboard_msgs::msg::SafetyMode::ConstSharedPtr message) {
          arm.safety_known = true;
          arm.safe_mode = message->mode == ur_dashboard_msgs::msg::SafetyMode::NORMAL ||
            message->mode == ur_dashboard_msgs::msg::SafetyMode::REDUCED;
          arm.safety_received = std::chrono::steady_clock::now();
        });
      arm.program_client = node->create_client<ur_dashboard_msgs::srv::IsProgramRunning>(
        key + "/dashboard_client/program_running");
      arm.remote_control_client = node->create_client<ur_dashboard_msgs::srv::IsInRemoteControl>(
        key + "/dashboard_client/is_in_remote_control");
      arm.loaded_program_client = node->create_client<ur_dashboard_msgs::srv::GetLoadedProgram>(
        key + "/dashboard_client/get_loaded_program");
      arm.controllers_client = node->create_client<controller_manager_msgs::srv::ListControllers>(
        key + "/controller_manager/list_controllers");
    }
    if (!joint_states_subs.count(robot)) {
      joint_states_subs[robot] = node->create_subscription<sensor_msgs::msg::JointState>(
        "/" + robot + "/joint_states", rclcpp::SensorDataQoS(), [this, robot](sensor_msgs::msg::JointState::ConstSharedPtr message) {
          const auto now = std::chrono::steady_clock::now();
          for (auto & entry : arms) {
            const auto key = entry.first;
            if (key.rfind("/" + robot + "/UR10_", 0) != 0) continue;
            const auto arm_prefix = key.substr(key.rfind("/UR10_") + 1) + "/";
            size_t count = 0;
            const std::vector<std::string> arm_joint_suffixes = {
              "shoulder_pan_joint", "shoulder_lift_joint", "elbow_joint",
              "wrist_1_joint", "wrist_2_joint", "wrist_3_joint"};
            for (const auto & suffix : arm_joint_suffixes) {
              const auto expected = arm_prefix + suffix;
              for (size_t index = 0; index < message->name.size(); ++index) {
                if (message->name[index] == expected && index < message->position.size() &&
                  std::isfinite(message->position[index])) {
                  ++count;
                  break;
                }
              }
            }
            auto & watched = entry.second;
            if (count >= 6) {
              watched.joint_state_known = true;
              watched.joint_count = count;
              watched.joint_received = now;
            }
          }
        });
    }
  }

  void requestStatus(Arm & arm)
  {
    const auto now = std::chrono::steady_clock::now();
    if (arm.program_client && arm.program_client->service_is_ready() && !arm.program_pending &&
      (arm.program_requested == std::chrono::steady_clock::time_point{} ||
       now - arm.program_requested >= std::chrono::seconds(1))) {
      arm.program_pending = true;
      arm.program_requested = now;
      auto request = std::make_shared<ur_dashboard_msgs::srv::IsProgramRunning::Request>();
      arm.program_client->async_send_request(request, [&arm](
        rclcpp::Client<ur_dashboard_msgs::srv::IsProgramRunning>::SharedFuture future) {
          arm.program_pending = false;
          try {
            const auto response = future.get();
            arm.program_known = response->success;
            arm.program_running = response->success && response->program_running;
          } catch (const std::exception &) {
            arm.program_known = false;
            arm.program_running = false;
          }
          arm.program_received = std::chrono::steady_clock::now();
        });
    }
    if (arm.loaded_program_client && arm.loaded_program_client->service_is_ready() &&
      !arm.loaded_program_pending &&
      (arm.loaded_program_requested == std::chrono::steady_clock::time_point{} ||
       now - arm.loaded_program_requested >= std::chrono::seconds(1))) {
      arm.loaded_program_pending = true;
      arm.loaded_program_requested = now;
      auto request = std::make_shared<ur_dashboard_msgs::srv::GetLoadedProgram::Request>();
      arm.loaded_program_client->async_send_request(request, [&arm](
        rclcpp::Client<ur_dashboard_msgs::srv::GetLoadedProgram>::SharedFuture future) {
          arm.loaded_program_pending = false;
          try {
            const auto response = future.get();
            arm.loaded_program_known = response->success;
            arm.loaded_program = QString::fromStdString(response->program_name);
          } catch (const std::exception &) {
            arm.loaded_program_known = false;
            arm.loaded_program.clear();
          }
          arm.loaded_program_received = std::chrono::steady_clock::now();
        });
    }
    if (arm.remote_control_client && arm.remote_control_client->service_is_ready() &&
      !arm.remote_control_pending &&
      (arm.remote_control_requested == std::chrono::steady_clock::time_point{} ||
       now - arm.remote_control_requested >= std::chrono::seconds(1))) {
      arm.remote_control_pending = true;
      arm.remote_control_requested = now;
      auto request = std::make_shared<ur_dashboard_msgs::srv::IsInRemoteControl::Request>();
      arm.remote_control_client->async_send_request(request, [&arm](
        rclcpp::Client<ur_dashboard_msgs::srv::IsInRemoteControl>::SharedFuture future) {
          arm.remote_control_pending = false;
          try {
            const auto response = future.get();
            arm.remote_control_known = response->success;
            arm.remote_control = response->success && response->remote_control;
          } catch (const std::exception &) {
            arm.remote_control_known = false;
            arm.remote_control = false;
          }
          arm.remote_control_received = std::chrono::steady_clock::now();
        });
    }
    if (arm.controllers_client && arm.controllers_client->service_is_ready() &&
      !arm.controllers_pending &&
      (arm.controllers_requested == std::chrono::steady_clock::time_point{} ||
       now - arm.controllers_requested >= std::chrono::seconds(1))) {
      arm.controllers_pending = true;
      arm.controllers_requested = now;
      auto request = std::make_shared<controller_manager_msgs::srv::ListControllers::Request>();
      arm.controllers_client->async_send_request(request, [&arm](
        rclcpp::Client<controller_manager_msgs::srv::ListControllers>::SharedFuture future) {
          arm.controllers_pending = false;
          try {
            const auto response = future.get();
            const auto found = std::find_if(response->controller.begin(), response->controller.end(),
              [](const auto & controller) {
                return controller.name == "integrated_cartesian_admittance_controller";
              });
            arm.controller_known = found != response->controller.end();
            arm.controller_active = arm.controller_known && found->state == "active";
            arm.controller_state = !arm.controller_known ? "Cartesian controller is not loaded" :
              QString::fromStdString(found->state);
          } catch (const std::exception & error) {
            arm.controller_known = false;
            arm.controller_active = false;
            arm.controller_state = QString("controller query failed: %1").arg(error.what());
          }
          arm.controller_received = std::chrono::steady_clock::now();
        });
    }
  }
};

RosUrArmJogAdapter::RosUrArmJogAdapter(rclcpp::Node::SharedPtr node)
: node_(std::move(node)), state_(std::make_unique<State>())
{
}

RosUrArmJogAdapter::~RosUrArmJogAdapter() = default;

void RosUrArmJogAdapter::setNode(rclcpp::Node::SharedPtr node)
{
  node_ = std::move(node);
  state_ = std::make_unique<State>();
  if (!node_) publishers_.clear();
}

bool RosUrArmJogAdapter::connected() const
{
  return node_ && rclcpp::ok(node_->get_node_base_interface()->get_context());
}

UrArmReadiness RosUrArmJogAdapter::inspect(const std::string & robot, const std::string & side)
{
  UrArmReadiness result;
  result.connected = connected();
  if (!connected()) return result;
  state_->ensureArm(node_, robot, side);
  auto & arm = state_->arms[arm_namespace(robot, side)];
  state_->requestStatus(arm);
  const auto now = std::chrono::steady_clock::now();
  const auto age_seconds = [&now](const auto & received) {
      return received == std::chrono::steady_clock::time_point{} ? -1.0 :
             std::chrono::duration<double>(now - received).count();
    };
  const double program_age = age_seconds(arm.program_received);
  const bool program_fresh = program_age >= 0.0 && program_age <= 3.0;
  const double loaded_age = age_seconds(arm.loaded_program_received);
  const bool loaded_fresh = loaded_age >= 0.0 && loaded_age <= 3.0;
  const bool external_program = arm.loaded_program_known &&
    arm.loaded_program.endsWith("external_control.urp", Qt::CaseInsensitive);
  result.external_control = arm.program_known && arm.program_running && program_fresh &&
    loaded_fresh && external_program;
  result.external_control_detail = !program_fresh || !loaded_fresh ? "program status missing or stale" :
    !arm.program_running ? "program is stopped" :
    !external_program ? "external_control.urp is not loaded" : "external_control.urp is running";

  const double remote_age = age_seconds(arm.remote_control_received);
  const bool remote_fresh = remote_age >= 0.0 && remote_age <= 3.0;
  result.remote_control = arm.remote_control_known && arm.remote_control && remote_fresh;
  result.remote_control_detail = !remote_fresh ? "remote-control status missing or stale" :
    arm.remote_control ? "enabled" : "disabled";

  const double robot_age = age_seconds(arm.robot_mode_received);
  const bool robot_fresh = arm.robot_mode_known && robot_age >= 0.0 && robot_age <= 2.0;
  const double safety_age = age_seconds(arm.safety_received);
  const bool safety_fresh = arm.safety_known && safety_age >= 0.0 && safety_age <= 2.0;
  result.reverse_interface = result.external_control && robot_fresh && arm.robot_running &&
    safety_fresh && arm.safe_mode;
  result.reverse_interface_detail = !robot_fresh ? "robot mode not reported or stale" :
    !arm.robot_running ? "robot is not in RUNNING mode" :
    !safety_fresh ? "safety mode not reported or stale" :
    !arm.safe_mode ? "safety mode blocks motion" :
    "driver mode stream current; reverse connection inferred";

  const double controller_age = age_seconds(arm.controller_received);
  const bool controller_fresh = controller_age >= 0.0 && controller_age <= 3.0;
  result.cartesian_controller_active = arm.controller_known && arm.controller_active && controller_fresh;
  result.controller_detail = !controller_fresh ? "controller status missing or stale" : arm.controller_state;

  const double joints_age = age_seconds(arm.joint_received);
  result.joint_state_fresh = arm.joint_state_known && arm.joint_count >= 6 &&
    joints_age >= 0.0 && joints_age <= 0.5;
  result.joint_state_detail = !arm.joint_state_known ? "six selected-arm joints not received" :
    QString("%1 selected-arm joints · %2 · %3 s")
      .arg(arm.joint_count).arg(result.joint_state_fresh ? "current" : "stale")
      .arg(joints_age, 0, 'f', 1);

  const auto topics = node_->get_topic_names_and_types();
  const auto endpoint = topics.find(command_topic(robot, side));
  result.command_endpoint_ready = endpoint != topics.end() &&
    endpoint->second.size() == 1 && endpoint->second.front() == kTwistStampedType &&
    node_->count_subscribers(command_topic(robot, side)) > 0;
  result.command_endpoint_detail = result.command_endpoint_ready ? "TwistStamped subscriber ready" :
    "expected TwistStamped subscriber on " + QString::fromStdString(command_topic(robot, side));
  return result;
}

void RosUrArmJogAdapter::publish(const std::string & robot, const std::string & side,
  const std::array<double, 6> & twist)
{
  if (!connected()) return;
  const auto topic = command_topic(robot, side);
  auto publisher = publishers_.find(topic);
  if (publisher == publishers_.end()) {
    const auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).lifespan(rclcpp::Duration(kCommandLifespan));
    auto created = node_->create_publisher<geometry_msgs::msg::TwistStamped>(topic, qos);
    publisher = publishers_.emplace(topic, std::move(created)).first;
  }
  geometry_msgs::msg::TwistStamped message;
  message.header.stamp = node_->now();
  message.header.frame_id = "UR10_" + side + "/base_link";
  message.twist.linear.x = twist[0];
  message.twist.linear.y = twist[1];
  message.twist.linear.z = twist[2];
  message.twist.angular.x = twist[3];
  message.twist.angular.y = twist[4];
  message.twist.angular.z = twist[5];
  publisher->second->publish(message);
}
