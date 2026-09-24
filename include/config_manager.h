#pragma once

#include <string>
#include <map>
#include "logger.h"

namespace HealthMonitor {

struct CpuThresholds {
    double warningPercent = 75.0;
    double criticalPercent = 85.0;
    int consecutiveBreachesToTrigger = 3;
    bool autoRecoveryEnabled = true;
    std::string action = "renice_low_priority";
};

struct MemoryThresholds {
    double warningPercent = 80.0;
    double criticalPercent = 90.0;
    uint64_t minAvailableMb = 64;
    bool autoRecoveryEnabled = true;
    std::string action = "drop_caches";
};

struct DiskThresholds {
    std::string monitoredMountPoint = "/";
    double warningPercent = 80.0;
    double criticalPercent = 90.0;
    bool autoRecoveryEnabled = true;
    std::string action = "cleanup_temp_and_journal";
};

struct TemperatureThresholds {
    std::string thermalZonePath = "/sys/class/thermal/thermal_zone0/temp";
    double warningCelsius = 70.0;
    double criticalCelsius = 80.0;
    bool autoRecoveryEnabled = true;
    std::string action = "throttle_governor";
};

struct NetworkThresholds {
    std::string interfaceName = "eth0";
    std::string pingTargetIp = "8.8.8.8";
    int pingTimeoutSeconds = 2;
    int maxConsecutiveFailures = 3;
    bool autoRecoveryEnabled = true;
    std::string action = "restart_interface";
};

struct CriticalServiceThresholds {
    std::string serviceName = "iot-gateway";
    std::string checkMethod = "systemd";
    int maxRestartAttempts = 5;
    int cooldownSeconds = 30;
    bool autoRecoveryEnabled = true;
    std::string action = "restart_service";
};

struct MonitorConfig {
    std::string deviceId = "embedded-linux-node";
    uint32_t pollingIntervalMs = 2000;
    std::string logFilePath = "/var/log/device-health-monitor.log";
    std::string logLevel = "INFO";
    bool enableSyslog = true;
    bool enableStdout = true;

    CpuThresholds cpu;
    MemoryThresholds memory;
    DiskThresholds disk;
    TemperatureThresholds temperature;
    NetworkThresholds network;
    CriticalServiceThresholds service;
};

class ConfigManager {
public:
    static ConfigManager& getInstance();

    bool loadFromFile(const std::string& configFilePath);
    bool saveToFile(const std::string& configFilePath);
    const MonitorConfig& getConfig() const;
    void setConfig(const MonitorConfig& newConfig);

    static bool parseJsonString(const std::string& jsonContent, MonitorConfig& outConfig);

private:
    ConfigManager();
    ~ConfigManager() = default;

    MonitorConfig m_config;
    std::string m_configPath;
    std::mutex m_mutex;
};

} // namespace HealthMonitor
