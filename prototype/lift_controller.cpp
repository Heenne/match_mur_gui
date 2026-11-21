#include "lift_controller.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QEventLoop>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

namespace
{
constexpr double kTicksPerMeter = 3225.0;
constexpr double kMinimumPhysicalHeight = 0.005;
constexpr double kMaximumPhysicalHeight = 0.260;
constexpr double kMinimumStep = 0.001;
constexpr double kMaximumStep = 0.05;
constexpr double kStateTimeoutSeconds = 2.0;
constexpr double kEpsilon = 1.0e-5;
const std::string kStateType = "ewellix_interfaces/msg/State";
const std::string kCommandType = "ewellix_interfaces/msg/Command";

bool validRobot(const QString & robot)
{
  static const QRegularExpression pattern("^[A-Za-z_][A-Za-z0-9_]*$");
  return pattern.match(robot).hasMatch();
}

bool validSide(const QString & side)
{
  return side == "left" || side == "right";
}

std::string sideCode(const std::string & side)
{
  return side == "left" ? "l" : "r";
}

std::string stateTopic(const std::string & robot, const std::string & side)
{
  return "/" + robot + "/ewellix_lift_" + sideCode(side) + "/state";
}

std::string commandTopic(const std::string & robot, const std::string & side)
{
  return "/" + robot + "/ewellix_lift_" + sideCode(side) + "/command";
}

std::string stateKey(const std::string & robot, const std::string & side)
{
  return robot + "/" + side;
}

class FakeLiftAdapter final : public LiftAdapter
{
public:
  struct Target { std::string robot; std::string side; double meters; };
  bool connected() const override { return is_connected; }
  LiftMeasurement measurement(const std::string & robot, const std::string & side) override
  {
    const auto found = samples.find(stateKey(robot, side));
    return found == samples.end() ? LiftMeasurement{} : found->second;
  }
  LiftCommandEndpoint commandEndpoint(const std::string &, const std::string &) override
  {
    return endpoint;
  }
  bool sendTarget(const std::string & robot, const std::string & side, double meters) override
  {
    targets.push_back({robot, side, meters});
    return send_succeeds;
  }

  bool is_connected{true};
  bool send_succeeds{true};
  LiftCommandEndpoint endpoint{kCommandType, 1};
  std::unordered_map<std::string, LiftMeasurement> samples;
  std::vector<Target> targets;
};
}

RosLiftAdapter::RosLiftAdapter(rclcpp::Node::SharedPtr node) : node_(std::move(node)) {}

void RosLiftAdapter::setNode(rclcpp::Node::SharedPtr node)
{
  if (node_ != node) {
    state_subscriptions_.clear();
    states_.clear();
    command_publishers_.clear();
  }
  node_ = std::move(node);
}

bool RosLiftAdapter::connected() const
{
  return node_ && rclcpp::ok(node_->get_node_base_interface()->get_context());
}

void RosLiftAdapter::subscribe(const std::string & robot, const std::string & side)
{
  if (!connected()) return;
  const auto key = stateKey(robot, side);
  if (state_subscriptions_.find(key) != state_subscriptions_.end()) return;
  const auto topic = stateTopic(robot, side);
  state_subscriptions_[key] = node_->create_subscription<ewellix_interfaces::msg::State>(
    topic, rclcpp::QoS(10), [this, key](ewellix_interfaces::msg::State::ConstSharedPtr message) {
      const auto count = std::min<size_t>(2, message->actual_positions.size());
      if (!count) return;
      double total = 0.0;
      size_t valid = 0;
      for (size_t index = 0; index < count; ++index) {
        if (message->actual_positions[index] >= 0) {
          total += static_cast<double>(message->actual_positions[index]);
          ++valid;
        }
      }
      if (!valid) return;
      states_[key] = {total / static_cast<double>(valid) / kTicksPerMeter,
        std::chrono::steady_clock::now()};
    });
}

