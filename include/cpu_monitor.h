#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include "config_manager.h"

namespace HealthMonitor {

enum class HealthStatus {
    NOMINAL,
    WARNING,
    CRITICAL
};

struct CpuSnapshot {
    uint64_t user = 0;
    uint64_t nice = 0;
    uint64_t system = 0;
    uint64_t idle = 0;
    uint64_t iowait = 0;
    uint64_t irq = 0;
    uint64_t softirq = 0;
    uint64_t steal = 0;

    uint64_t getTotal() const {
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }
    uint64_t getActive() const {
        return user + nice + system + irq + softirq + steal;
    }
};

struct CpuMetrics {
    double totalUsagePercent = 0.0;
    double userPercent = 0.0;
    double systemPercent = 0.0;
    double iowaitPercent = 0.0;
    int coreCount = 1;
    HealthStatus status = HealthStatus::NOMINAL;
    int consecutiveBreaches = 0;
    std::string topOffendingProcessName;
    int topOffendingPid = -1;
};

class CpuMonitor {
public:
    explicit CpuMonitor(const CpuThresholds& thresholds);
    ~CpuMonitor() = default;

    void updateThresholds(const CpuThresholds& thresholds);
    CpuMetrics sample();
    bool executeRecoveryAction(const std::string& action);

private:
    bool readProcStat(CpuSnapshot& snapshot);
    void detectRogueProcess(CpuMetrics& metrics);

    CpuThresholds m_thresholds;
    CpuSnapshot m_prevSnapshot;
    bool m_hasInitialSample = false;
    int m_consecutiveBreaches = 0;
};

} // namespace HealthMonitor
