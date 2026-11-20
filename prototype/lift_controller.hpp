#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

#include "ewellix_interfaces/msg/command.hpp"
#include "ewellix_interfaces/msg/state.hpp"
#include "rclcpp/rclcpp.hpp"

struct LiftMeasurement
{
  bool received{false};
  double meters{0.0};
  double age_seconds{-1.0};
};

struct LiftCommandEndpoint
{
  std::string type;
  size_t subscribers{0};
};

class LiftAdapter
{
public:
  virtual ~LiftAdapter() = default;
  virtual bool connected() const = 0;
  virtual LiftMeasurement measurement(const std::string & robot, const std::string & side) = 0;
  virtual LiftCommandEndpoint commandEndpoint(const std::string & robot, const std::string & side) = 0;
  virtual bool sendTarget(const std::string & robot, const std::string & side, double meters) = 0;
};

class RosLiftAdapter final : public LiftAdapter
{
public:
  explicit RosLiftAdapter(rclcpp::Node::SharedPtr node = {});
  void setNode(rclcpp::Node::SharedPtr node);
  bool connected() const override;
  LiftMeasurement measurement(const std::string & robot, const std::string & side) override;
  LiftCommandEndpoint commandEndpoint(const std::string & robot, const std::string & side) override;
  bool sendTarget(const std::string & robot, const std::string & side, double meters) override;

private:
  struct StateSample
  {
    double meters{0.0};
    std::chrono::steady_clock::time_point received{};
  };
  void subscribe(const std::string & robot, const std::string & side);
  rclcpp::Node::SharedPtr node_;
  std::unordered_map<std::string,
    rclcpp::Subscription<ewellix_interfaces::msg::State>::SharedPtr> state_subscriptions_;
  std::unordered_map<std::string, StateSample> states_;
  std::unordered_map<std::string,
    rclcpp::Publisher<ewellix_interfaces::msg::Command>::SharedPtr> command_publishers_;
};

class LiftController final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString side READ side NOTIFY changed)
  Q_PROPERTY(double heightMeters READ heightMeters NOTIFY changed)
  Q_PROPERTY(double ageSeconds READ ageSeconds NOTIFY changed)
  Q_PROPERTY(double stepMeters READ stepMeters NOTIFY changed)
  Q_PROPERTY(double minimumMeters READ minimumMeters NOTIFY changed)
  Q_PROPERTY(double maximumMeters READ maximumMeters NOTIFY changed)
  Q_PROPERTY(bool canLiftUp READ canLiftUp NOTIFY changed)
  Q_PROPERTY(bool canLiftDown READ canLiftDown NOTIFY changed)
  Q_PROPERTY(bool holding READ holding NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)

public:
  explicit LiftController(std::shared_ptr<LiftAdapter> adapter, QObject * parent = nullptr);
  ~LiftController() override;

  QString side() const { return side_; }
  double heightMeters() const { return height_meters_; }
  double ageSeconds() const { return age_seconds_; }
  double stepMeters() const { return step_meters_; }
  double minimumMeters() const { return minimum_meters_; }
  double maximumMeters() const { return maximum_meters_; }
  bool canLiftUp() const { return can_lift_up_; }
  bool canLiftDown() const { return can_lift_down_; }
  bool holding() const { return holding_; }
  QString status() const { return status_; }

  Q_INVOKABLE void setContext(const QString & robot, const QString & mode,
    const QString & target, bool connected, const QString & preset = QString());
  Q_INVOKABLE void selectSide(const QString & side);
  Q_INVOKABLE void setStepMeters(double meters);
  Q_INVOKABLE void setLimits(double minimum, double maximum);
  Q_INVOKABLE bool press(const QString & direction);
  Q_INVOKABLE void release();
  Q_INVOKABLE void stop(const QString & reason = QStringLiteral("stop"));
  Q_INVOKABLE void refresh();

signals:
  void changed();

private slots:
  void tick();

private:
  bool ready(QString * reason = nullptr);
  bool step(const QString & direction);
  bool haltOwnedCommand(const QString & reason);
  void loadPresetSettings(const QString & preset);
  void savePresetSettings() const;
  void updateAvailability();
  void setStatus(const QString & status);

  std::shared_ptr<LiftAdapter> adapter_;
  QTimer repeat_timer_;
  QTimer refresh_timer_;
  QString robot_;
  QString target_;
  QString preset_;
  QString mode_;
  QString side_{"left"};
  QString holding_direction_;
  double height_meters_{0.0};
  double age_seconds_{-1.0};
  double step_meters_{0.01};
  double minimum_meters_{0.005};
  double maximum_meters_{0.260};
  bool have_height_{false};
  bool can_lift_up_{false};
  bool can_lift_down_{false};
  bool holding_{false};
  bool context_connected_{false};
  QString status_{"Disconnected · choose and connect a hardware target"};
};

bool run_lift_controller_contract_test();