LiftMeasurement RosLiftAdapter::measurement(const std::string & robot, const std::string & side)
{
  if (!connected() || sideCode(side).empty()) return {};
  subscribe(robot, side);
  const auto found = states_.find(stateKey(robot, side));
  if (found == states_.end()) return {};
  const double age = std::chrono::duration<double>(
    std::chrono::steady_clock::now() - found->second.received).count();
  return {true, found->second.meters, age};
}

LiftCommandEndpoint RosLiftAdapter::commandEndpoint(const std::string & robot, const std::string & side)
{
  if (!connected()) return {};
  const auto topic = commandTopic(robot, side);
  const auto graph = node_->get_topic_names_and_types();
  const auto found = graph.find(topic);
  if (found == graph.end() || found->second.size() != 1) return {};
  return {found->second.front(), static_cast<size_t>(node_->count_subscribers(topic))};
}

bool RosLiftAdapter::sendTarget(const std::string & robot, const std::string & side, double meters)
{
  if (!connected() || !std::isfinite(meters) || meters < kMinimumPhysicalHeight ||
    meters > kMaximumPhysicalHeight) return false;
  const auto topic = commandTopic(robot, side);
  const auto endpoint = commandEndpoint(robot, side);
  if (endpoint.type != kCommandType || endpoint.subscribers == 0) return false;
  auto found = command_publishers_.find(topic);
  if (found == command_publishers_.end()) {
    found = command_publishers_.emplace(topic,
      node_->create_publisher<ewellix_interfaces::msg::Command>(topic, rclcpp::QoS(10))).first;
  }
  ewellix_interfaces::msg::Command message;
  message.ticks = 0;
  message.meters = static_cast<float>(meters);
  found->second->publish(message);
  return true;
}

LiftController::LiftController(std::shared_ptr<LiftAdapter> adapter, QObject * parent)
: QObject(parent), adapter_(std::move(adapter))
{
  repeat_timer_.setInterval(250);
  connect(&repeat_timer_, &QTimer::timeout, this, &LiftController::tick);
  refresh_timer_.setInterval(100);
  connect(&refresh_timer_, &QTimer::timeout, this, &LiftController::refresh);
  refresh_timer_.start();

  const auto test_path = qEnvironmentVariable("MUR_COMMAND_CENTER_SETTINGS");
  if (test_path.isEmpty()) {
    settings_ = std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
      "MATCH", "MuRCommandCenter");
  } else {
    settings_ = std::make_unique<QSettings>(test_path, QSettings::IniFormat);
  }
}

LiftController::~LiftController()
{
  stop("controller destroyed");
}

void LiftController::setContext(const QString & robot, const QString & mode,
  const QString & target, bool connected, const QString & preset)
{
  const auto next_robot = robot.trimmed();
  const auto next_mode = mode.trimmed();
  const auto next_target = target.trimmed();
  const auto next_preset = preset.trimmed().isEmpty() ? QStringLiteral("Default") : preset.trimmed();
  if (next_robot == robot_ && next_mode == mode_ && next_target == target_ &&
    connected == context_connected_ && next_preset == preset_) return;

  if (holding_) stop("session context changed");
  const bool preset_changed = next_preset != preset_;
  robot_ = validRobot(next_robot) ? next_robot : QString();
  mode_ = next_mode;
  target_ = next_target;
  context_connected_ = connected;
  preset_ = next_preset;
  if (preset_changed) loadPresetSettings(preset_);
  refresh();
  emit changed();
}

void LiftController::selectSide(const QString & side)
{
  const auto selected = side.trimmed().toLower();
  if (!validSide(selected) || selected == side_) return;
  if (holding_) stop("lift side changed");
  side_ = selected;
  refresh();
}

void LiftController::setStepMeters(double meters)
{
  if (!std::isfinite(meters) || meters < kMinimumStep || meters > kMaximumStep) {
    setStatus("Invalid step · choose 0.001–0.050 m");
    return;
  }
  if (holding_) stop("step size changed");
  step_meters_ = meters;
  savePresetSettings();
  emit changed();
}

