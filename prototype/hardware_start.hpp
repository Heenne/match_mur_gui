#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QMap>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <chrono>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "std_msgs/msg/string.hpp"

#include <functional>
#include <memory>

struct HardwareStartConfig
{
  QString preset;
  QString mode;
  QString host;
  int ros_domain{62};
  QString robot;
  QString local_workspace;
  QString remote_workspace;
  QStringList arms;
  bool launch_mir{false};
  bool launch_cameras{false};
  bool integrated_controller{true};
  bool use_ft_sensor{true};
  bool require_wrench{false};
  bool collision_avoidance{true};
  bool collision_markers{false};
  bool zero_admittance{false};
  bool launch_moveit{true};
  int moveit_speed_percent{20};
  QString localization_source{"mir"};
};
struct HardwareArmCheck
{
  QString state{"Unknown"};
  QString detail{"Safety state was not confirmed"};
};

struct HardwarePreflightResult
{
  bool host_ready{false};
  QString host_state{"Unknown"};
  QString host_detail{"Host preflight did not confirm readiness"};
  QMap<QString, HardwareArmCheck> arms;
};

struct HardwareActionResult
{
  bool accepted{false};
  QString detail;
};

class HardwareStartAdapter
{
public:
  using PreflightCallback = std::function<void(const HardwarePreflightResult &)>;
  using ActionCallback = std::function<void(const HardwareActionResult &)>;
  virtual ~HardwareStartAdapter() = default;
  virtual void runPreflight(const HardwareStartConfig & config, PreflightCallback callback) = 0;
  virtual void startHardware(const HardwareStartConfig & config, ActionCallback callback) = 0;
  virtual void prepareAmcl(const HardwareStartConfig &, ActionCallback callback) { callback({false, "AMCL preparation is not supported by this adapter."}); }
  virtual void syncSource(const HardwareStartConfig & config, ActionCallback callback) = 0;
  virtual void buildWorkspace(const HardwareStartConfig & config, ActionCallback callback) = 0;
};

class ProcessManager;
class LogStore;

class SshHardwareStartAdapter final : public QObject, public HardwareStartAdapter
{
public:
  explicit SshHardwareStartAdapter(ProcessManager * processes, LogStore * logs = nullptr,
    const QString & ssh_program = "ssh", const QString & rsync_program = "rsync");
  void runPreflight(const HardwareStartConfig & config, PreflightCallback callback) override;
  void startHardware(const HardwareStartConfig & config, ActionCallback callback) override;
  void prepareAmcl(const HardwareStartConfig & config, ActionCallback callback) override;
  void syncSource(const HardwareStartConfig & config, ActionCallback callback) override;
  void buildWorkspace(const HardwareStartConfig & config, ActionCallback callback) override;
private:
  void runRemoteAction(const QString & name, const HardwareStartConfig & config,
    const QString & shell_command, ActionCallback callback);
  ProcessManager * processes_{};
  LogStore * logs_{};
  QString ssh_program_;
  QString rsync_program_;
};

class HardwareStartController : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariantList checks READ checks NOTIFY changed)
  Q_PROPERTY(QString state READ state NOTIFY changed)
  Q_PROPERTY(QString message READ message NOTIFY changed)
  Q_PROPERTY(QVariantMap options READ options NOTIFY changed)
  Q_PROPERTY(QString localizationState READ localizationState NOTIFY changed)
  Q_PROPERTY(QString localizationDetail READ localizationDetail NOTIFY changed)
  Q_PROPERTY(QString activeLocalization READ activeLocalization NOTIFY changed)
  Q_PROPERTY(bool preflightReady READ preflightReady NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)

public:
  explicit HardwareStartController(std::shared_ptr<HardwareStartAdapter> adapter,
    QObject * parent = nullptr, const QString & settings_path = {});
  QVariantList checks() const { return checks_; }
  QString state() const { return state_; }
  QString message() const { return message_; }
  QVariantMap options() const { return options_; }
  QString localizationState() const;
  QString localizationDetail() const;
  QString activeLocalization() const { return active_localization_; }
  void setNode(rclcpp::Node::SharedPtr node);
  bool preflightReady() const { return preflight_ready_; }
  bool busy() const { return busy_; }

  Q_INVOKABLE void setContext(const QString & preset, const QString & mode,
    const QString & host, int ros_domain, const QString & robot,
    const QString & connection_state, const QString & local_workspace,
    const QString & remote_workspace);
  Q_INVOKABLE void setOption(const QString & name, const QVariant & value);
  Q_INVOKABLE void setLocalizationStatus(const QString & source, const QString & state,
    const QString & detail);
  Q_INVOKABLE void runPreflight();
  Q_INVOKABLE void startHardware();
  Q_INVOKABLE void prepareAmcl();
  Q_INVOKABLE void syncSource();
  Q_INVOKABLE void buildWorkspace();

signals:
  void changed();

private:
  void resetLocalizationSubscriptions();
  void refreshLocalization();
  HardwareStartConfig config() const;
  QString contextFingerprint() const;
  void invalidatePreflight(const QString & reason);
  void finishAction(const HardwareActionResult & result, const QString & verb);
  void saveOptions();
  QVariantMap defaultOptions() const;

  std::shared_ptr<HardwareStartAdapter> adapter_;
  std::unique_ptr<QSettings> settings_;
  QString preset_;
  QString mode_;
  QString host_;
  int ros_domain_{62};
  QString robot_;
  QString connection_state_;
  QString local_workspace_;
  QString remote_workspace_;
  QVariantMap options_;
  QMap<QString, QVariantMap> localization_statuses_;
  rclcpp::Node::SharedPtr node_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr localization_source_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr localization_pose_subscription_;
  QTimer localization_timer_;
  QString reported_source_;
  QString active_localization_{"Unknown · no confirmed localization source"};
  std::chrono::steady_clock::time_point pose_received_{};
  bool have_localization_pose_{false};
  bool amcl_adapter_available_{false};
  QVariantList checks_;
  QString state_{"Idle"};
  QString message_{"Connect to the selected hardware target, then run preflight."};
  QString checked_context_;
  bool preflight_ready_{false};
  bool busy_{false};
};
