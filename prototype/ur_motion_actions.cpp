#include "ur_motion_actions.hpp"

namespace {
class FakeMotionAdapter final : public UrMotionActionsAdapter {
public:
  bool connected() const override { return true; }
  UrArmReadiness inspect(const std::string &, const std::string &) override { return readiness; }
  bool start(const UrMotionTask & value, QString *) override { task = value; ++starts; return true; }
  void stop(const QString &) override { ++stops; }
  UrMotionTaskStatus status(const QString &) const override { return state; }
  UrArmReadiness readiness;
  UrMotionTask task;
  UrMotionTaskStatus state{"Running", "Planning", -1};
  int starts{0};
  int stops{0};
};
}

bool run_ur_motion_actions_contract_test()
{
  auto adapter = std::make_shared<FakeMotionAdapter>();
  UrMotionActions action(adapter);
  action.setContext("Lab", "Hardware", "robot-host", 62, "mur620d", "Connected", "session", "$HOME/colcon_ws", true, true);
  if (action.startHome() || adapter->starts != 0 || action.ready()) return false;
  auto & r = adapter->readiness;
  r.connected = r.external_control = r.remote_control = r.reverse_interface = true;
  r.cartesian_controller_active = r.joint_state_fresh = r.command_endpoint_ready = true;
  action.refresh();
  if (!action.ready() || !action.startHome() || adapter->starts != 1 ||
      adapter->task.named_pose != "Home_custom" || adapter->task.robot != "mur620d") return false;
  action.cancel();
  if (adapter->stops != 1 || !action.busy()) return false;
  adapter->state = {"Stopped", "Cancellation requested", 130};
  action.refresh();
  if (action.busy() || !action.result().contains("Stopped")) return false;
  action.selectArm("l");
  action.setAlignmentPlane("XZ");
  action.setAlignmentDirection("-Y");
  action.setAngularVelocity(0.07);
  if (!action.previewAlignment() || !action.alignmentPreview().contains("-Y") ||
      !action.referenceFrame().contains("UR10_l") || !action.startAlignment()) return false;
  if (adapter->task.side != "l" || adapter->task.plane != "XZ" ||
      adapter->task.direction != "-Y" || adapter->task.angular_velocity != 0.07) return false;
  adapter->readiness.joint_state_fresh = false;
  adapter->state = {"Running", "Executing", -1};
  action.refresh();
  if (adapter->stops != 2 || !action.status().contains("readiness lost")) return false;
  adapter->state = {"Stopped", "Cancelled", 130};
  adapter->readiness.joint_state_fresh = true; action.refresh();
  action.setShutdownInterlock([](const QString & robot, const QString & side) {
    return robot == "mur620d" && side == "l";
  });
  if (action.startHome() || adapter->starts != 2) return false;
  action.selectArm("r");
  if (!action.startHome() || adapter->starts != 3) return false;
  return true;
}

#include "process_manager.hpp"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <utility>