void LiftController::setLimits(double minimum, double maximum)
{
  if (holding_) stop("limits changed");
  if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
    minimum < kMinimumPhysicalHeight || maximum > kMaximumPhysicalHeight || minimum >= maximum) {
    limits_valid_ = false;
    can_lift_up_ = false;
    can_lift_down_ = false;
    setStatus("Invalid lift limits · allowed range is 0.005–0.260 m and minimum must be below maximum");
    emit changed();
    return;
  }
  minimum_meters_ = minimum;
  maximum_meters_ = maximum;
  limits_valid_ = true;
  savePresetSettings();
  refresh();
}

bool LiftController::press(const QString & direction)
{
  const auto selected = direction.trimmed().toLower();
  if (selected != "up" && selected != "down") return false;
  if (holding_) stop("direction changed");
  holding_direction_ = selected;
  if (!step(selected)) {
    holding_direction_.clear();
    holding_ = false;
    repeat_timer_.stop();
    emit changed();
    return false;
  }
  holding_ = true;
  repeat_timer_.start();
  emit changed();
  return true;
}

void LiftController::release()
{
  stop("released");
}

void LiftController::stop(const QString & reason)
{
  if (!holding_) {
    repeat_timer_.stop();
    holding_direction_.clear();
    return;
  }
  haltOwnedCommand(reason);
}

void LiftController::refresh()
{
  const auto sample = adapter_ && validRobot(robot_) && validSide(side_)
    ? adapter_->measurement(robot_.toStdString(), side_.toStdString()) : LiftMeasurement{};
  have_height_ = sample.received && std::isfinite(sample.meters) &&
    std::isfinite(sample.age_seconds) && sample.age_seconds >= 0.0;
  if (have_height_) {
    height_meters_ = sample.meters;
    age_seconds_ = sample.age_seconds;
  } else {
    age_seconds_ = -1.0;
  }
  updateAvailability();
  emit changed();
}

void LiftController::tick()
{
  if (!holding_ || holding_direction_.isEmpty()) {
    repeat_timer_.stop();
    return;
  }
  if (!step(holding_direction_)) stop("step blocked or failed");
}

bool LiftController::ready(QString * reason)
{
  auto fail = [reason](const QString & why) {
    if (reason) *reason = why;
    return false;
  };
  if (mode_ != "Hardware") return fail("Lift control is unavailable in Simulation · use a simulation-specific lift adapter");
  if (!context_connected_ || !adapter_ || !adapter_->connected()) return fail("Disconnected · connect to the selected hardware target");
  if (!validRobot(robot_) || target_.isEmpty()) return fail("Select a valid robot and target");
  if (!limits_valid_) return fail("Invalid configured limits · correct the range before lifting");
  if (!have_height_) return fail("Missing height · waiting for a valid Ewellix state");
  if (age_seconds_ > kStateTimeoutSeconds) return fail(QString("Stale height · data is %1 s old").arg(age_seconds_, 0, 'f', 1));
  if (height_meters_ < kMinimumPhysicalHeight || height_meters_ > kMaximumPhysicalHeight) {
    return fail("Invalid height · reported value is outside the physical lift range");
  }
  if (height_meters_ < minimum_meters_ - kEpsilon || height_meters_ > maximum_meters_ + kEpsilon) {
    return fail("Height is outside the configured range · review profile limits");
  }
  const auto endpoint = adapter_->commandEndpoint(robot_.toStdString(), side_.toStdString());
  if (endpoint.type != kCommandType) return fail("Command unavailable · expected ewellix_interfaces/msg/Command");
  if (endpoint.subscribers == 0) return fail("Command unavailable · no lift controller subscribes to the selected side");
  if (reason) reason->clear();
  return true;
}

