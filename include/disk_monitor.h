#pragma once

#include <string>
#include <cstdint>
#include "config_manager.h"
#include "cpu_monitor.h"

namespace HealthMonitor {

struct DiskMetrics {
    std::string mountPoint = "/";
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    uint64_t availableBytes = 0;
    uint64_t usedBytes = 0;
    double usedPercent = 0.0;
    uint64_t totalInodes = 0;
    uint64_t freeInodes = 0;
    double inodesUsedPercent = 0.0;
    HealthStatus status = HealthStatus::NOMINAL;
    bool readOnlyFilesystem = false;
};

class DiskMonitor {
public:
    explicit DiskMonitor(const DiskThresholds& thresholds);
    ~DiskMonitor() = default;

    void updateThresholds(const DiskThresholds& thresholds);
    DiskMetrics sample();
    bool executeRecoveryAction(const std::string& action);

private:
    DiskThresholds m_thresholds;
};

} // namespace HealthMonitor
