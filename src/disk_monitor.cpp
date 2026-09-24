#include "disk_monitor.h"
#include "logger.h"
#include <sys/statvfs.h>
#include <algorithm>
#include <unistd.h>

namespace HealthMonitor {

DiskMonitor::DiskMonitor(const DiskThresholds& thresholds)
    : m_thresholds(thresholds) {}

void DiskMonitor::updateThresholds(const DiskThresholds& thresholds) {
    m_thresholds = thresholds;
}

DiskMetrics DiskMonitor::sample() {
    DiskMetrics metrics;
    metrics.mountPoint = m_thresholds.monitoredMountPoint;

    struct statvfs stat{};
    if (statvfs(metrics.mountPoint.c_str(), &stat) != 0) {
        Logger::getInstance().warn("DISK", "statvfs failed for mount point " + metrics.mountPoint);
        return metrics;
    }

    metrics.totalBytes = static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
    metrics.freeBytes = static_cast<uint64_t>(stat.f_bfree) * stat.f_frsize;
    metrics.availableBytes = static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;

    if (metrics.totalBytes > 0) {
        metrics.usedBytes = metrics.totalBytes - metrics.freeBytes;
        // Non-root available calculation
        metrics.usedPercent = (static_cast<double>(metrics.totalBytes - metrics.availableBytes) /
                               static_cast<double>(metrics.totalBytes)) * 100.0;
        metrics.usedPercent = std::clamp(metrics.usedPercent, 0.0, 100.0);
    }

    metrics.totalInodes = stat.f_files;
    metrics.freeInodes = stat.f_ffree;
    if (metrics.totalInodes > 0) {
        metrics.inodesUsedPercent = (static_cast<double>(metrics.totalInodes - metrics.freeInodes) /
                                     static_cast<double>(metrics.totalInodes)) * 100.0;
    }

    metrics.readOnlyFilesystem = (stat.f_flag & ST_RDONLY) != 0;

    // Threshold evaluation
    if (metrics.usedPercent >= m_thresholds.criticalPercent || metrics.readOnlyFilesystem) {
        metrics.status = HealthStatus::CRITICAL;
        Logger::getInstance().alert("DISK", "Critical disk utilization on " + metrics.mountPoint +
            ": " + std::to_string(metrics.usedPercent) + "% used (Avail: " +
            std::to_string(metrics.availableBytes / (1024 * 1024)) + " MB)");

        if (m_thresholds.autoRecoveryEnabled && !metrics.readOnlyFilesystem) {
            executeRecoveryAction(m_thresholds.action);
        }
    } else if (metrics.usedPercent >= m_thresholds.warningPercent) {
        metrics.status = HealthStatus::WARNING;
    } else {
        metrics.status = HealthStatus::NOMINAL;
    }

    return metrics;
}

bool DiskMonitor::executeRecoveryAction(const std::string& action) {
    Logger::getInstance().recovery("DISK", "Executing Disk Auto-Recovery: [" + action + "]");

    if (action == "cleanup_temp_and_journal" || action == "prune_caches") {
        // Step 1: Remove stale ephemeral files in /tmp older than 24 hours
        int retTmp = system("find /tmp -type f -atime +1 -delete 2>/dev/null");

        // Step 2: Vacuum systemd journal to prevent unbounded flash memory exhaustion
        int retJournal = system("journalctl --vacuum-size=50M 2>/dev/null");

        // Step 3: Remove rotated old logs if present (.1, .gz)
        int retLog = system("find /var/log -type f -name \"*.gz\" -delete 2>/dev/null");

        (void)retTmp; (void)retJournal; (void)retLog;
        Logger::getInstance().recovery("DISK", "Disk reclamation routines complete (/tmp scrubbed, journal pruned to 50M).");
        return true;
    }

    return false;
}

} // namespace HealthMonitor
