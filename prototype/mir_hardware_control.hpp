#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "mir_srvs/srv/color_rgb.hpp"
#include "std_srvs/srv/trigger.hpp"

struct MirEndpoint
{
  std::string type;
  size_t subscribers{0};
};

class MirHardwareAdapter
{
public:
  using Result = std::function<void(bool, const QString &)>;
  virtual ~MirHardwareAdapter() = default;
  virtual bool connected() const = 0;
  virtual MirEndpoint inspectTopic(const std::string & topic) = 0;
  virtual void publishTwist(const std::string & topic, bool stamped,
    const std::string & frame, double linear_x, double angular_z) = 0;
  virtual bool hasService(const std::string & service, const std::string & type) = 0;
  virtual void callTrigger(const std::string & service, Result result) = 0;
  virtual void setColor(const std::string & service, int red, int green, int blue, Result result) = 0;
};

class RosMirHardwareAdapter final : public MirHardwareAdapter
{
public:
  explicit RosMirHardwareAdapter(rclcpp::Node::SharedPtr node = {});
  void setNode(rclcpp::Node::SharedPtr node);
  bool connected() const override;
  MirEndpoint inspectTopic(const std::string & topic) override;
  void publishTwist(const std::string & topic, bool stamped,
    const std::string & frame, double linear_x, double angular_z) override;
  bool hasService(const std::string & service, const std::string & type) override;
  void callTrigger(const std::string & service, Result result) override;
  void setColor(const std::string & service, int red, int green, int blue, Result result) override;

private:
  rclcpp::Node::SharedPtr node_;
  std::unordered_map<std::string,
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr> twist_publishers_;
  std::unordered_map<std::string,
    rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr> stamped_publishers_;
};

class MirHardwareControl final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool armed READ armed NOTIFY changed)
  Q_PROPERTY(bool holding READ holding NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString lightStatus READ lightStatus NOTIFY changed)
  Q_PROPERTY(double linearSpeed READ linearSpeed NOTIFY changed)
  Q_PROPERTY(double angularSpeed READ angularSpeed NOTIFY changed)

public:
  explicit MirHardwareControl(std::shared_ptr<MirHardwareAdapter> adapter, QObject * parent = nullptr);
  bool armed() const { return armed_; }
  bool holding() const { return holding_; }
  QString status() const { return status_; }
  QString lightStatus() const { return light_status_; }
  double linearSpeed() const { return linear_speed_; }
  double angularSpeed() const { return angular_speed_; }

  Q_INVOKABLE void setContext(const QString & robot, const QString & mode,
    const QString & target, bool connected);
  Q_INVOKABLE void setArmed(bool armed);
  Q_INVOKABLE void setLinearSpeed(double speed);
  Q_INVOKABLE void setAngularSpeed(double speed);
  Q_INVOKABLE bool press(const QString & direction);
  Q_INVOKABLE void release();
  Q_INVOKABLE void stop(const QString & reason = "stop");
  Q_INVOKABLE void rainbowStart();
  Q_INVOKABLE void rainbowStop();
  Q_INVOKABLE void matchColor();
  Q_INVOKABLE void setSolidColor(int red, int green, int blue);

signals:
  void changed();

private slots:
  void tick();

private:
  bool selectEndpoint();
  bool send(double linear_x, double angular_z);
  void callLight(const QString & suffix, const QString & type, bool solid,
    int red = 0, int green = 0, int blue = 0);
  void clearHold();
  void setStatus(const QString & status);
  void setLightStatus(const QString & status);
  std::string serviceName(const QString & suffix) const;

  std::shared_ptr<MirHardwareAdapter> adapter_;
  QTimer timer_;
  QTimer light_timeout_timer_;
  QString robot_;
  QString target_;
  QString context_key_;
  bool hardware_mode_{false};
  bool context_connected_{false};
  bool armed_{false};
  bool holding_{false};
  std::string command_topic_;
  bool command_stamped_{false};
  double linear_x_{0.0};
  double angular_z_{0.0};
  double linear_speed_{0.05};
  double angular_speed_{0.2};
  QString status_{"MiR jog disarmed"};
  QString light_status_{"MiR light services not checked"};
  uint64_t light_request_id_{0};
};

bool run_mir_hardware_control_contract_test();
