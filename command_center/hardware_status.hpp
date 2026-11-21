#pragma once

#include <QStringList>
#include <QMap>

#include <chrono>
#include <functional>
#include <vector>

class HardwareStatus
{
public:
  using Clock = std::function<std::chrono::steady_clock::time_point()>;

  explicit HardwareStatus(Clock clock = [] { return std::chrono::steady_clock::now(); });
  void expectBattery(const QString & label, const QString & topic);
  void updateBattery(const QString & label, const QString & topic, double percent,
    double voltage, double current, double remaining_capacity, double capacity);
  void updateSoc(const QString & topic, double percent);
  void updateRemainingEstimate(const QString & topic, int seconds);
  void updateLift(const std::vector<std::string> & names, const std::vector<double> & positions);
  QStringList diagnostics(const QString & robot) const;

private:
  struct Battery
  {
    QString topic;
    double percent{0.0};
    double voltage{0.0};
    double current{0.0};
    double remaining_capacity{0.0};
    double capacity{0.0};
    bool percent_valid{false};
    bool voltage_valid{false};
    bool current_valid{false};
    bool remaining_capacity_valid{false};
    bool capacity_valid{false};
    std::chrono::steady_clock::time_point received{};
    int remaining_seconds{-1};
    std::chrono::steady_clock::time_point remaining_received{};
  };
  QString batteryLine(const QString & label, const Battery & battery) const;
  Clock clock_;
  QMap<QString, Battery> batteries_;
  QMap<QString, double> lift_positions_;
  std::chrono::steady_clock::time_point lift_received_{};
};