UrMotionActions::UrMotionActions(std::shared_ptr<UrMotionActionsAdapter> adapter, QObject * parent)
: QObject(parent), adapter_(std::move(adapter))
{
  timer_.setInterval(250);
  connect(&timer_, &QTimer::timeout, this, [this]() { refresh(); });
  timer_.start();
  updateReferenceFrame();
  updatePreview();
}
QString UrMotionActions::selectedArm() const { return side_ == "l" ? "UR10_l" : "UR10_r"; }
void UrMotionActions::setContext(const QString & preset, const QString & mode,
  const QString & host, int domain, const QString & robot, const QString & connection,
  const QString & key, const QString & workspace, bool left, bool right)
{
  if (preset_ != preset || mode_ != mode || host_ != host || ros_domain_ != domain ||
      robot_ != robot || connection_state_ != connection || session_key_ != key ||
      remote_workspace_ != workspace || arm_l_enabled_ != left || arm_r_enabled_ != right) {
    if (busy_) cancel();
    preset_ = preset; mode_ = mode; host_ = host; ros_domain_ = domain; robot_ = robot;
    connection_state_ = connection; session_key_ = key; remote_workspace_ = workspace;
    arm_l_enabled_ = left; arm_r_enabled_ = right;
    updateReferenceFrame(); updatePreview(); refresh();
  }
}
void UrMotionActions::selectArm(const QString & side)
{
  if (side != "l" && side != "r") return;
  if (side != side_) { if (busy_) cancel(); side_ = side; updateReferenceFrame(); updatePreview(); refresh(); }
}
void UrMotionActions::setHomePose(const QString & pose) { if (!busy_) { home_pose_ = pose.trimmed(); emit changed(); } }
void UrMotionActions::setVelocityScaling(double percent)
{ if (!busy_) { velocity_scaling_ = std::isfinite(percent) ? std::clamp(percent, 1.0, 100.0) / 100.0 : 0.01; emit changed(); } }
void UrMotionActions::setAngularVelocity(double value)
{ if (!busy_) { angular_velocity_ = std::isfinite(value) ? std::clamp(value, 0.03, 0.30) : 0.03; emit changed(); } }
void UrMotionActions::setAlignmentPlane(const QString & plane)
{
  if (busy_ || (plane != "XY" && plane != "XZ" && plane != "YZ")) return;
  alignment_plane_ = plane; alignment_direction_ = plane == "XY" ? "+Z" : plane == "XZ" ? "+Y" : "+X";
  updatePreview(); emit changed();
}
void UrMotionActions::setAlignmentDirection(const QString & direction)
{
  if (busy_) return;
  const auto axis = alignment_plane_ == "XY" ? "Z" : alignment_plane_ == "XZ" ? "Y" : "X";
  if (direction != QString("+") + axis && direction != QString("-") + axis) return;
  alignment_direction_ = direction; updatePreview(); emit changed();
}
void UrMotionActions::updateReferenceFrame() { reference_frame_ = robot_ + "/" + selectedArm() + "/base_link"; }
void UrMotionActions::updatePreview()
{ alignment_preview_ = QString("TCP Z → %1 in %2; nearest rotation, current TCP position retained.").arg(alignment_direction_, reference_frame_); }
bool UrMotionActions::validIdentifier(const QString & value)
{ static const QRegularExpression re("^[A-Za-z][A-Za-z0-9_]*$"); return re.match(value).hasMatch(); }
void UrMotionActions::refresh()
{
  UrArmReadiness r;
  if (adapter_ && adapter_->connected() && mode_ == "Hardware" && connection_state_ == "Connected" &&
      validIdentifier(robot_) && (side_ == "l" ? arm_l_enabled_ : arm_r_enabled_)) {
    try { r = adapter_->inspect(robot_.toStdString(), side_.toStdString()); } catch (...) { r = {}; }
  }
  ready_ = r.ready(); readiness_ = r.summary(selectedArm());
  if (busy_) {
    updateTaskStatus();
    if (busy_ && !ready_) { cancel(); status_ = "Cancellation requested · readiness lost"; }
  }
  emit changed();
}
bool UrMotionActions::canStart(QString * reason)
{
  refresh();
  if (shutdown_interlock_ && shutdown_interlock_(robot_, side_)) *reason = "UR shutdown interlock";
  else if (busy_) *reason = "Another motion is active";
  else if (jog_ && (jog_->armed() || jog_->holding())) *reason = "Disarm arm jog first";
  else if (!ready_) *reason = "Selected arm is not ready";
  else return true;
  return false;
}
UrMotionTask UrMotionActions::task(const QString & kind) const
{
  UrMotionTask t; t.kind = kind; t.name = "UR motion · " + robot_ + " · " + side_;
  t.host = host_; t.ros_domain = ros_domain_; t.robot = robot_; t.side = side_;
  t.remote_workspace = remote_workspace_; t.named_pose = home_pose_; t.plane = alignment_plane_;
  t.direction = alignment_direction_; t.reference_frame = reference_frame_; t.velocity_scaling = velocity_scaling_; t.angular_velocity = angular_velocity_;
  return t;
}
void UrMotionActions::start(const QString & kind)
{
  QString detail; auto t = task(kind);
  if (!adapter_->start(t, &detail)) { status_ = "Start failed · " + detail; result_ = detail; emit changed(); return; }
  process_name_ = t.name; busy_ = true; status_ = "Starting " + kind;
  progress_ = "Starting remote motion"; result_ = "Pending"; emit changed();
}
bool UrMotionActions::startHome()
{
  QString reason; if (!canStart(&reason)) { status_ = "Home blocked · " + reason; emit changed(); return false; }
  if (!validIdentifier(home_pose_)) { status_ = "Home blocked · invalid named pose"; emit changed(); return false; }
  start("Home"); return busy_;
}
bool UrMotionActions::previewAlignment() { updatePreview(); emit changed(); return true; }
bool UrMotionActions::startAlignment()
{
  QString reason; if (!canStart(&reason)) { status_ = "Alignment blocked · " + reason; emit changed(); return false; }
  start("Alignment"); return busy_;
}
void UrMotionActions::cancel()
{
  if (!busy_) return; adapter_->stop(process_name_); status_ = "Cancellation requested";
  progress_ = "Waiting for stop confirmation"; emit changed();
}
void UrMotionActions::updateTaskStatus()
{
  const auto state = adapter_->status(process_name_);
  if (!state.output.isEmpty()) progress_ = state.output.right(1500);
  if (state.state == "Stop unconfirmed" || state.state == "Stop failed" ||
      (state.state == "Failed" && (state.exit_code == 255 || state.exit_code == -1)) ||
      (state.state == "Stopped" && state.output.contains("cancellation was not confirmed", Qt::CaseInsensitive))) {
    status_ = "Stop unconfirmed · remote motion may still be active";
    result_ = state.output.right(500);
    return;
  }
  if (state.state == "Exited" || state.state == "Failed" || state.state == "Stopped") {
    busy_ = false; status_ = state.state; result_ = QString("%1 · exit %2 · %3").arg(state.state).arg(state.exit_code).arg(state.output.right(500));
  }
}
void UrMotionActions::setStatus(const QString & value) { status_ = value; emit changed(); }

