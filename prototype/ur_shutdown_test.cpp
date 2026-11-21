#include "ur_shutdown.hpp"

class FakeShutdown final : public UrShutdownAdapter {
public:
  void start(const QString & host, const QString & robot, const QString & workspace,
    const QString & side, Callback done) override {
    ++starts; last = host + "/" + robot + "/" + side; callback = done; Q_UNUSED(workspace);
  }
  int starts{0}; QString last; Callback callback;
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
  adapter->callback(true, "accepted");
  if (!controller.results().contains("UR10_r") || !controller.results().contains("accepted")) return false;
  if (!controller.inhibited("host-a", "mur620d", "r")) return false;
  return true;
}
