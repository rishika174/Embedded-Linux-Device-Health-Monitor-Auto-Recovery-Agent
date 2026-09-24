#include "memory_monitor.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <algorithm>
#include <fcntl.h>

namespace HealthMonitor {

MemoryMonitor::MemoryMonitor(const MemoryThresholds& thresholds)
    : m_thresholds(thresholds) {}

void MemoryMonitor::updateThresholds(const MemoryThresholds& thresholds) {
    m_thresholds = thresholds;
}

bool MemoryMonitor::readProcMeminfo(MemoryMetrics& metrics) {
    std::ifstream file(m_meminfoPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    uint64_t memTotalKb = 0;
    uint64_t memFreeKb = 0;
    uint64_t memAvailableKb = 0;
    uint64_t buffersKb = 0;
    uint64_t cachedKb = 0;
    uint64_t swapTotalKb = 0;
    uint64_t swapFreeKb = 0;

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string key;
        uint64_t val;
        std::string unit;

        ss >> key >> val >> unit;

        if (key == "MemTotal:") memTotalKb = val;
        else if (key == "MemFree:") memFreeKb = val;
        else if (key == "MemAvailable:") memAvailableKb = val;
        else if (key == "Buffers:") buffersKb = val;
        else if (key == "Cached:") cachedKb = val;
        else if (key == "SwapTotal:") swapTotalKb = val;
        else if (key == "SwapFree:") swapFreeKb = val;
    }

    // Fallback if MemAvailable isn't supported on older Linux kernels (<3.14)
    if (memAvailableKb == 0 && memTotalKb > 0) {
        memAvailableKb = memFreeKb + buffersKb + cachedKb;
    }

    metrics.totalBytes = memTotalKb * 1024ULL;
    metrics.freeBytes = memFreeKb * 1024ULL;
    metrics.availableBytes = memAvailableKb * 1024ULL;
    metrics.buffersBytes = buffersKb * 1024ULL;
    metrics.cachedBytes = cachedKb * 1024ULL;
    metrics.swapTotalBytes = swapTotalKb * 1024ULL;
    metrics.swapFreeBytes = swapFreeKb * 1024ULL;

    if (metrics.totalBytes > 0) {
        uint64_t usedBytes = (metrics.totalBytes >= metrics.availableBytes)
                             ? (metrics.totalBytes - metrics.availableBytes)
                             : 0;
        metrics.usedPercent = (static_cast<double>(usedBytes) / static_cast<double>(metrics.totalBytes)) * 100.0;
        metrics.usedPercent = std::clamp(metrics.usedPercent, 0.0, 100.0);
    }

    if (metrics.swapTotalBytes > 0) {
        uint64_t swapUsed = metrics.swapTotalBytes - metrics.swapFreeBytes;
        metrics.swapUsedPercent = (static_cast<double>(swapUsed) / static_cast<double>(metrics.swapTotalBytes)) * 100.0;
    }

    return true;
}

MemoryMetrics MemoryMonitor::sample() {
    MemoryMetrics metrics;
    if (!readProcMeminfo(metrics)) {
        Logger::getInstance().warn("MEM", "Failed to read /proc/meminfo");
        return metrics;
    }

    uint64_t availMb = metrics.availableBytes / (1024ULL * 1024ULL);

    if (metrics.usedPercent >= m_thresholds.criticalPercent || availMb < m_thresholds.minAvailableMb) {
        metrics.status = HealthStatus::CRITICAL;
        metrics.oomRisk = true;

        Logger::getInstance().alert("MEM", "RAM critical threshold breached: " +
            std::to_string(metrics.usedPercent) + "% used, available: " +
            std::to_string(availMb) + " MB (min threshold: " +
            std::to_string(m_thresholds.minAvailableMb) + " MB)");

        if (m_thresholds.autoRecoveryEnabled) {
            executeRecoveryAction(m_thresholds.action);
        }
    } else if (metrics.usedPercent >= m_thresholds.warningPercent) {
        metrics.status = HealthStatus::WARNING;
    } else {
        metrics.status = HealthStatus::NOMINAL;
    }

    return metrics;
}

bool MemoryMonitor::executeRecoveryAction(const std::string& action) {
    Logger::getInstance().recovery("MEM", "Executing Memory Auto-Recovery: [" + action + "]");

    // Step 1: Force POSIX filesystem sync to guarantee clean page writeback
    sync();

    if (action == "drop_caches" || action == "emergency_flush") {
        // Direct kernel write to /proc/sys/vm/drop_caches (3 drops pagecache, dentries and inodes)
        int fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
        if (fd >= 0) {
            const char* val = "3\n";
            ssize_t written = write(fd, val, 2);
            close(fd);
            if (written > 0) {
                Logger::getInstance().recovery("MEM", "Successfully dropped Linux kernel pagecaches & dentries via sysctl");
                return true;
            }
        }

        // Fallback via shell call if permission permits
        int ret = system("echo 3 > /proc/sys/vm/drop_caches 2>/dev/null");
        if (ret == 0) {
            Logger::getInstance().recovery("MEM", "Drop caches executed via shell fallback");
            return true;
        }

        Logger::getInstance().warn("MEM", "Could not write to /proc/sys/vm/drop_caches (requires root/CAP_SYS_ADMIN)");
        return false;
    }

    return true;
}

} // namespace HealthMonitor
