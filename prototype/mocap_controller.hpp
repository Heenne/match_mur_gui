#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_srvs/srv/set_bool.hpp"
#include "tf2_msgs/msg/tf_message.hpp"

class ProcessManager;

class MocapController final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString pose READ pose NOTIFY changed)
  Q_PROPERTY(QString poseFrame READ poseFrame NOTIFY changed)
  Q_PROPERTY(QString poseStatus READ poseStatus NOTIFY changed)
  Q_PROPERTY(QString mapStatus READ mapStatus NOTIFY changed)
  Q_PROPERTY(double dataAgeSeconds READ dataAgeSeconds NOTIFY changed)
  Q_PROPERTY(double frequencyHz READ frequencyHz NOTIFY changed)
  Q_PROPERTY(bool frozen READ frozen NOTIFY changed)
  Q_PROPERTY(QString freezeStatus READ freezeStatus NOTIFY changed)
  Q_PROPERTY(QString bridgeState READ bridgeState NOTIFY changed)
  Q_PROPERTY(QString sshHost READ sshHost NOTIFY changed)
  Q_PROPERTY(QString qtmHost READ qtmHost NOTIFY changed)
  Q_PROPERTY(int qtmPort READ qtmPort NOTIFY changed)

public:
  explicit MocapController(QObject * parent = nullptr);
  ~MocapController() override = default;

  QString pose() const;
  QString poseFrame() const { return pose_frame_; }
  QString poseStatus() const { return pose_status_; }
  QString mapStatus() const { return map_status_; }
  double dataAgeSeconds() const { return data_age_seconds_; }
  double frequencyHz() const { return frequency_hz_; }
  bool frozen() const { return frozen_; }
  QString freezeStatus() const { return freeze_status_; }
  QString bridgeState() const { return bridge_state_; }
  QString sshHost() const { return ssh_host_; }
  QString qtmHost() const { return qtm_host_; }
  int qtmPort() const { return qtm_port_; }

  void setNode(rclcpp::Node::SharedPtr node);
  void setProcessManager(ProcessManager * manager);
  Q_INVOKABLE void setRobot(const QString & robot);
  Q_INVOKABLE void setBridgeSettings(const QString & ssh_host, const QString & qtm_host, int qtm_port);
  Q_INVOKABLE bool startBridge();
  Q_INVOKABLE void stopBridge();
  Q_INVOKABLE void setMapOutput(bool enabled);
  Q_INVOKABLE void setFrozen(bool frozen);

signals:
  void changed();

private slots:
  void refresh();

private:
  void resetSubscriptions();
  void acceptPose(const geometry_msgs::msg::PoseStamped & message, bool map_pose);
  void updateProcessState();
  bool externalBridgeDetected() const;

  rclcpp::Node::SharedPtr node_;
  ProcessManager * process_manager_{nullptr};
  QString robot_{"mur620d"};
  QString pose_text_{"No pose received"};
  QString pose_frame_;
  QString pose_status_{"Disconnected · connect to inspect Qualisys"};
  QString map_status_{"Not connected"};
  QString freeze_status_{"Not connected"};
  QString bridge_state_{"Stopped"};
  QString ssh_host_{"roscore"};
  QString qtm_host_{"QTM"};
  int qtm_port_{22223};
  double data_age_seconds_{-1.0};
  double frequency_hz_{0.0};
  bool frozen_{false};
  bool have_pose_{false};
  bool have_map_pose_{false};
  bool have_map_tf_{false};
  bool map_output_request_pending_{false};
  bool freeze_request_pending_{false};
  std::chrono::steady_clock::time_point last_pose_received_{};
  std::chrono::steady_clock::time_point last_map_pose_received_{};
  std::vector<double> pose_intervals_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr map_pose_subscription_;
  rclcpp::Subscription<tf2_msgs::msg::TFMessage>::SharedPtr static_tf_subscription_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr frozen_subscription_;
  rclcpp::Client<std_srvs::srv::SetBool>::SharedPtr map_output_client_;
  rclcpp::Client<std_srvs::srv::SetBool>::SharedPtr freeze_client_;
  QTimer refresh_timer_;
};
