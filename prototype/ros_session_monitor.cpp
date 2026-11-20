#include "ros_session_monitor.hpp"
#include "log_store.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>

#include <QString>

RosSessionMonitor::RosSessionMonitor(const rclcpp::Node::SharedPtr & node, LogStore * logs)
: node_(node)
{
  rosout_subscription_ = node_->create_subscription<rcl_interfaces::msg::Log>(
    "/rosout", rclcpp::QoS(1000), [logs](rcl_interfaces::msg::Log::ConstSharedPtr message) {
      if (!logs) return;
      QString level = "INFO";
      switch (message->level) {
        case rcl_interfaces::msg::Log::DEBUG: level = "DEBUG"; break;
        case rcl_interfaces::msg::Log::WARN: level = "WARN"; break;
        case rcl_interfaces::msg::Log::ERROR: level = "ERROR"; break;
        case rcl_interfaces::msg::Log::FATAL: level = "FATAL"; break;
        default: break;
      }
      logs->append("ROS /rosout", level,
        QString("%1 (%2): %3").arg(QString::fromStdString(message->name),
          QString::fromStdString(message->file + ":" + std::to_string(message->line)),
          QString::fromStdString(message->msg)));
    });
  clock_subscription_ = node_->create_subscription<rosgraph_msgs::msg::Clock>(
    "/clock", rclcpp::QoS(10), [this](rosgraph_msgs::msg::Clock::ConstSharedPtr message) {
      const int64_t now = static_cast<int64_t>(message->clock.sec) * 1000000000LL +
        message->clock.nanosec;
      const auto received = std::chrono::steady_clock::now();
      if (clock_samples_ == 0 || now > last_clock_ns_) last_clock_advance_ = received;
      last_clock_received_ = received;
      last_clock_ns_ = now;
      ++clock_samples_;
    });
}

void RosSessionMonitor::configure_hardware_subscriptions(const QString & robot)
{
  const auto name = robot.toStdString();
  if (hardware_robot_ == name) return;
  hardware_robot_ = name;
  hardware_subscriptions_.clear();
  joint_states_subscription_.reset();

  const auto battery_topic = [name](const std::string & suffix) {
      return "/" + name + "/" + suffix;
    };
  auto mur_state = battery_topic("bms_status/battery_state");
  hardware_status_.expectBattery("MuR", QString::fromStdString(mur_state));
  hardware_status_.expectBattery("MiR", QString::fromStdString(battery_topic("battery_state")));
  hardware_subscriptions_.push_back(node_->create_subscription<sensor_msgs::msg::BatteryState>(
      mur_state, rclcpp::QoS(10), [this, mur_state](sensor_msgs::msg::BatteryState::ConstSharedPtr msg) {
        hardware_status_.updateBattery("MuR", QString::fromStdString(mur_state), msg->percentage * 100.0,
          msg->voltage, msg->current, msg->charge, msg->capacity);
      }));
  auto mur_soc = battery_topic("bms_status/SOC");
  hardware_subscriptions_.push_back(node_->create_subscription<std_msgs::msg::Float32>(
      mur_soc, rclcpp::QoS(10), [this, mur_soc](std_msgs::msg::Float32::ConstSharedPtr msg) {
        hardware_status_.updateSoc(QString::fromStdString(mur_soc), msg->data);
      }));
  auto mir_topic = battery_topic("battery_state");
  hardware_subscriptions_.push_back(node_->create_subscription<sensor_msgs::msg::BatteryState>(
      mir_topic, rclcpp::QoS(10), [this, mir_topic](sensor_msgs::msg::BatteryState::ConstSharedPtr msg) {
        hardware_status_.updateBattery("MiR", QString::fromStdString(mir_topic), msg->percentage * 100.0,
          msg->voltage, msg->current, msg->charge, msg->capacity);
      }));
  auto mir_remaining = battery_topic("battery_time_remaining");
  hardware_subscriptions_.push_back(node_->create_subscription<std_msgs::msg::Int32>(
      mir_remaining, rclcpp::QoS(10), [this, mir_remaining](std_msgs::msg::Int32::ConstSharedPtr msg) {
        hardware_status_.updateRemainingEstimate(QString::fromStdString(mir_remaining), msg->data);
      }));
  joint_states_subscription_ = node_->create_subscription<sensor_msgs::msg::JointState>(
    "/" + name + "/joint_states", rclcpp::QoS(10), [this](sensor_msgs::msg::JointState::ConstSharedPtr msg) {
      hardware_status_.updateLift(msg->name, msg->position);
    });
  controller_states_by_manager_.clear();
  controller_requests_pending_.clear();
  controller_clients_.clear();
}

