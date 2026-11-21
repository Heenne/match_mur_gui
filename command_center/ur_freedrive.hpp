#pragma once
#include "ur_arm_jog.hpp"
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <functional>

class UrMotionActions;
class UrFreedriveAdapter {
public:
  virtual ~UrFreedriveAdapter() = default;
  virtual bool connected() const = 0;
  virtual bool safeFailure() const { return false; }
  virtual UrArmReadiness inspect(const std::string &, const std::string &) = 0;
  virtual void enter(const QString &, const QString &, std::function<void(bool, QString)> done) = 0;
  virtual void leave(std::function<void(bool, QString)> done) = 0;
  virtual bool publish(bool enabled) = 0;
};
class UrFreedrive : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString selectedArm READ selectedArm NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool active READ active NOTIFY changed)
public:
  explicit UrFreedrive(std::shared_ptr<UrFreedriveAdapter>, QObject * parent = nullptr);
  QString selectedArm() const { return side_ == "l" ? "UR10_l" : "UR10_r"; }
  QString robot() const { return robot_; }
  QString side() const { return side_; }
  QString status() const { return status_; }
  bool busy() const { return state_ != Idle; }
  bool active() const { return state_ == Active; }
  void setJog(UrArmJog * jog) { jog_ = jog; }
  void setMotion(UrMotionActions * motion) { motion_ = motion; }
  void setShutdownInterlock(std::function<bool(const QString &, const QString &)> blocked) { shutdown_blocked_ = std::move(blocked); }
  Q_INVOKABLE void setContext(const QString &, const QString &, const QString &, bool, bool, bool);
  Q_INVOKABLE void selectArm(const QString &);
  Q_INVOKABLE bool enter();
  Q_INVOKABLE void leave();
signals:
  void changed();
private:
  enum State { Idle, Entering, WaitingEndpoint, Active, Leaving, Unconfirmed };
  void applyPendingContext();
  std::shared_ptr<UrFreedriveAdapter> adapter_;
  UrArmJog * jog_{};
  UrMotionActions * motion_{};
  std::function<bool(const QString &, const QString &)> shutdown_blocked_;
  QTimer timer_;
  QElapsedTimer endpoint_wait_;
  QString robot_, mode_, key_, side_{"r"}, status_{"Freedrive inactive"};
  bool connected_{false}, left_{false}, right_{false}, cancel_{false};
  QString pending_robot_, pending_mode_, pending_key_;
  bool pending_context_{false}, pending_connected_{false}, pending_left_{false}, pending_right_{false};
  State state_{Idle};
};
bool run_ur_freedrive_contract_test();

class RosUrFreedriveAdapter final : public QObject, public UrFreedriveAdapter {
public:
 explicit RosUrFreedriveAdapter(std::shared_ptr<UrArmJogAdapter>, QObject * parent = nullptr);
 ~RosUrFreedriveAdapter() override;
 void setNode(rclcpp::Node::SharedPtr node);
 bool connected() const override;
 bool safeFailure() const override;
 UrArmReadiness inspect(const std::string &, const std::string &) override;
 void enter(const QString &, const QString &, std::function<void(bool, QString)>) override;
 void leave(std::function<void(bool, QString)>) override;
 bool publish(bool) override;
private:
 class State;
 std::shared_ptr<State> state_;
};
