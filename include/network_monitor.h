#pragma once

#include <string>
#include <cstdint>
#include "config_manager.h"
#include "cpu_monitor.h"

namespace HealthMonitor {

struct NetworkMetrics {
    std::string interfaceName = "eth0";
    bool carrierDetected = false;
    std::string operstate = "unknown";
    bool gatewayReachable = false;
    double pingLatencyMs = 0.0;
    uint64_t rxBytes = 0;
    uint64_t txBytes = 0;
    uint64_t rxErrors = 0;
    uint64_t txErrors = 0;
    int consecutiveFailures = 0;
    HealthStatus status = HealthStatus::NOMINAL;
};

class NetworkMonitor {
public:
    explicit NetworkMonitor(const NetworkThresholds& thresholds);
    ~NetworkMonitor() = default;

    void updateThresholds(const NetworkThresholds& thresholds);
    NetworkMetrics sample();
    bool executeRecoveryAction(const std::string& action);

private:
    bool checkCarrier(const std::string& iface);
    std::string readOperState(const std::string& iface);
    bool checkPingSocket(const std::string& ip, int timeoutSeconds, double& latencyMs);
    void readDevStats(const std::string& iface, NetworkMetrics& metrics);

    NetworkThresholds m_thresholds;
    int m_consecutiveFailures = 0;
};

} // namespace HealthMonitor
