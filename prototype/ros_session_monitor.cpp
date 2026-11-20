#include "ros_session_monitor.hpp"
#include "log_store.hpp"

#include <algorithm>
#include <chrono>

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
    if (queried_manager_ != service.first) {
      controller_client_ = node_->create_client<controller_manager_msgs::srv::ListControllers>(service.first);
      queried_manager_ = service.first;
      controller_request_pending_ = false;
      controller_states_ = "waiting for response";
    }
    if (controller_client_ && !controller_request_pending_ && controller_client_->service_is_ready()) {
      auto request = std::make_shared<controller_manager_msgs::srv::ListControllers::Request>();
      controller_request_pending_ = true;
      controller_client_->async_send_request(request,
        [this](rclcpp::Client<controller_manager_msgs::srv::ListControllers>::SharedFuture future) {
          controller_request_pending_ = false;
          try {
            const auto response = future.get();
            QStringList states;
            for (const auto & controller : response->controller) {
              states << QString("%1: %2").arg(QString::fromStdString(controller.name),
                QString::fromStdString(controller.state));
            }
            controller_states_ = states.isEmpty() ? "no controllers reported" : states.join("; ");
          } catch (const std::exception & error) {
            controller_states_ = QString("query failed: %1").arg(error.what());
          }
        });
    }
    result << QString("Controller manager: %1 · %2")
      .arg(QString::fromStdString(service.first), QString::fromStdString(
        service.second.empty() ? "unknown type" : service.second.front()));
    result << "Controller states: " + controller_states_;
    found_manager = true;
  }
  if (!found_manager) {
    controller_client_.reset();
    queried_manager_.clear();
    result << "Controller managers: no list_controllers service discovered";
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
