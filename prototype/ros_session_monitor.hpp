#pragma once

#include <QStringList>
#include <memory>
#include <chrono>
#include <string>
#include <map>
#include <vector>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "rosgraph_msgs/msg/clock.hpp"
#include "controller_manager_msgs/srv/list_controllers.hpp"
#include "rcl_interfaces/msg/log.hpp"
#include "sensor_msgs/msg/battery_state.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/int32.hpp"
#include "hardware_status.hpp"

class LogStore;

class RosSessionMonitor
{
public:
  explicit RosSessionMonitor(const rclcpp::Node::SharedPtr & node, LogStore * logs = nullptr);
  QStringList diagnose(const QString & target, const QString & robot, const QString & mode, int domain);
  bool simulationServerAvailable() const;

private:
  rclcpp::Node::SharedPtr node_;
  rclcpp::Subscription<rosgraph_msgs::msg::Clock>::SharedPtr clock_subscription_;
  rclcpp::Subscription<rcl_interfaces::msg::Log>::SharedPtr rosout_subscription_;
  int clock_samples_{0};
  int64_t last_clock_ns_{0};
  std::chrono::steady_clock::time_point last_clock_received_{};
  std::chrono::steady_clock::time_point last_clock_advance_{};
  void configure_hardware_subscriptions(const QString & robot);
  std::string hardware_robot_;
  HardwareStatus hardware_status_;
  std::vector<rclcpp::SubscriptionBase::SharedPtr> hardware_subscriptions_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_states_subscription_;
  std::map<std::string, QString> controller_states_by_manager_;
  std::map<std::string, bool> controller_requests_pending_;
  std::map<std::string, rclcpp::Client<controller_manager_msgs::srv::ListControllers>::SharedPtr>
    controller_clients_;
};
