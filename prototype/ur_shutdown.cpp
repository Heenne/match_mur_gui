#include "ur_shutdown.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>

void SshUrShutdownAdapter::start(const QString & host, const QString &, const QString & workspace,
  const QString & side, Callback done)
{
  static const QRegularExpression valid_host("^[A-Za-z0-9][A-Za-z0-9.-]*$");
  static const QRegularExpression valid_path("^(\\$HOME|~|/[A-Za-z0-9_./-]+)(/[A-Za-z0-9_./-]*)?$");
  const auto ws = workspace.trimmed().isEmpty() ? QString("$HOME/colcon_ws") : workspace.trimmed();
  if (!valid_host.match(host).hasMatch() || !valid_path.match(ws).hasMatch() || (side != "l" && side != "r")) {
    done(false, "Invalid host, workspace or arm"); return;
  }
  QString path = ws;
  if (ws.startsWith("$HOME")) path = "\"${HOME}" + ws.mid(5) + "\"";
  else if (ws.startsWith("~")) path = "\"${HOME}" + ws.mid(1) + "\"";
  const auto arm = "UR10_" + side;
  const auto command = "timeout --signal=TERM --kill-after=3s 100s python3 -u " + path +
    "/src/match_mur_gui/scripts/shutdown_urs.py --json --host " + arm;
  auto * process = new QProcess(this);
  auto completed = std::make_shared<bool>(false);
  auto finish = [process, done, completed, arm](bool normal, int code) {
    if (*completed) return;
    *completed = true;
    const auto output = process->readAllStandardOutput();
    const auto error = QString::fromUtf8(process->readAllStandardError()).right(500);
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(output, &parse);
    bool accepted = false; QString detail;
    for (const auto & value : document.object().value("arms").toArray()) {
      const auto item = value.toObject();
      if (item.value("host").toString() == arm) {
        accepted = item.value("shutdown_accepted").toBool(false);
        detail = item.value("error").toString();
      }
    }
    if (normal && code == 0 && accepted) done(true, "UR confirmed shutdown request");
    else done(false, detail.isEmpty() ? "Shutdown unconfirmed; inspect UR. " + error : detail);
    process->deleteLater();
  };
  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), process,
    [finish](int code, QProcess::ExitStatus status) { finish(status == QProcess::NormalExit, code); });
  connect(process, &QProcess::errorOccurred, process, [finish](QProcess::ProcessError e) {
    if (e == QProcess::FailedToStart) finish(false, -1);
  });
  process->start("ssh", {"-o", "BatchMode=yes", "-o", "ConnectTimeout=5", "-o", "ServerAliveInterval=5",
    "-o", "ServerAliveCountMax=3", host, "bash", "-lc", "'" + command + "'"});
}

UrShutdown::UrShutdown(std::shared_ptr<UrShutdownAdapter> adapter, QObject * parent)
: QObject(parent), adapter_(std::move(adapter)) {
  timer_.setInterval(250); connect(&timer_, &QTimer::timeout, this, &UrShutdown::refresh); timer_.start();
}
void UrShutdown::setMotionHooks(std::function<void(const QString &, const QString &)> stop,
  std::function<bool(const QString &, const QString &)> busy) { stop_ = std::move(stop); motion_busy_ = std::move(busy); }
QString UrShutdown::key(const QString & host, const QString & robot, const QString & side) const { return host + "\n" + robot + "\n" + side; }
bool UrShutdown::inhibited(const QString & host, const QString & robot, const QString & side) const { return inhibited_.contains(key(host, robot, side)); }
bool UrShutdown::anyInhibited(const QString & host, const QString & robot) const { return inhibited(host, robot, "l") || inhibited(host, robot, "r"); }
void UrShutdown::setContext(const QString & mode, const QString & host, const QString & robot,
  const QString & connection, const QString & workspace, bool left, bool right) {
  if (mode_ == mode && host_ == host && robot_ == robot && connection_ == connection && workspace_ == workspace && left_ == left && right_ == right) return;
  mode_ = mode; host_ = host; robot_ = robot; connection_ = connection; workspace_ = workspace; left_ = left; right_ = right;
  selected_.clear(); prepared_.clear(); emit changed();
}
void UrShutdown::setSelected(const QString & side, bool selected) {
  if (busy_ || (side != "l" && side != "r")) return;
  if (selected && ((side == "l" && left_) || (side == "r" && right_))) { if (!selected_.contains(side)) selected_.append(side); }
  else selected_.removeAll(side);
  prepared_.clear(); emit changed();
}
QString UrShutdown::targets() const {
  QStringList list; for (const auto & side : selected_) list << host_ + "/" + robot_ + "/UR10_" + side; return list.join("\n");
}
bool UrShutdown::prepare() {
  static const QRegularExpression id("^[A-Za-z][A-Za-z0-9_]*$");
  static const QRegularExpression host("^[A-Za-z0-9][A-Za-z0-9.-]*$");
  if (busy_ || selected_.isEmpty() || mode_ != "Hardware" || connection_ != "Connected" || !host.match(host_).hasMatch() || !id.match(robot_).hasMatch() || !adapter_ || !stop_ || !motion_busy_ || (start_busy_ && start_busy_())) {
    results_ = "Shutdown blocked: select configured arms on a connected hardware target and wait for hardware startup."; emit changed(); return false;
  }
  prepared_ = targets(); emit changed(); return true;
}
bool UrShutdown::confirm() {
  if (prepared_.isEmpty() || prepared_ != targets() || busy_ || (start_busy_ && start_busy_())) { prepared_.clear(); return false; }
  prepared_.clear(); busy_ = true; run_host_ = host_; run_robot_ = robot_; run_workspace_ = workspace_; pending_ = selected_; results_.clear();
  for (const auto & side : pending_) inhibited_.insert(key(run_host_, run_robot_, side));
  for (const auto & side : pending_) stop_(run_robot_, side);
  results_ = "Stopping GUI motions; waiting for confirmation."; emit changed(); refresh(); return true;
}
void UrShutdown::refresh() {
  if (!busy_ || launching_) return;
  for (const auto & side : pending_) if (motion_busy_(run_robot_, side)) return;
  launching_ = true; results_.clear();
  const auto targets = pending_;
  for (const auto & side : targets) {
    results_ += run_host_ + "/" + run_robot_ + "/UR10_" + side + ": shutting down\n";
    QPointer<UrShutdown> guard(this);
    adapter_->start(run_host_, run_robot_, run_workspace_, side, [guard, side](bool ok, const QString & detail) {
      if (!guard) return;
      const auto target = guard->run_host_ + "/" + guard->run_robot_ + "/UR10_" + side + ": ";
      guard->results_.replace(target + "shutting down\n", target + (ok ? "Accepted: " : "Failed or unconfirmed: ") + detail + "\n");
      guard->pending_.removeAll(side);
      if (guard->pending_.isEmpty()) { guard->busy_ = false; guard->launching_ = false; }
      emit guard->changed();
    });
  }
  emit changed();
}
