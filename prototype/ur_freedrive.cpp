#include "ur_freedrive.hpp"
#include "ur_motion_actions.hpp"
#include <QEventLoop>
#include <QPointer>

namespace {
class FakeTask final : public UrMotionActionsAdapter {
public:
 bool connected() const override { return true; }
 UrArmReadiness inspect(const std::string &, const std::string &) override { return readiness; }
 bool start(const UrMotionTask &, QString *) override { return true; }
 void stop(const QString &) override {}
 UrMotionTaskStatus status(const QString &) const override { return {"Stop unconfirmed", "remote motion may continue", -1}; }
 UrArmReadiness readiness;
};
class FakeFreedrive final : public UrFreedriveAdapter {
public:
 bool connected() const override { return true; }
 UrArmReadiness inspect(const std::string &, const std::string &) override { return readiness; }
 void enter(const QString &, const QString &, std::function<void(bool, QString)> callback) override { done = std::move(callback); }
 void leave(std::function<void(bool, QString)> callback) override { done = std::move(callback); }
 bool publish(bool enabled) override { messages.push_back(enabled); return true; }
 UrArmReadiness readiness;
 std::function<void(bool, QString)> done;
 std::vector<bool> messages;
};
}
bool run_ur_freedrive_contract_test() {
 auto a = std::make_shared<FakeFreedrive>(); UrFreedrive f(a);
 f.setContext("mur620d", "Hardware", "session", true, true, true);
 if (f.enter() || f.busy()) return false;
 auto & r=a->readiness; r.connected=r.external_control=r.remote_control=r.reverse_interface=true;
 r.cartesian_controller_active=r.joint_state_fresh=r.command_endpoint_ready=true;
 if (!f.enter() || !f.busy() || f.active()) return false;
 a->done(true, "controller active");
 if (!f.active() || a->messages.empty() || !a->messages.back()) return false;
 QEventLoop loop; QTimer::singleShot(260, &loop, &QEventLoop::quit); loop.exec();
 if (a->messages.size() < 3) return false;
 f.leave(); if (!f.busy() || f.active() || a->messages.back()) return false;
 a->done(false, "unconfirmed"); if (!f.busy()) return false;
 f.leave(); a->done(true, "restored"); if (f.busy()) return false;
 if (!f.enter()) return false;
 f.setContext("mur620b", "Hardware", "next", true, true, true);
 if (!f.busy() || f.robot() != "mur620d") return false;
 auto activated=a->done; activated(true, "controller active");
 if (!f.busy() || f.active() || a->messages.back()) return false;
 a->done(true, "restored");
 if (f.busy() || f.robot() != "mur620b") return false;
 f.setShutdownInterlock([](const QString &, const QString &) { return true; });
 if (f.enter()) return false;
 f.setShutdownInterlock({});
 auto task = std::make_shared<FakeTask>(); task->readiness = r;
 UrMotionActions motion(task); f.setMotion(&motion);
 motion.setContext("Lab", "Hardware", "host", 62, "mur620d", "Connected", "session", "$HOME/colcon_ws", true, true);
 if (!motion.startHome()) return false;
 motion.cancel(); motion.refresh();
 if (!motion.busy() || f.enter()) return false;
 return true;
}