SshUrMotionActionsAdapter::SshUrMotionActionsAdapter(std::shared_ptr<UrArmJogAdapter> readiness,
  ProcessManager * processes) : readiness_(std::move(readiness)), processes_(processes) {}
bool SshUrMotionActionsAdapter::connected() const { return readiness_ && readiness_->connected(); }
UrArmReadiness SshUrMotionActionsAdapter::inspect(const std::string & robot, const std::string & side)
{ return readiness_->inspect(robot, side); }
bool SshUrMotionActionsAdapter::start(const UrMotionTask & t, QString * detail)
{
  static const QRegularExpression identifier("^[A-Za-z][A-Za-z0-9_]*$");
  static const QRegularExpression path("^(\\$HOME|~|/[A-Za-z0-9_./-]+)(/[A-Za-z0-9_./-]*)?$");
  const auto workspace = t.remote_workspace.trimmed().isEmpty() ? QString("$HOME/colcon_ws") : t.remote_workspace.trimmed();
  if (!processes_ || !identifier.match(t.robot).hasMatch() || !identifier.match(t.named_pose).hasMatch() ||
      (t.side != "l" && t.side != "r") || !path.match(workspace).hasMatch()) {
    *detail = "Invalid robot, pose, arm, or remote workspace"; return false;
  }
  QString ws = "'" + workspace + "'";
  if (workspace.startsWith("$HOME")) ws = "\"${HOME}" + workspace.mid(5) + "\"";
  else if (workspace.startsWith("~")) ws = "\"${HOME}" + workspace.mid(1) + "\"";
  QString command = "source /opt/ros/jazzy/setup.bash && source " + ws + "/install/setup.bash && ";
  if (t.kind == "Home") {
    command += QString("exec timeout --signal=TERM --kill-after=5s 90s ros2 run match_mur_gui move_arm_to_named_pose.py --ros-args -p robot_name:=%1 -p robot_profile:=%1 -p arm:=%2 -p group:=UR_arm_%2 -p named_pose:=%3 -p velocity_scaling:=%4")
      .arg(t.robot, t.side, t.named_pose).arg(t.velocity_scaling, 0, 'f', 3);
  } else if (t.kind == "Alignment") {
    const QString axis = t.plane == "XY" ? "Z" : t.plane == "XZ" ? "Y" : t.plane == "YZ" ? "X" : "";
    if (axis.isEmpty() || (t.direction != "+" + axis && t.direction != "-" + axis)) { *detail = "Invalid alignment"; return false; }
    const auto key = t.plane.toLower() + (t.direction.startsWith('+') ? "_negative" : "_positive");
    if (!std::isfinite(t.angular_velocity) || t.angular_velocity < 0.03 || t.angular_velocity > 0.30) { *detail = "Invalid angular velocity"; return false; }
    const double timeout = std::max(90.0, 1.875 * 3.141592653589793 / t.angular_velocity + 20.0);
    command += QString("exec timeout --signal=TERM --kill-after=5s %1s ros2 run match_mur_gui align_arm_to_plane.py --robot-name %2 --arm %3 --alignment %4 --max-angular-velocity %5 --timeout %6")
      .arg(static_cast<int>(std::ceil(timeout + 10))).arg(t.robot, t.side, key)
      .arg(t.angular_velocity, 0, 'f', 3).arg(timeout, 0, 'f', 1);
  } else { *detail = "Unknown action"; return false; }
  const bool ok = processes_->startRemoteOwnedCommand(t.name, t.host, t.ros_domain, command,
    QString("Starting %1 for %2/%3 on %4").arg(t.kind, t.robot, t.side, t.host));
  *detail = ok ? "Started" : "Unable to start remote owned process"; return ok;
}
void SshUrMotionActionsAdapter::stop(const QString & name) { if (processes_) processes_->stopProcess(name); }
UrMotionTaskStatus SshUrMotionActionsAdapter::status(const QString & name) const
{
  if (processes_) for (const auto & item : processes_->processes()) {
    const auto p = item.toMap();
    if (p.value("name").toString() == name) return {p.value("state").toString(), p.value("log").toString(), p.value("exitCode").toInt()};
  }
  return {"Failed", "Owned motion process not found", -1};
}
