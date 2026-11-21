#pragma once

#include "ur_arm_jog.hpp"

#include <QObject>
#include <QTimer>
#include <QVariantMap>

#include <memory>

class ProcessManager;

struct UrMotionTask
{
  QString name;
  QString kind;
  QString host;
  int ros_domain{62};
  QString robot;
  QString side;
  QString remote_workspace;
  QString named_pose;
  QString plane;
  QString direction;
  QString reference_frame;
  double velocity_scaling{0.2};
  double angular_velocity{0.10};
};

struct UrMotionTaskStatus
{
  QString state{"Stopped"};
  QString output;
  int exit_code{-1};
};

class UrMotionActionsAdapter
{
public:
  virtual ~UrMotionActionsAdapter() = default;
  virtual bool connected() const = 0;
  virtual UrArmReadiness inspect(const std::string & robot, const std::string & side) = 0;
  virtual bool start(const UrMotionTask & task, QString * detail) = 0;
  virtual void stop(const QString & name) = 0;
  virtual UrMotionTaskStatus status(const QString & name) const = 0;
};

class SshUrMotionActionsAdapter final : public UrMotionActionsAdapter
{
public:
  SshUrMotionActionsAdapter(std::shared_ptr<UrArmJogAdapter> readiness,
    ProcessManager * processes);
  bool connected() const override;
  UrArmReadiness inspect(const std::string & robot, const std::string & side) override;
  bool start(const UrMotionTask & task, QString * detail) override;
  void stop(const QString & name) override;
  UrMotionTaskStatus status(const QString & name) const override;

private:
  std::shared_ptr<UrArmJogAdapter> readiness_;
  ProcessManager * processes_{};
};

class UrMotionActions final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString selectedArm READ selectedArm NOTIFY changed)
  Q_PROPERTY(QString readiness READ readiness NOTIFY changed)
  Q_PROPERTY(bool ready READ ready NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString progress READ progress NOTIFY changed)
  Q_PROPERTY(QString result READ result NOTIFY changed)
  Q_PROPERTY(QString homePose READ homePose NOTIFY changed)
  Q_PROPERTY(double velocityScaling READ velocityScaling NOTIFY changed)
  Q_PROPERTY(double angularVelocity READ angularVelocity NOTIFY changed)
  Q_PROPERTY(QString referenceFrame READ referenceFrame NOTIFY changed)
  Q_PROPERTY(QString alignmentPreview READ alignmentPreview NOTIFY changed)
  Q_PROPERTY(QString alignmentPlane READ alignmentPlane NOTIFY changed)
  Q_PROPERTY(QString alignmentDirection READ alignmentDirection NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)

public:
  explicit UrMotionActions(std::shared_ptr<UrMotionActionsAdapter> adapter,
    QObject * parent = nullptr);
  void setJog(UrArmJog * jog) { jog_ = jog; }
  void setMotionBlocked(bool blocked) { motion_blocked_ = blocked; }
  QString selectedArm() const;
  QString readiness() const { return readiness_; }
  bool ready() const { return ready_; }
  QString status() const { return status_; }
  QString progress() const { return progress_; }
  QString result() const { return result_; }
  QString homePose() const { return home_pose_; }
  double velocityScaling() const { return velocity_scaling_; }
  double angularVelocity() const { return angular_velocity_; }
  QString referenceFrame() const { return reference_frame_; }
  QString alignmentPreview() const { return alignment_preview_; }
  QString alignmentPlane() const { return alignment_plane_; }
  QString alignmentDirection() const { return alignment_direction_; }
  bool busy() const { return busy_; }

  Q_INVOKABLE void setContext(const QString & preset, const QString & mode,
    const QString & host, int ros_domain, const QString & robot,
    const QString & connection_state, const QString & session_key,
    const QString & remote_workspace, bool arm_l_enabled, bool arm_r_enabled);
  Q_INVOKABLE void selectArm(const QString & side);
  Q_INVOKABLE void setHomePose(const QString & pose);
  Q_INVOKABLE void setVelocityScaling(double percent);
  Q_INVOKABLE void setAngularVelocity(double value);
  Q_INVOKABLE void setAlignmentPlane(const QString & plane);
  Q_INVOKABLE void setAlignmentDirection(const QString & direction);
  Q_INVOKABLE void refresh();
  Q_INVOKABLE bool startHome();
  Q_INVOKABLE bool previewAlignment();
  Q_INVOKABLE bool startAlignment();
  Q_INVOKABLE void cancel();

signals:
  void changed();

private:
  UrMotionTask task(const QString & kind) const;
  bool canStart(QString * reason);
  void start(const QString & kind);
  void updateTaskStatus();
  void updateReferenceFrame();
  void updatePreview();
  void setStatus(const QString & value);
  static bool validIdentifier(const QString & value);

  std::shared_ptr<UrMotionActionsAdapter> adapter_;
  UrArmJog * jog_{nullptr};
  QTimer timer_;
  QString preset_;
  QString mode_;
  QString host_;
  int ros_domain_{62};
  QString robot_;
  QString connection_state_;
  QString session_key_;
  QString remote_workspace_;
  QString side_{"r"};
  QString home_pose_{"Home_custom"};
  QString alignment_plane_{"XY"};
  QString alignment_direction_{"+Z"};
  QString reference_frame_;
  QString alignment_preview_;
  QString process_name_;
  QString readiness_{"Connect to a hardware session and select a configured arm."};
  QString status_{"Idle"};
  QString progress_{"No motion active"};
  QString result_{"No result yet"};
  double velocity_scaling_{0.2};
  double angular_velocity_{0.10};
  bool arm_l_enabled_{false};
  bool arm_r_enabled_{false};
  bool ready_{false};
  bool busy_{false};
  bool motion_blocked_{false};
};

bool run_ur_motion_actions_contract_test();
