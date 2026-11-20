#pragma once

#include <memory>
#include <chrono>
#include <future>
#include <unordered_map>

#include <QObject>
#include <QVariant>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "actionlib_msgs/msg/goal_id.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

class GoalsPosesAdapter : public QObject
{
  Q_OBJECT
public:
  using QObject::QObject;
  virtual ~GoalsPosesAdapter() = default;
  virtual bool connected() const = 0;
  virtual bool sendGoal(const QString & robot, const QString & mode, const QString & frame, double x, double y, double yaw,
    QString * detail) = 0;
  virtual bool cancel(const QString & robot, const QString & mode, QString * detail) = 0;
  virtual bool currentPose(const QString & robot, const QString & mode, double * x, double * y, double * yaw,
    QString * frame, double * ageSeconds, QString * detail) = 0;
  virtual QString adapterStatus(const QString & robot, const QString & mode) const = 0;
  virtual void poll() = 0;

signals:
  void operationCompleted(const QString & detail, bool error);
};

class RosGoalsPosesAdapter final : public GoalsPosesAdapter
{
public:
  explicit RosGoalsPosesAdapter(rclcpp::Node::SharedPtr node = {});
  void setNode(rclcpp::Node::SharedPtr node);
  bool connected() const override;
  bool sendGoal(const QString & robot, const QString & mode, const QString & frame, double x, double y, double yaw,
    QString * detail) override;
  bool cancel(const QString & robot, const QString & mode, QString * detail) override;
  bool currentPose(const QString & robot, const QString & mode, double * x, double * y, double * yaw,
    QString * frame, double * ageSeconds, QString * detail) override;
  QString adapterStatus(const QString & robot, const QString & mode) const override;
  void poll() override;

private:
  struct PoseSample {
    geometry_msgs::msg::Pose pose;
    std::chrono::steady_clock::time_point received;
  };
  using NavigateToPose = nav2_msgs::action::NavigateToPose;
  using NavigateGoalHandle = rclcpp_action::ClientGoalHandle<NavigateToPose>;
  struct SimPoseSample {
    geometry_msgs::msg::PoseStamped pose;
    std::chrono::steady_clock::time_point received;
  };
  rclcpp::Node::SharedPtr node_;
  std::unordered_map<std::string, rclcpp_action::Client<NavigateToPose>::SharedPtr> nav2_clients_;
  std::unordered_map<std::string, NavigateGoalHandle::SharedPtr> nav2_goals_;
  std::unordered_map<std::string, std::shared_future<action_msgs::srv::CancelGoal::Response::SharedPtr>> nav2_cancels_;
  std::unordered_map<std::string, rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr> sim_poses_;
  std::unordered_map<std::string, SimPoseSample> latest_sim_poses_;
  std::unordered_map<std::string, rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr> goals_;
  std::unordered_map<std::string, rclcpp::Publisher<actionlib_msgs::msg::GoalID>::SharedPtr> cancels_;
  std::unordered_map<std::string, rclcpp::Subscription<geometry_msgs::msg::Pose>::SharedPtr> poses_;
  std::unordered_map<std::string, PoseSample> latest_poses_;
};

class GoalsPoses final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariantList poses READ poses NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
  explicit GoalsPoses(std::shared_ptr<GoalsPosesAdapter> adapter, QString storage_path,
    QObject * parent = nullptr);
  QVariantList poses() const;
  QString status() const { return status_; }
  bool busy() const { return busy_; }
  void setConnected(bool connected);

  Q_INVOKABLE void reload();
  Q_INVOKABLE bool importFile(const QString & pathOrUrl);
  Q_INVOKABLE bool save(const QString & name, const QString & frame, double x, double y, double yawDegrees);
  Q_INVOKABLE bool remove(const QString & name);
  Q_INVOKABLE bool send(const QString & robot, const QString & mode, const QString & name,
    const QString & frame, double x, double y, double yawDegrees);
  Q_INVOKABLE bool sendSaved(const QString & robot, const QString & mode, const QString & name);
  Q_INVOKABLE bool saveCurrent(const QString & robot, const QString & mode, const QString & name);
  Q_INVOKABLE void cancelSelected(const QString & robot, const QString & mode);
  Q_INVOKABLE QString adapterStatus(const QString & robot, const QString & mode) const;

signals:
  void changed();

private:
  bool write();
  bool validPose(const QString & name, const QString & frame, double x, double y, double yawDegrees,
    QString * error) const;
  void setStatus(const QString & text);
  QJsonArray poses_;
  std::shared_ptr<GoalsPosesAdapter> adapter_;
  QString storage_path_;
  QString status_{"Disconnected · goals require a connected Hardware profile"};
  bool connected_{false};
  bool busy_{false};
  QTimer current_pose_timer_;
  QTimer adapter_poll_timer_;
  QString pending_robot_;
  QString pending_pose_name_;
  QString pending_mode_;
  int current_pose_attempts_{0};
};

bool run_goals_poses_contract_test();