UrFreedrive::UrFreedrive(std::shared_ptr<UrFreedriveAdapter> adapter, QObject * parent)
: QObject(parent), adapter_(std::move(adapter)) {
 timer_.setInterval(100);
 connect(&timer_, &QTimer::timeout, this, [this] {
   if (state_ == WaitingEndpoint) {
     if (adapter_->connected() && adapter_->publish(true)) { state_=Active; status_="Freedrive controller active · keepalive running"; emit changed(); }
     else if (endpoint_wait_.elapsed() >= 3000) { state_=Unconfirmed; status_="Enable endpoint unavailable · restoring controller"; emit changed(); leave(); }
     return;
   }
   if (state_ == Active) {
     UrArmReadiness r;
     try { if (adapter_->connected()) r=adapter_->inspect(robot_.toStdString(), side_.toStdString()); } catch (...) {}
     if (!r.connected || !r.external_control || !r.remote_control || !r.reverse_interface || !r.joint_state_fresh) {
       status_="Arm readiness lost · restoring controller"; state_=Unconfirmed; emit changed(); leave(); return;
     }
   }
   if (state_ == Active && (!adapter_->connected() || !adapter_->publish(true))) {
     timer_.stop(); state_ = Unconfirmed; status_ = "Freedrive keepalive lost · exit unconfirmed"; emit changed();
   }
 });
}
void UrFreedrive::setContext(const QString & robot, const QString & mode, const QString & key,
 bool connected, bool left, bool right) {
 if (robot != robot_ || mode != mode_ || key != key_ || connected != connected_ || left != left_ || right != right_) {
   if (busy()) { pending_robot_=robot; pending_mode_=mode; pending_key_=key; pending_connected_=connected; pending_left_=left; pending_right_=right; pending_context_=true; leave(); status_ = "Session changed · exit must be confirmed"; emit changed(); return; }
   robot_=robot; mode_=mode; key_=key; connected_=connected; left_=left; right_=right; emit changed();
 }
}
void UrFreedrive::applyPendingContext() {
 if (!pending_context_) return;
 robot_=pending_robot_; mode_=pending_mode_; key_=pending_key_; connected_=pending_connected_; left_=pending_left_; right_=pending_right_; pending_context_=false;
}
void UrFreedrive::selectArm(const QString & side) {
 if (!busy() && (side == "l" || side == "r")) { side_=side; emit changed(); }
}
bool UrFreedrive::enter() {
 if (busy() || !adapter_ || !adapter_->connected() || !connected_ || mode_ != "Hardware" ||
     robot_.isEmpty() || (side_ == "l" ? !left_ : !right_) ||
     (shutdown_blocked_ && shutdown_blocked_(robot_, side_)) ||
     (jog_ && (jog_->armed() || jog_->holding())) || (motion_ && motion_->busy())) {
   status_="Freedrive blocked · check session, shutdown and arm motion"; emit changed(); return false;
 }
 UrArmReadiness r;
 try { r=adapter_->inspect(robot_.toStdString(), side_.toStdString()); } catch (...) {}
 if (!r.ready()) { status_="Freedrive blocked · selected arm readiness is unconfirmed"; emit changed(); return false; }
 state_=Entering; cancel_=false; status_="Switching controller · confirmation pending"; emit changed();
 QPointer<UrFreedrive> guard(this);
 QTimer::singleShot(6000, this, [guard]() { if (guard && guard->state_ == Entering) { guard->status_="Controller response unconfirmed · waiting before recovery"; emit guard->changed(); } });
 adapter_->enter(robot_, side_, [this, guard](bool ok, QString detail) {
   if (!guard) return;
   if (!ok) { state_=adapter_->safeFailure() ? Idle : Unconfirmed; status_=(state_ == Idle ? "Activation blocked · " : "Activation unconfirmed · ") + detail; if (state_ == Idle) applyPendingContext(); emit changed(); return; }
   if (cancel_) { state_=Unconfirmed; leave(); return; }
   if (!adapter_->publish(true)) { state_=WaitingEndpoint; status_="Waiting for enable endpoint"; endpoint_wait_.start(); timer_.start(); emit changed(); return; }
   state_=Active; status_="Freedrive controller active · keepalive running"; timer_.start(); emit changed();
 });
 return true;
}
void UrFreedrive::leave() {
 if (state_ == Idle || state_ == Leaving) return;
 if (state_ == Entering) { cancel_=true; status_="Cancellation requested · waiting for switch"; emit changed(); return; }
 timer_.stop(); adapter_->publish(false); state_=Leaving; status_="Restoring controller · confirmation pending"; emit changed();
 QPointer<UrFreedrive> guard(this);
 QTimer::singleShot(6000, this, [guard]() { if (guard && guard->state_ == Leaving) { guard->status_="Restoration response unconfirmed · waiting before recovery"; emit guard->changed(); } });
 adapter_->leave([this, guard](bool ok, QString detail) {
   if (!guard) return;
   state_=ok ? Idle : Unconfirmed; if (ok) applyPendingContext(); status_=(ok ? "Freedrive exited · " : "Exit unconfirmed · ") + detail; emit changed();
 });
}

#include <controller_manager_msgs/srv/list_controllers.hpp>
#include <controller_manager_msgs/srv/switch_controller.hpp>
#include <std_msgs/msg/bool.hpp>
#include <set>

