#include "ur_shutdown.hpp"
#include <QMap>

class FakeShutdown final : public UrShutdownAdapter {
public:
  void start(const QString & host, const QString & robot, const QString & workspace,
    const QString & side, Callback done) override {
    ++starts; last = host + "/" + robot + "/" + side; callbacks.insert(side, done); Q_UNUSED(workspace);
  }
  int starts{0}; QString last; QMap<QString, Callback> callbacks;
};

bool run_ur_shutdown_contract_test()
{
  auto adapter = std::make_shared<FakeShutdown>();
  UrShutdown controller(adapter);
  int stops = 0; bool busy = true;
  controller.setMotionHooks([&](const QString &, const QString &) { ++stops; },
    [&](const QString &, const QString &) { return busy; });
  controller.setContext("Hardware", "host-a", "mur620d", "Connected", "$HOME/ws", true, true);
  if (controller.confirm() || adapter->starts) return false;
  controller.setSelected("r", true);
  if (!controller.prepare() || !controller.targets().contains("host-a/mur620d/UR10_r")) return false;
  controller.setSelected("l", true);
  if (controller.confirm() || adapter->starts) return false;
  controller.setSelected("l", false);
  if (!controller.prepare() || !controller.confirm() || stops != 1 || adapter->starts) return false;
  if (!controller.inhibited("host-a", "mur620d", "r") || controller.inhibited("host-a", "mur620d", "l")) return false;
  busy = false; controller.refresh();
  if (adapter->starts != 1 || adapter->last != "host-a/mur620d/r") return false;
  adapter->callbacks.value("r")(true, "accepted");
  if (!controller.results().contains("UR10_r") || !controller.results().contains("accepted")) return false;
  if (!controller.inhibited("host-a", "mur620d", "r")) return false;
  controller.setContext("Hardware", "host-b", "mur620d", "Connected", "$HOME/ws", true, true);
  controller.setSelected("l", true); controller.setSelected("r", true);
  if (!controller.prepare() || !controller.confirm() || adapter->starts != 3) return false;
  adapter->callbacks.value("l")(false, "dashboard unavailable");
  if (!controller.busy() || !controller.results().contains("dashboard unavailable")) return false;
  adapter->callbacks.value("r")(true, "accepted");
  if (controller.busy() || !controller.results().contains("host-b/mur620d/UR10_l: Failed") ||
      !controller.results().contains("host-b/mur620d/UR10_r: Accepted") ||
      !controller.inhibited("host-b", "mur620d", "l")) return false;
  return true;
}
