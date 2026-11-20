#include "hardware_status.hpp"

#include <cmath>
#include <algorithm>
#include <utility>

HardwareStatus::HardwareStatus(Clock clock) : clock_(std::move(clock)) {}

void HardwareStatus::expectBattery(const QString & label, const QString & topic)
{
  batteries_[label].topic = topic;
}

void HardwareStatus::updateBattery(const QString & label, const QString & topic, double percent,
  double voltage, double current, double remaining_capacity, double capacity)
{
  auto & battery = batteries_[label];
  battery.topic = topic;
  battery.percent = percent;
  battery.percent_valid = std::isfinite(percent) && percent >= 0.0 && percent <= 100.0;
  battery.voltage = voltage;
  battery.voltage_valid = std::isfinite(voltage) && voltage > 0.0;
  battery.current = current;
  battery.current_valid = std::isfinite(current);
  battery.remaining_capacity = remaining_capacity;
  battery.remaining_capacity_valid = std::isfinite(remaining_capacity) && remaining_capacity >= 0.0;
  battery.capacity = capacity;
  battery.capacity_valid = std::isfinite(capacity) && capacity > 0.0;
  battery.received = clock_();
}

void HardwareStatus::updateSoc(const QString & topic, double percent)
{
  const auto & current = batteries_["MuR"];
  if (current.topic.endsWith("/bms_status/battery_state") &&
    current.received != std::chrono::steady_clock::time_point{} &&
    clock_() - current.received < std::chrono::seconds(3)) return;
  updateBattery("MuR", topic, percent, NAN, NAN, NAN, NAN);
}

void HardwareStatus::updateRemainingEstimate(const QString & topic, int seconds)
{
  if (seconds < 0) return;
  auto & battery = batteries_["MiR"];
  Q_UNUSED(topic)
  battery.remaining_seconds = seconds;
  battery.remaining_received = clock_();
}

void HardwareStatus::updateLift(const std::vector<std::string> & names, const std::vector<double> & positions)
{
  lift_positions_.clear();
  const auto count = std::min(names.size(), positions.size());
  for (size_t index = 0; index < count; ++index) {
    if (names[index] == "left_lift_joint" || names[index] == "right_lift_joint") {
      lift_positions_[QString::fromStdString(names[index])] = positions[index];
    }
  }
  lift_received_ = clock_();
}

QString HardwareStatus::batteryLine(const QString & label, const Battery & battery) const
{
  if (battery.received == std::chrono::steady_clock::time_point{}) {
    return QString("%1 battery: not received · expected %2").arg(label, battery.topic);
  }
  const double age = std::chrono::duration<double>(clock_() - battery.received).count();
  const QString freshness = age > 3.0 ? "stale" : "current";
  QString value = battery.percent_valid ? QString("%1% · ").arg(battery.percent, 0, 'f', 1) : "percentage unavailable · ";
  value += QString("%1 · %2 s · source %3").arg(freshness).arg(age, 0, 'f', 1).arg(battery.topic);
  QStringList details;
  if (battery.voltage_valid) details << QString("%1 V").arg(battery.voltage, 0, 'f', 1);
  if (battery.current_valid) details << QString("%1 A").arg(battery.current, 0, 'f', 1);
  if (battery.remaining_capacity_valid) details << QString("%1 Ah remaining").arg(battery.remaining_capacity, 0, 'f', 1);
  if (battery.capacity_valid) details << QString("%1 Ah capacity").arg(battery.capacity, 0, 'f', 1);
  if (label == "MiR") {
    if (battery.remaining_received == std::chrono::steady_clock::time_point{}) {
      details << "runtime estimate not received";
    } else {
      const double estimate_age = std::chrono::duration<double>(clock_() - battery.remaining_received).count();
      const auto estimate_freshness = estimate_age > 3.0 ? "stale estimate" : "reported estimate";
      details << QString("runtime %1 h · %2 · %3 s")
        .arg(static_cast<double>(battery.remaining_seconds) / 3600.0, 0, 'f', 1)
        .arg(estimate_freshness).arg(estimate_age, 0, 'f', 1);
    }
  }
  if (!details.isEmpty()) value += " · " + details.join(", ");
  return QString("%1 battery: %2").arg(label, value);
}

QStringList HardwareStatus::diagnostics(const QString & robot) const
{
  QStringList lines;
  Battery mur;
  mur.topic = QString("/%1/bms_status/battery_state or /%1/bms_status/SOC").arg(robot);
  Battery mir;
  mir.topic = QString("/%1/battery_state").arg(robot);
  lines << batteryLine("MuR", batteries_.value("MuR", mur));
  lines << batteryLine("MiR", batteries_.value("MiR", mir));

  if (lift_received_ == std::chrono::steady_clock::time_point{}) {
    lines << QString("Hardware component lift: not received · expected /%1/joint_states (left_lift_joint, right_lift_joint)")
      .arg(robot);
  } else {
    const double age = std::chrono::duration<double>(clock_() - lift_received_).count();
    const auto freshness = age > 3.0 ? "stale" : "current";
    QStringList positions;
    for (const auto & joint : {QString("left_lift_joint"), QString("right_lift_joint")}) {
      if (lift_positions_.contains(joint)) positions << QString("%1: %2 m")
        .arg(joint).arg(lift_positions_[joint], 0, 'f', 3);
      else positions << QString("%1: unavailable").arg(joint);
    }
    lines << QString("Hardware component lift: %1 · %2 s · %3")
      .arg(freshness).arg(age, 0, 'f', 1).arg(positions.join(" · "));
  }
  return lines;
}