bool RosSessionMonitor::simulationServerAvailable() const
{
  const auto graph = node_->get_node_graph_interface();
  const auto topics = graph->get_topic_names_and_types();
  const auto clock = topics.find("/clock");
  return clock != topics.end() &&
    std::find(clock->second.begin(), clock->second.end(), "rosgraph_msgs/msg/Clock") != clock->second.end() &&
    graph->count_publishers("/clock") > 0;
}

QStringList RosSessionMonitor::diagnose(const QString & target, const QString & robot,
  const QString & mode, int domain)
{
  const auto graph = node_->get_node_graph_interface();
  const auto topics = graph->get_topic_names_and_types();
  const auto services = graph->get_service_names_and_types();
  const auto graph_nodes = graph->get_node_names_and_namespaces();
  QStringList result;
  result << QString("Target: %1 · ROS domain %2").arg(target).arg(domain);

  size_t remote_nodes = 0;
  for (const auto & entry : graph_nodes) {
    if (entry.first != node_->get_name() || entry.second != node_->get_namespace()) ++remote_nodes;
  }
  result << (remote_nodes == 0 ? "ROS graph: no remote nodes discovered in this domain" :
    QString("ROS graph: %1 remote node(s) discovered").arg(remote_nodes));

  if (mode == "Hardware") configure_hardware_subscriptions(robot);

  const std::string clock_name = "/clock";
  const auto clock = topics.find(clock_name);
  if (mode != "Simulation") {
    result << "Clock: not required in Hardware mode · using system time";
  } else if (clock == topics.end() || clock->second.empty() || graph->count_publishers(clock_name) == 0) {
    result << "Clock: missing (/clock has no publisher)";
  } else if (std::find(clock->second.begin(), clock->second.end(), "rosgraph_msgs/msg/Clock") == clock->second.end()) {
    result << QString("Clock: wrong type (%1)").arg(QString::fromStdString(clock->second.front()));
  } else if (clock_samples_ == 0) {
    result << "Clock: /clock exists, waiting for first message";
  } else {
    const auto age = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - last_clock_received_).count();
    const auto since_advance = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - last_clock_advance_).count();
    const auto state = age > 2.0 ? "stale" : (since_advance <= 2.0 ? "advancing" : "paused");
    result << QString("Clock: %1 · simulation time %2 s · last message %3 s ago")
      .arg(state).arg(static_cast<double>(last_clock_ns_) / 1e9, 0, 'f', 3).arg(age, 0, 'f', 1);
  }

  const auto expected_topic = "/" + robot.toStdString() + "/mobile_base_controller/cmd_vel";
  const auto cmd = topics.find(expected_topic);
  if (cmd == topics.end()) {
    result << QString("Base command: missing %1 (expected geometry_msgs/msg/TwistStamped)")
      .arg(QString::fromStdString(expected_topic));
  } else {
    const auto expected = std::find(cmd->second.begin(), cmd->second.end(), "geometry_msgs/msg/TwistStamped");
    result << (expected != cmd->second.end() ?
      QString("Base command: %1 · geometry_msgs/msg/TwistStamped · %2 publisher(s), %3 subscriber(s)")
        .arg(QString::fromStdString(expected_topic))
        .arg(graph->count_publishers(expected_topic)).arg(graph->count_subscribers(expected_topic)) :
      QString("Base command: wrong type on %1 · found %2, expected geometry_msgs/msg/TwistStamped")
        .arg(QString::fromStdString(expected_topic), QString::fromStdString(cmd->second.front())));
  }

  bool found_action = false;
  for (const auto & topic : topics) {
    if (topic.first.find("/_action/") != std::string::npos) {
      result << QString("Action endpoint: %1 · %2")
        .arg(QString::fromStdString(topic.first), QString::fromStdString(
          topic.second.empty() ? "unknown type" : topic.second.front()));
      found_action = true;
    }
  }
  if (!found_action) result << "Actions: no ROS action endpoint topics discovered";

  bool found_manager = false;
  std::set<std::string> available_controller_managers;
  for (const auto & service : services) {
    if (service.first.find("/controller_manager/list_controllers") == std::string::npos) continue;
    const auto expected_manager_type = std::find(service.second.begin(), service.second.end(),
      "controller_manager_msgs/srv/ListControllers");
    if (expected_manager_type == service.second.end()) {
      result << QString("Controller manager: wrong type on %1 · found %2, expected controller_manager_msgs/srv/ListControllers")
        .arg(QString::fromStdString(service.first), QString::fromStdString(
          service.second.empty() ? "unknown type" : service.second.front()));
      found_manager = true;
      continue;
    }
    available_controller_managers.insert(service.first);
    auto & client = controller_clients_[service.first];
    if (!client) client = node_->create_client<controller_manager_msgs::srv::ListControllers>(service.first);
    auto & pending = controller_requests_pending_[service.first];
    if (!pending && client->service_is_ready()) {
      auto request = std::make_shared<controller_manager_msgs::srv::ListControllers::Request>();
      pending = true;
      const auto manager_name = service.first;
      client->async_send_request(request,
        [this, manager_name](rclcpp::Client<controller_manager_msgs::srv::ListControllers>::SharedFuture future) {
          controller_requests_pending_[manager_name] = false;
          try {
            const auto response = future.get();
            QStringList states;
            for (const auto & controller : response->controller) {
              states << QString("%1: %2").arg(QString::fromStdString(controller.name),
                QString::fromStdString(controller.state));
            }
            controller_states_by_manager_[manager_name] = states.isEmpty() ? "no controllers reported" : states.join("; ");
          } catch (const std::exception & error) {
            controller_states_by_manager_[manager_name] = QString("query failed: %1").arg(error.what());
          }
        });
    }
    result << QString("Controller manager: %1 · %2")
      .arg(QString::fromStdString(service.first), QString::fromStdString(
        service.second.empty() ? "unknown type" : service.second.front()));
    result << QString("Controller states (%1): %2").arg(QString::fromStdString(service.first),
      controller_states_by_manager_.count(service.first) ? controller_states_by_manager_[service.first] : "waiting for response");
    found_manager = true;
  }
  if (!found_manager) {
    result << "Controller managers: no list_controllers service discovered";
  }

  if (mode == "Hardware") {
    const auto robot_name = robot.toStdString();
    const auto root_manager = "/" + robot_name + "/controller_manager/list_controllers";
    const auto controller_status = [this, &available_controller_managers](const std::string & manager) {
        if (!available_controller_managers.count(manager)) {
          return QString("not reported · list_controllers service missing");
        }
        auto state = controller_states_by_manager_.find(manager);
        return state == controller_states_by_manager_.end() ? QString("not reported · %1 unavailable or waiting")
          .arg(QString::fromStdString(manager)) : state->second;
      };
    result << "Hardware component base: " + controller_status(root_manager);
    result << "Hardware component left arm: " + controller_status(
      "/" + robot_name + "/UR10_l/controller_manager/list_controllers");
    result << "Hardware component right arm: " + controller_status(
      "/" + robot_name + "/UR10_r/controller_manager/list_controllers");
    result << hardware_status_.diagnostics(robot);
  }

  for (const auto & topic : topics) {
    if (topic.first == clock_name || topic.first == expected_topic) continue;
    for (const auto & type : topic.second) {
      result << QString("Topic: %1 · %2 · %3 publisher(s), %4 subscriber(s)")
        .arg(QString::fromStdString(topic.first), QString::fromStdString(type))
        .arg(graph->count_publishers(topic.first)).arg(graph->count_subscribers(topic.first));
    }
  }
  for (const auto & service : services) {
    if (service.first.find("/controller_manager/list_controllers") != std::string::npos) continue;
    for (const auto & type : service.second) {
      result << QString("Service: %1 · %2").arg(QString::fromStdString(service.first), QString::fromStdString(type));
    }
  }
  return result;
}