bool LiftController::step(const QString & direction)
{
  refresh();
  QString reason;
  if (!ready(&reason)) {
    setStatus(reason);
    return false;
  }
  const double delta = direction == "up" ? step_meters_ : direction == "down" ? -step_meters_ : 0.0;
  if (std::abs(delta) < kEpsilon) return false;
  const double target = std::clamp(height_meters_ + delta, minimum_meters_, maximum_meters_);
  if (std::abs(target - height_meters_) < kEpsilon) {
    setStatus(direction == "up" ? "Upper limit reached · upward motion blocked" :
      "Lower limit reached · downward motion blocked");
    return false;
  }
  if (!adapter_->sendTarget(robot_.toStdString(), side_.toStdString(), target)) {
    setStatus("Command failed · no confirmation that the lift target was sent");
    return false;
  }
  setStatus(QString("Target accepted · %1 side %2 to %3 m · step %4 m")
    .arg(robot_, side_).arg(target, 0, 'f', 3).arg(std::abs(target - height_meters_), 0, 'f', 3));
  return true;
}

bool LiftController::haltOwnedCommand(const QString & reason)
{
  repeat_timer_.stop();
  const bool was_holding = holding_;
  holding_ = false;
  holding_direction_.clear();
  bool stopped = false;
  if (was_holding && adapter_ && context_connected_ && adapter_->connected() && validRobot(robot_) && validSide(side_)) {
    const auto sample = adapter_->measurement(robot_.toStdString(), side_.toStdString());
    const auto endpoint = adapter_->commandEndpoint(robot_.toStdString(), side_.toStdString());
    if (sample.received && std::isfinite(sample.meters) && std::isfinite(sample.age_seconds) &&
      sample.age_seconds >= 0.0 && sample.age_seconds <= kStateTimeoutSeconds &&
      endpoint.type == kCommandType && endpoint.subscribers > 0 &&
      sample.meters >= minimum_meters_ && sample.meters <= maximum_meters_) {
      stopped = adapter_->sendTarget(robot_.toStdString(), side_.toStdString(), sample.meters);
    }
  }
  setStatus(stopped ? QString("Stopped · held lift target cancelled at current height (%1)").arg(reason) :
    QString("Stopped sending targets · lift may still be completing its last target (%1)").arg(reason));
  emit changed();
  return stopped;
}

void LiftController::loadPresetSettings(const QString & preset)
{
  const QString base = QString("presets/%1/lift/").arg(preset);
  step_meters_ = settings_->value(base + "stepMeters", 0.01).toDouble();
  minimum_meters_ = settings_->value(base + "minimumMeters", 0.005).toDouble();
  maximum_meters_ = settings_->value(base + "maximumMeters", 0.260).toDouble();
  limits_valid_ = std::isfinite(step_meters_) && step_meters_ >= kMinimumStep && step_meters_ <= kMaximumStep &&
    std::isfinite(minimum_meters_) && std::isfinite(maximum_meters_) &&
    minimum_meters_ >= kMinimumPhysicalHeight && maximum_meters_ <= kMaximumPhysicalHeight &&
    minimum_meters_ < maximum_meters_;
  if (!limits_valid_) {
    step_meters_ = 0.01;
    minimum_meters_ = 0.005;
    maximum_meters_ = 0.260;
    limits_valid_ = true;
    setStatus("Invalid saved lift range · safe defaults restored");
  }
}

void LiftController::savePresetSettings() const
{
  if (!settings_ || preset_.isEmpty() || !limits_valid_) return;
  const QString base = QString("presets/%1/lift/").arg(preset_);
  settings_->setValue(base + "stepMeters", step_meters_);
  settings_->setValue(base + "minimumMeters", minimum_meters_);
  settings_->setValue(base + "maximumMeters", maximum_meters_);
  settings_->sync();
}

void LiftController::updateAvailability()
{
  QString reason;
  const bool is_ready = ready(&reason);
  can_lift_up_ = false;
  can_lift_down_ = false;
  if (is_ready) {
    can_lift_up_ = height_meters_ < maximum_meters_ - kEpsilon;
    can_lift_down_ = height_meters_ > minimum_meters_ + kEpsilon;
    if (!can_lift_up_ && !can_lift_down_) reason = "Lift at configured limit";
    else reason = QString("Ready · height %1 m · age %2 s")
      .arg(height_meters_, 0, 'f', 3).arg(age_seconds_, 0, 'f', 1);
  }
  if (!holding_) setStatus(reason);
}

