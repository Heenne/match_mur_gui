#pragma once

#include <QStringList>
#include <memory>
#include <chrono>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "rosgraph_msgs/msg/clock.hpp"
#include "controller_manager_msgs/srv/list_controllers.hpp"
#include "rcl_interfaces/msg/log.hpp"

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
  rclcpp::Client<controller_manager_msgs::srv::ListControllers>::SharedPtr controller_client_;
  bool controller_request_pending_{false};
  QString controller_states_{"waiting for response"};
  std::string queried_manager_;
};
