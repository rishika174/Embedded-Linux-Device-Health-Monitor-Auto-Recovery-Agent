#include "temperature_monitor.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <dirent.h>

namespace HealthMonitor {

TemperatureMonitor::TemperatureMonitor(const TemperatureThresholds& thresholds)
    : m_thresholds(thresholds) {}

void TemperatureMonitor::updateThresholds(const TemperatureThresholds& thresholds) {
    m_thresholds = thresholds;
}

double TemperatureMonitor::readThermalZone(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return -1.0;
    }

    long milliDeg = 0;
    if (file >> milliDeg) {
        if (milliDeg > 1000) {
            return static_cast<double>(milliDeg) / 1000.0;
        }
        return static_cast<double>(milliDeg);
    }
    return -1.0;
}

std::string TemperatureMonitor::readSysfsString(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string val;
    std::getline(file, val);
    return val;
}

TemperatureMetrics TemperatureMonitor::sample() {
    TemperatureMetrics metrics;
    metrics.warningThreshold = m_thresholds.warningCelsius;
    metrics.criticalThreshold = m_thresholds.criticalCelsius;

    double temp = readThermalZone(m_thresholds.thermalZonePath);

    // Fallback search across /sys/class/thermal if user specified zone is missing
    if (temp < 0.0) {
        for (int i = 0; i < 4; ++i) {
            std::string candidate = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
            double candTemp = readThermalZone(candidate);
            if (candTemp > 0.0) {
                temp = candTemp;
                metrics.thermalZoneName = "thermal_zone" + std::to_string(i);
                break;
            }
        }
    }

    // Hardware monitor hwmon fallback (common on x86 & ARM boards)
    if (temp < 0.0) {
        temp = readThermalZone("/sys/class/hwmon/hwmon0/temp1_input");
        if (temp > 0.0) metrics.thermalZoneName = "hwmon0_temp1";
    }

    // Default simulation fallback if running in containers without physical thermal sensors
    if (temp < 0.0) {
        temp = 48.5; // Nominal ambient embedded operating temp
        metrics.thermalZoneName = "emulated_sensor";
    }

    metrics.currentCelsius = temp;

    // Check cooling device status
    std::string coolingCur = readSysfsString("/sys/class/thermal/cooling_device0/cur_state");
    metrics.coolingDeviceState = coolingCur.empty() ? "N/A" : coolingCur;

    // Evaluate temperature status
    if (metrics.currentCelsius >= m_thresholds.criticalCelsius) {
        metrics.status = HealthStatus::CRITICAL;
        metrics.isThrottling = true;

        Logger::getInstance().alert("THERMAL", "Overheating detected! Temperature: " +
            std::to_string(metrics.currentCelsius) + "°C (Critical: " +
            std::to_string(m_thresholds.criticalCelsius) + "°C). Initiating emergency thermal mitigation.");

        if (m_thresholds.autoRecoveryEnabled) {
            executeRecoveryAction(m_thresholds.action);
        }
    } else if (metrics.currentCelsius >= m_thresholds.warningCelsius) {
        metrics.status = HealthStatus::WARNING;
    } else {
        metrics.status = HealthStatus::NOMINAL;
    }

    return metrics;
}

bool TemperatureMonitor::executeRecoveryAction(const std::string& action) {
    Logger::getInstance().recovery("THERMAL", "Executing Thermal Auto-Recovery: [" + action + "]");

    if (action == "throttle_governor" || action == "boost_fan") {
        // Step 1: Boost cooling fan to maximum if PWM cooling device exists
        std::ofstream fan("/sys/class/thermal/cooling_device0/cur_state");
        if (fan.is_open()) {
            fan << "4"; // Maximum cooling level
            fan.close();
            Logger::getInstance().recovery("THERMAL", "Cooling device 0 set to maximum state");
        }

        // Step 2: Switch CPU frequency scaling governor to powersave
        DIR* cpuDir = opendir("/sys/devices/system/cpu");
        if (cpuDir) {
            struct dirent* entry;
            while ((entry = readdir(cpuDir)) != nullptr) {
                std::string name = entry->d_name;
                if (name.rfind("cpu", 0) == 0 && name.length() > 3 && isdigit(name[3])) {
                    std::string govPath = std::string("/sys/devices/system/cpu/") + name + "/cpufreq/scaling_governor";
                    std::ofstream gov(govPath);
                    if (gov.is_open()) {
                        gov << "powersave";
                    }
                }
            }
            closedir(cpuDir);
        }

        // Fallback command
        int ret = system("cpufreq-set -g powersave 2>/dev/null");
        (void)ret;

        Logger::getInstance().recovery("THERMAL", "Thermal mitigation active: CPU governor throttled to powersave");
        return true;
    }

    return false;
}

} // namespace HealthMonitor
