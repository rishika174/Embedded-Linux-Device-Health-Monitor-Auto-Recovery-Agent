#pragma once

#include <string>
#include <vector>
#include "config_manager.h"
#include "cpu_monitor.h"

namespace HealthMonitor {

struct TemperatureMetrics {
    double currentCelsius = 0.0;
    double warningThreshold = 70.0;
    double criticalThreshold = 80.0;
    std::string thermalZoneName = "cpu-thermal";
    std::string coolingDeviceState;
    HealthStatus status = HealthStatus::NOMINAL;
    bool isThrottling = false;
};

class TemperatureMonitor {
public:
    explicit TemperatureMonitor(const TemperatureThresholds& thresholds);
    ~TemperatureMonitor() = default;

    void updateThresholds(const TemperatureThresholds& thresholds);
    TemperatureMetrics sample();
    bool executeRecoveryAction(const std::string& action);

private:
    double readThermalZone(const std::string& path);
    std::string readSysfsString(const std::string& path);

    TemperatureThresholds m_thresholds;
};

} // namespace HealthMonitor
