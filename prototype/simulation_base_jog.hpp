#pragma once

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

struct SimBaseEndpoint
{
  std::string type;
  size_t subscribers{0};
};

class SimBaseJogAdapter
{
public:
  virtual ~SimBaseJogAdapter() = default;
  virtual bool connected() const = 0;
  virtual SimBaseEndpoint inspect(const std::string & topic) = 0;
  virtual void publish(const std::string & topic, const std::string & frame,
    double linear_x, double angular_z) = 0;
};

class RosSimBaseJogAdapter final : public SimBaseJogAdapter
{
public:
  explicit RosSimBaseJogAdapter(rclcpp::Node::SharedPtr node = {});
  void setNode(rclcpp::Node::SharedPtr node);
  bool connected() const override;
  SimBaseEndpoint inspect(const std::string & topic) override;
  void publish(const std::string & topic, const std::string & frame,
    double linear_x, double angular_z) override;

private:
  rclcpp::Node::SharedPtr node_;
  std::unordered_map<std::string,
    rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr> publishers_;
};

class SimulationBaseJog final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
  Q_PROPERTY(bool holding READ holding NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(double linearSpeed READ linearSpeed NOTIFY changed)
  Q_PROPERTY(double angularSpeed READ angularSpeed NOTIFY changed)

public:
  explicit SimulationBaseJog(std::shared_ptr<SimBaseJogAdapter> adapter, QObject * parent = nullptr);
  bool enabled() const { return enabled_; }
  bool holding() const { return holding_; }
  QString status() const { return status_; }
  double linearSpeed() const { return linear_speed_; }
  double angularSpeed() const { return angular_speed_; }

  Q_INVOKABLE void setContext(const QString & robot, const QString & mode,
    const QString & session_key, bool connected);
  Q_INVOKABLE void setEnabled(bool enabled);
  Q_INVOKABLE void setLinearSpeed(double speed);
  Q_INVOKABLE void setAngularSpeed(double speed);
  Q_INVOKABLE bool press(const QString & direction);
  Q_INVOKABLE void release();
  Q_INVOKABLE void stop(const QString & reason = "stop");

signals:
  void changed();

private slots:
  void tick();

private:
  bool send(double linear_x, double angular_z);
  void clearHold();
  void setStatus(const QString & status);

  std::shared_ptr<SimBaseJogAdapter> adapter_;
  QTimer timer_;
  QString robot_;
  QString context_mode_;
  QString context_key_;
  bool context_connected_{false};
  bool enabled_{false};
  bool holding_{false};
  double linear_x_{0.0};
  double angular_z_{0.0};
  double linear_speed_{0.08};
  double angular_speed_{0.2};
  QString status_{"Simulation jog disabled"};
};

bool run_sim_base_jog_contract_test();