void LiftController::setStatus(const QString & status)
{
  if (status_ == status) return;
  status_ = status;
  emit changed();
}

bool run_lift_controller_contract_test()
{
  QTemporaryDir directory;
  if (!directory.isValid()) return false;
  const QByteArray previous_settings = qgetenv("MUR_COMMAND_CENTER_SETTINGS");
  qputenv("MUR_COMMAND_CENTER_SETTINGS", directory.filePath("settings.ini").toLocal8Bit());

  auto adapter = std::make_shared<FakeLiftAdapter>();
  adapter->samples[stateKey("mur620b", "right")] = {true, 0.050, 0.1};
  LiftController lift(adapter);
  lift.setContext("mur620b", "Hardware", "localhost", true, "Lift contract");
  lift.selectSide("right");
  lift.setStepMeters(0.02);
  bool passed = lift.canLiftUp() && lift.canLiftDown() && lift.press("up") &&
    adapter->targets.size() == 1 && adapter->targets.front().robot == "mur620b" &&
    adapter->targets.front().side == "right" &&
    std::abs(adapter->targets.front().meters - 0.07) < 1.0e-6;
  lift.release();
  passed = passed && !lift.holding() && adapter->targets.size() == 2 &&
    std::abs(adapter->targets.back().meters - 0.05) < 1.0e-6 && lift.status().contains("Stopped");

  adapter->samples[stateKey("mur620b", "right")] = {true, 0.260, 0.1};
  lift.refresh();
  const auto count_at_top = adapter->targets.size();
  passed = passed && !lift.canLiftUp() && lift.canLiftDown() && !lift.press("up") &&
    adapter->targets.size() == count_at_top && lift.status().contains("limit");

  adapter->samples[stateKey("mur620b", "right")] = {true, 0.005, 0.1};
  lift.refresh();
  const auto count_at_bottom = adapter->targets.size();
  passed = passed && !lift.canLiftDown() && lift.canLiftUp() && !lift.press("down") &&
    adapter->targets.size() == count_at_bottom;

  adapter->samples[stateKey("mur620b", "right")] = {true, 0.1, kStateTimeoutSeconds + 0.1};
  lift.refresh();
  const auto count_when_stale = adapter->targets.size();
  passed = passed && !lift.canLiftUp() && !lift.canLiftDown() && !lift.press("up") &&
    adapter->targets.size() == count_when_stale && lift.status().contains("Stale");

  adapter->samples[stateKey("mur620b", "right")] = {true, 0.1, 0.1};
  lift.setContext("mur620b", "Simulation", "localhost", true, "Lift contract");
  passed = passed && !lift.press("up") && adapter->targets.size() == count_when_stale &&
    lift.status().contains("Simulation");
  lift.setContext("mur620b", "Hardware", "localhost", true, "Lift contract");
  lift.setLimits(0.3, 0.2);
  passed = passed && !lift.canLiftUp() && !lift.canLiftDown() && !lift.press("up") &&
    lift.status().contains("Invalid lift limits");

  lift.setLimits(0.01, 0.25);
  lift.setStepMeters(0.03);
  LiftController restored(std::make_shared<FakeLiftAdapter>());
  restored.setContext("mur620b", "Hardware", "localhost", true, "Lift contract");
  passed = passed && std::abs(restored.stepMeters() - 0.03) < 1.0e-6 &&
    std::abs(restored.minimumMeters() - 0.01) < 1.0e-6 &&
    std::abs(restored.maximumMeters() - 0.25) < 1.0e-6;

  qputenv("MUR_COMMAND_CENTER_SETTINGS", previous_settings);
  return passed;
}
