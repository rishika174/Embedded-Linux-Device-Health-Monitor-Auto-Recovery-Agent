#pragma once

#include <string>
#include <cstdint>
#include "config_manager.h"
#include "cpu_monitor.h"

namespace HealthMonitor {

struct MemoryMetrics {
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    uint64_t availableBytes = 0;
    uint64_t buffersBytes = 0;
    uint64_t cachedBytes = 0;
    uint64_t swapTotalBytes = 0;
    uint64_t swapFreeBytes = 0;

    double usedPercent = 0.0;
    double swapUsedPercent = 0.0;
    HealthStatus status = HealthStatus::NOMINAL;
    bool oomRisk = false;
};

class MemoryMonitor {
public:
    explicit MemoryMonitor(const MemoryThresholds& thresholds);
    ~MemoryMonitor() = default;

    void updateThresholds(const MemoryThresholds& thresholds);
    MemoryMetrics sample();
    bool executeRecoveryAction(const std::string& action);

private:
    bool readProcMeminfo(MemoryMetrics& metrics);

    MemoryThresholds m_thresholds;
    std::string m_meminfoPath = "/proc/meminfo";
};

} // namespace HealthMonitor
