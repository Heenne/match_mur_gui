#pragma once
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <functional>
#include <memory>

class UrShutdownAdapter {
public:
  using Callback = std::function<void(bool, QString)>;
  virtual ~UrShutdownAdapter() = default;
  virtual void start(const QString & host, const QString & robot, const QString & workspace,
    const QString & side, Callback done) = 0;
};

class SshUrShutdownAdapter final : public QObject, public UrShutdownAdapter {
public:
  using QObject::QObject;
  void start(const QString & host, const QString & robot, const QString & workspace,
    const QString & side, Callback done) override;
};

class UrShutdown final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString targets READ targets NOTIFY changed)
  Q_PROPERTY(QString results READ results NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool leftSelected READ leftSelected NOTIFY changed)
  Q_PROPERTY(bool rightSelected READ rightSelected NOTIFY changed)
public:
  explicit UrShutdown(std::shared_ptr<UrShutdownAdapter> adapter, QObject * parent = nullptr);
  void setMotionHooks(std::function<void(const QString &, const QString &)> stop,
    std::function<bool(const QString &, const QString &)> busy);
  void setStartBusyHook(std::function<bool()> busy) { start_busy_ = std::move(busy); }
  void setContext(const QString & mode, const QString & host, const QString & robot,
    const QString & connection, const QString & workspace, bool left, bool right);
  Q_INVOKABLE void setSelected(const QString & side, bool selected);
  Q_INVOKABLE bool prepare();
  Q_INVOKABLE bool confirm();
  Q_INVOKABLE void refresh();
  QString targets() const;
  QString results() const { return results_; }
  bool busy() const { return busy_; }
  bool leftSelected() const { return selected_.contains("l"); }
  bool rightSelected() const { return selected_.contains("r"); }
  bool inhibited(const QString & host, const QString & robot, const QString & side) const;
  bool anyInhibited(const QString & host, const QString & robot) const;
signals:
  void changed();
private:
  QString key(const QString & host, const QString & robot, const QString & side) const;
  std::shared_ptr<UrShutdownAdapter> adapter_;
  std::function<void(const QString &, const QString &)> stop_;
  std::function<bool(const QString &, const QString &)> motion_busy_;
  std::function<bool()> start_busy_;
  QString mode_, host_, robot_, connection_, workspace_, prepared_, results_;
  QString run_host_, run_robot_, run_workspace_;
  QStringList selected_, pending_;
  QSet<QString> inhibited_;
  bool left_{false}, right_{false}, busy_{false}, launching_{false};
  QTimer timer_;
};
bool run_ur_shutdown_contract_test();
