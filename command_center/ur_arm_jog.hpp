#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"

struct UrArmReadiness
{
  bool connected{false};
  bool external_control{false};
  bool remote_control{false};
  bool reverse_interface{false};
  bool cartesian_controller_active{false};
  bool joint_state_fresh{false};
  bool command_endpoint_ready{false};
  QString external_control_detail{"not checked"};
  QString remote_control_detail{"not checked"};
  QString reverse_interface_detail{"not checked"};
  QString controller_detail{"not checked"};
  QString joint_state_detail{"not checked"};
  QString command_endpoint_detail{"not checked"};

  bool ready() const;
  QString summary(const QString & side) const;
};

class UrArmJogAdapter
{
public:
  virtual ~UrArmJogAdapter() = default;
  virtual bool connected() const = 0;
  virtual UrArmReadiness inspect(const std::string & robot, const std::string & side) = 0;
  virtual void publish(const std::string & robot, const std::string & side,
    const std::array<double, 6> & twist) = 0;
};

class RosUrArmJogAdapter final : public UrArmJogAdapter
{
public:
  explicit RosUrArmJogAdapter(rclcpp::Node::SharedPtr node = {});
  ~RosUrArmJogAdapter() override;
  void setNode(rclcpp::Node::SharedPtr node);
  bool connected() const override;
  UrArmReadiness inspect(const std::string & robot, const std::string & side) override;
  void publish(const std::string & robot, const std::string & side,
    const std::array<double, 6> & twist) override;

private:
  class State;
  rclcpp::Node::SharedPtr node_;
  std::unique_ptr<State> state_;
  std::unordered_map<std::string,
    rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr> publishers_;
};

class UrArmJog final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString selectedArm READ selectedArm NOTIFY changed)
  Q_PROPERTY(QString readiness READ readiness NOTIFY changed)
  Q_PROPERTY(bool ready READ ready NOTIFY changed)
  Q_PROPERTY(bool armed READ armed NOTIFY changed)
  Q_PROPERTY(bool holding READ holding NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(double linearSpeed READ linearSpeed NOTIFY changed)
  Q_PROPERTY(double angularSpeed READ angularSpeed NOTIFY changed)

public:
  explicit UrArmJog(std::shared_ptr<UrArmJogAdapter> adapter, QObject * parent = nullptr);
  QString selectedArm() const { return side_ == "l" ? "UR10_l" : "UR10_r"; }
  QString readiness() const { return readiness_; }
  bool ready() const { return readiness_ok_; }
  bool armed() const { return armed_; }
  bool holding() const { return holding_; }
  QString status() const { return status_; }
  double linearSpeed() const { return linear_speed_; }
  double angularSpeed() const { return angular_speed_; }

  Q_INVOKABLE void setContext(const QString & robot, const QString & mode,
    const QString & sessionKey, bool connected);
  Q_INVOKABLE void selectArm(const QString & side);
  void setMotionBlocked(bool blocked);
  void setShutdownInterlock(std::function<bool(const QString &, const QString &)> fn) { shutdown_interlock_ = std::move(fn); }
  QString robot() const { return robot_; }
  QString side() const { return side_; }
  Q_INVOKABLE void setArmed(bool armed);
  Q_INVOKABLE void setLinearSpeed(double speed);
  Q_INVOKABLE void setAngularSpeed(double speed);
  Q_INVOKABLE void refresh();
  Q_INVOKABLE bool press(const QString & axis);
  Q_INVOKABLE void release();
  Q_INVOKABLE void stop(const QString & reason = "operator stop");

signals:
  void changed();

private slots:
  void tick();

private:
  bool send(const std::array<double, 6> & twist);
  void clearHold();
  void setStatus(const QString & status);

  std::shared_ptr<UrArmJogAdapter> adapter_;
  QTimer timer_;
  QString robot_;
  QString mode_;
  QString context_key_;
  QString side_{"r"};
  QString readiness_{"Select a connected hardware session and arm"};
  QString status_{"Arm jog disarmed"};
  bool context_connected_{false};
  bool readiness_ok_{false};
  bool motion_blocked_{false};
  std::function<bool(const QString &, const QString &)> shutdown_interlock_;
  bool armed_{false};
  bool holding_{false};
  double linear_speed_{0.01};
  double angular_speed_{0.05};
  std::array<double, 6> twist_{};
};

bool run_ur_arm_jog_contract_test();
