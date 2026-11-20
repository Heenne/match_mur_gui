#include "lift_controller.hpp"

#include <cmath>
#include <unordered_map>
#include <vector>

namespace
{
class FakeLiftAdapter final : public LiftAdapter
{
public:
  bool connected() const override { return is_connected; }
  LiftMeasurement measurement(const std::string &, const std::string &) override { return sample; }
  LiftCommandEndpoint commandEndpoint(const std::string &, const std::string &) override
  {
    return endpoint;
  }
  bool sendTarget(const std::string & robot, const std::string & side, double meters) override
  {
    targets.push_back({robot, side, meters});
    return send_succeeds;
  }
  struct Target { std::string robot; std::string side; double meters; };
  bool is_connected{true};
  bool send_succeeds{true};
  LiftMeasurement sample{true, 0.050, 0.1};
  LiftCommandEndpoint endpoint{"ewellix_interfaces/msg/Command", 1};
  std::vector<Target> targets;
};
}

RosLiftAdapter::RosLiftAdapter(rclcpp::Node::SharedPtr node) : node_(std::move(node)) {}
void RosLiftAdapter::setNode(rclcpp::Node::SharedPtr node) { node_ = std::move(node); }
bool RosLiftAdapter::connected() const { return false; }
LiftMeasurement RosLiftAdapter::measurement(const std::string &, const std::string &) { return {}; }
LiftCommandEndpoint RosLiftAdapter::commandEndpoint(const std::string &, const std::string &) { return {}; }
bool RosLiftAdapter::sendTarget(const std::string &, const std::string &, double) { return false; }
void RosLiftAdapter::subscribe(const std::string &, const std::string &) {}

LiftController::LiftController(std::shared_ptr<LiftAdapter> adapter, QObject * parent)
: QObject(parent), adapter_(std::move(adapter)) {}
LiftController::~LiftController() = default;
void LiftController::setContext(const QString &, const QString &, const QString &, bool, const QString &) {}
void LiftController::selectSide(const QString &) {}
void LiftController::setStepMeters(double) {}
void LiftController::setLimits(double, double) {}
bool LiftController::press(const QString &) { return false; }
void LiftController::release() {}
void LiftController::stop(const QString &) {}
void LiftController::refresh() {}
void LiftController::tick() {}
bool LiftController::ready(QString *) { return false; }
bool LiftController::step(const QString &) { return false; }
bool LiftController::haltOwnedCommand(const QString &) { return false; }
void LiftController::loadPresetSettings(const QString &) {}
void LiftController::savePresetSettings() const {}
void LiftController::updateAvailability() {}
void LiftController::setStatus(const QString & status) { status_ = status; emit changed(); }

bool run_lift_controller_contract_test()
{
  auto adapter = std::make_shared<FakeLiftAdapter>();
  LiftController lift(adapter);
  lift.setContext("mur620b", "Hardware", "localhost", true, "Lift contract");
  lift.selectSide("right");
  lift.setStepMeters(0.02);
  return lift.press("up") && adapter->targets.size() == 1 &&
    adapter->targets.front().robot == "mur620b" &&
    adapter->targets.front().side == "right" &&
    std::abs(adapter->targets.front().meters - 0.07) < 1.0e-6;
}