class RosUrFreedriveAdapter::State {
public:
 using List = controller_manager_msgs::srv::ListControllers;
 using Switch = controller_manager_msgs::srv::SwitchController;
 rclcpp::Node::SharedPtr node;
 std::shared_ptr<UrArmJogAdapter> readiness;
 rclcpp::Client<List>::SharedPtr list;
 rclcpp::Client<Switch>::SharedPtr change;
 rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr publisher;
 std::vector<std::string> previous;
 std::function<void(bool, QString)> done;
 bool entering{false};
 bool safe_failure{false};
};
namespace {
template<typename S> void auto_done(const std::shared_ptr<S> & state, bool ok, const QString & detail) { auto callback=state->done; if (callback) callback(ok, detail); }
const std::string freedrive_name = "freedrive_mode_controller";
const std::set<std::string> motion_names = {"integrated_cartesian_admittance_controller", "forward_velocity_controller", "scaled_joint_trajectory_controller", "joint_trajectory_controller", "forward_position_controller", "forward_effort_controller", "force_mode_controller", "passthrough_trajectory_controller", "tool_contact_controller"};
}
RosUrFreedriveAdapter::RosUrFreedriveAdapter(std::shared_ptr<UrArmJogAdapter> readiness, QObject * parent)
: QObject(parent), state_(std::make_shared<State>()) { state_->readiness = std::move(readiness); }
RosUrFreedriveAdapter::~RosUrFreedriveAdapter() = default;
void RosUrFreedriveAdapter::setNode(rclcpp::Node::SharedPtr node) { state_->node=std::move(node); }
bool RosUrFreedriveAdapter::connected() const { return bool(state_->node); }
bool RosUrFreedriveAdapter::safeFailure() const { return state_->safe_failure; }
UrArmReadiness RosUrFreedriveAdapter::inspect(const std::string & robot, const std::string & side) { return state_->readiness->inspect(robot, side); }
void RosUrFreedriveAdapter::enter(const QString & robot, const QString & side, std::function<void(bool, QString)> done) {
 auto s=state_; s->done=std::move(done); s->entering=true; s->safe_failure=true; s->previous.clear();
 if (!s->node) { auto_done(s, false, "ROS disconnected"); return; }
 const auto prefix="/" + robot.toStdString() + "/UR10_" + side.toStdString();
 s->list=s->node->create_client<State::List>(prefix + "/controller_manager/list_controllers");
 s->change=s->node->create_client<State::Switch>(prefix + "/controller_manager/switch_controller");
 s->publisher=s->node->create_publisher<std_msgs::msg::Bool>(prefix + "/freedrive_mode_controller/enable_freedrive_mode", 10);
 std::weak_ptr<State> weak=s;
 QTimer::singleShot(1500, this, [weak]() {
  auto s=weak.lock(); if (!s || !s->done) return;
  if (!s->node || !s->list->service_is_ready() || !s->change->service_is_ready()) { auto_done(s, false, "controller services unavailable"); return; }
 s->list->async_send_request(std::make_shared<State::List::Request>(), [weak](rclcpp::Client<State::List>::SharedFuture f) {
  auto s=weak.lock(); if (!s || !s->done) return;
  bool found=false; try { for (const auto & c : f.get()->controller) {
    if (c.name == freedrive_name && c.state == "inactive") found=true;
    if (motion_names.count(c.name) && c.state == "active") s->previous.push_back(c.name);
  }} catch (...) { auto_done(s, false, "controller query failed"); return; }
  if (!found || s->previous.empty()) { auto_done(s, false, "freedrive inactive controller or active motion controller missing"); return; }
  auto request=std::make_shared<State::Switch::Request>(); request->activate_controllers={freedrive_name};
  request->deactivate_controllers=s->previous; request->strictness=State::Switch::Request::STRICT; request->timeout.sec=3;
  s->safe_failure=false;
  s->change->async_send_request(request, [weak](rclcpp::Client<State::Switch>::SharedFuture f2) {
   auto s=weak.lock(); if (!s || !s->done) return;
   try { if (!f2.get()->ok) { auto_done(s, false, "controller switch failed"); return; } } catch (...) { auto_done(s, false, "controller switch failed"); return; }
   s->list->async_send_request(std::make_shared<State::List::Request>(), [weak](rclcpp::Client<State::List>::SharedFuture f3) {
    auto s=weak.lock(); if (!s || !s->done) return; bool active=false; std::set<std::string> inactive;
    try { for (const auto & c : f3.get()->controller) {
      if (c.name==freedrive_name && c.state=="active") active=true;
      if (c.state=="inactive") inactive.insert(c.name);
    } } catch (...) {}
    for (const auto & c : s->previous) if (!inactive.count(c)) active=false;
    auto_done(s, active, active ? "controller active and previous controllers inactive" : "controller state unconfirmed");
   });
  });
 });
 });
}
void RosUrFreedriveAdapter::leave(std::function<void(bool, QString)> done) {
 auto s=state_; s->done=std::move(done);
 if (!s->node || !s->change || !s->list || !s->change->service_is_ready() || s->previous.empty()) { auto_done(s, false, "restoration unavailable"); return; }
 auto request=std::make_shared<State::Switch::Request>(); request->activate_controllers=s->previous;
 request->deactivate_controllers={freedrive_name}; request->strictness=State::Switch::Request::BEST_EFFORT; request->timeout.sec=3;
 std::weak_ptr<State> weak=s;
 s->change->async_send_request(request, [weak](rclcpp::Client<State::Switch>::SharedFuture f) {
  auto s=weak.lock(); if (!s || !s->done) return;
  try { if (!f.get()->ok) { auto_done(s, false, "restore switch failed"); return; } } catch (...) { auto_done(s, false, "restore switch failed"); return; }
  s->list->async_send_request(std::make_shared<State::List::Request>(), [weak](rclcpp::Client<State::List>::SharedFuture f2) {
   auto s=weak.lock(); if (!s || !s->done) return; bool off=false; std::set<std::string> active;
   try { for (const auto & c : f2.get()->controller) { if (c.name==freedrive_name && c.state=="inactive") off=true; if(c.state=="active") active.insert(c.name); } } catch (...) {}
   for (const auto & c : s->previous) if (!active.count(c)) off=false;
   auto_done(s, off, off ? "previous controllers restored" : "controller restoration unconfirmed");
  });
 });
}
bool RosUrFreedriveAdapter::publish(bool enabled) {
 if (!state_->node || !state_->publisher) return false;
 std_msgs::msg::Bool msg; msg.data=enabled; state_->publisher->publish(msg);
 return state_->publisher->get_subscription_count() > 0;
}
