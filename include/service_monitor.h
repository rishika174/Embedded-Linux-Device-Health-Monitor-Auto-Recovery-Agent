#pragma once

#include <string>
#include <chrono>
#include "config_manager.h"
#include "cpu_monitor.h"

namespace HealthMonitor {

enum class ServiceState {
    ACTIVE_RUNNING,
    INACTIVE_DEAD,
    ACTIVATING,
    DEACTIVATING,
    FAILED,
    NOT_FOUND
};

struct ServiceMetrics {
    std::string serviceName;
    ServiceState state = ServiceState::NOT_FOUND;
    std::string stateString = "unknown";
    int processPid = -1;
    uint64_t uptimeSeconds = 0;
    int restartCount = 0;
    HealthStatus status = HealthStatus::NOMINAL;
    std::string lastFailureReason;
};

class ServiceMonitor {
public:
    explicit ServiceMonitor(const CriticalServiceThresholds& thresholds);
    ~ServiceMonitor() = default;

    void updateThresholds(const CriticalServiceThresholds& thresholds);
    ServiceMetrics sample();
    bool executeRecoveryAction(const std::string& action);

private:
    ServiceState querySystemdStatus(const std::string& serviceName, int& pid, std::string& rawStatus);
    bool restartSystemdService(const std::string& serviceName);

    CriticalServiceThresholds m_thresholds;
    int m_restartAttemptsInWindow = 0;
    std::chrono::steady_clock::time_point m_lastRestartTime;
    int m_totalRestarts = 0;
};

} // namespace HealthMonitor
