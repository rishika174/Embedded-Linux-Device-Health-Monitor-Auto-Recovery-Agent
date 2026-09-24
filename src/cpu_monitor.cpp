#include "cpu_monitor.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <dirent>
#include <unistd.h>
#include <sys/resource.h>
#include <algorithm>

namespace HealthMonitor {

CpuMonitor::CpuMonitor(const CpuThresholds& thresholds)
    : m_thresholds(thresholds) {
    // Prime initial sample
    readProcStat(m_prevSnapshot);
    m_hasInitialSample = true;
}

void CpuMonitor::updateThresholds(const CpuThresholds& thresholds) {
    m_thresholds = thresholds;
}

bool CpuMonitor::readProcStat(CpuSnapshot& snapshot) {
    std::ifstream statFile("/proc/stat");
    if (!statFile.is_open()) {
        return false;
    }

    std::string line;
    if (std::getline(statFile, line)) {
        std::istringstream ss(line);
        std::string cpuLabel;
        ss >> cpuLabel;
        if (cpuLabel == "cpu") {
            ss >> snapshot.user
               >> snapshot.nice
               >> snapshot.system
               >> snapshot.idle
               >> snapshot.iowait
               >> snapshot.irq
               >> snapshot.softirq
               >> snapshot.steal;
            return true;
        }
    }
    return false;
}

void CpuMonitor::detectRogueProcess(CpuMetrics& metrics) {
    DIR* procDir = opendir("/proc");
    if (!procDir) return;

    struct dirent* entry;
    uint64_t maxJiffies = 0;
    int topPid = -1;
    std::string topName = "unknown";

    while ((entry = readdir(procDir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;

        // Check if directory name is numeric (PID)
        char* endPtr = nullptr;
        long pid = strtol(entry->d_name, &endPtr, 10);
        if (*endPtr != '\0' || pid <= 1) continue;

        std::string statPath = std::string("/proc/") + entry->d_name + "/stat";
        std::ifstream f(statPath);
        if (!f.is_open()) continue;

        std::string line;
        if (std::getline(f, line)) {
            size_t openParen = line.find('(');
            size_t closeParen = line.rfind(')');
            if (openParen != std::string::npos && closeParen != std::string::npos) {
                std::string comm = line.substr(openParen + 1, closeParen - openParen - 1);
                std::string rest = line.substr(closeParen + 2);
                std::istringstream restStream(rest);

                char state;
                int ppid, pgrp, session, tty_nr, tpgid;
                unsigned long flags, minflt, cminflt, majflt, cmajflt;
                unsigned long utime = 0, stime = 0;

                restStream >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid
                           >> flags >> minflt >> cminflt >> majflt >> cmajflt
                           >> utime >> stime;

                uint64_t totalJiffies = utime + stime;
                if (totalJiffies > maxJiffies) {
                    maxJiffies = totalJiffies;
                    topPid = static_cast<int>(pid);
                    topName = comm;
                }
            }
        }
    }
    closedir(procDir);

    metrics.topOffendingPid = topPid;
    metrics.topOffendingProcessName = topName;
}

CpuMetrics CpuMonitor::sample() {
    CpuMetrics metrics;
    metrics.coreCount = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));

    CpuSnapshot currSnapshot;
    if (!readProcStat(currSnapshot)) {
        Logger::getInstance().warn("CPU", "Failed to read /proc/stat. Utilizing baseline.");
        return metrics;
    }

    if (!m_hasInitialSample) {
        m_prevSnapshot = currSnapshot;
        m_hasInitialSample = true;
        return metrics;
    }

    uint64_t prevTotal = m_prevSnapshot.getTotal();
    uint64_t currTotal = currSnapshot.getTotal();
    uint64_t prevActive = m_prevSnapshot.getActive();
    uint64_t currActive = currSnapshot.getActive();

    uint64_t deltaTotal = (currTotal >= prevTotal) ? (currTotal - prevTotal) : 1;
    uint64_t deltaActive = (currActive >= prevActive) ? (currActive - prevActive) : 0;
    uint64_t deltaUser = (currSnapshot.user >= m_prevSnapshot.user) ? (currSnapshot.user - m_prevSnapshot.user) : 0;
    uint64_t deltaSystem = (currSnapshot.system >= m_prevSnapshot.system) ? (currSnapshot.system - m_prevSnapshot.system) : 0;
    uint64_t deltaIowait = (currSnapshot.iowait >= m_prevSnapshot.iowait) ? (currSnapshot.iowait - m_prevSnapshot.iowait) : 0;

    metrics.totalUsagePercent = (static_cast<double>(deltaActive) / static_cast<double>(deltaTotal)) * 100.0;
    metrics.userPercent = (static_cast<double>(deltaUser) / static_cast<double>(deltaTotal)) * 100.0;
    metrics.systemPercent = (static_cast<double>(deltaSystem) / static_cast<double>(deltaTotal)) * 100.0;
    metrics.iowaitPercent = (static_cast<double>(deltaIowait) / static_cast<double>(deltaTotal)) * 100.0;

    // Constrain to 0.0 - 100.0%
    metrics.totalUsagePercent = std::clamp(metrics.totalUsagePercent, 0.0, 100.0);

    // Evaluate health state
    if (metrics.totalUsagePercent >= m_thresholds.criticalPercent) {
        metrics.status = HealthStatus::CRITICAL;
        m_consecutiveBreaches++;
    } else if (metrics.totalUsagePercent >= m_thresholds.warningPercent) {
        metrics.status = HealthStatus::WARNING;
        m_consecutiveBreaches = std::max(1, m_consecutiveBreaches);
    } else {
        metrics.status = HealthStatus::NOMINAL;
        m_consecutiveBreaches = 0;
    }

    metrics.consecutiveBreaches = m_consecutiveBreaches;

    // Detect rogue process if under high load
    if (metrics.status != HealthStatus::NOMINAL) {
        detectRogueProcess(metrics);
    }

    // Trigger auto-recovery if threshold condition met
    if (m_thresholds.autoRecoveryEnabled &&
        m_consecutiveBreaches >= m_thresholds.consecutiveBreachesToTrigger) {
        Logger::getInstance().alert("CPU", "Critical CPU threshold breached for " +
            std::to_string(m_consecutiveBreaches) + " consecutive cycles (" +
            std::to_string(metrics.totalUsagePercent) + "%). Initiating Auto-Recovery.");
        executeRecoveryAction(m_thresholds.action);
        m_consecutiveBreaches = 0; // Reset breach window after action
    }

    m_prevSnapshot = currSnapshot;
    return metrics;
}

bool CpuMonitor::executeRecoveryAction(const std::string& action) {
    Logger::getInstance().recovery("CPU", "Executing CPU Auto-Recovery: [" + action + "]");

    if (action == "renice_low_priority" || action == "throttle_rogue") {
        // Lower CPU priority of rogue non-essential tasks
        CpuMetrics m;
        detectRogueProcess(m);
        if (m.topOffendingPid > 1 && m.topOffendingProcessName != "device_health_monitor") {
            int currentPri = getpriority(PRIO_PROCESS, m.topOffendingPid);
            int newPri = std::min(19, currentPri + 10);
            if (setpriority(PRIO_PROCESS, m.topOffendingPid, newPri) == 0) {
                Logger::getInstance().recovery("CPU", "Successfully reniced rogue process PID " +
                    std::to_string(m.topOffendingPid) + " (" + m.topOffendingProcessName +
                    ") from priority " + std::to_string(currentPri) + " to " + std::to_string(newPri));
                return true;
            }
        }
        // Fallback or general embedded Linux mitigation: sched_yield or governor downclock
        int res = system("renice +10 -u nobody 2>/dev/null");
        (void)res;
        return true;
    }

    // Default fallback
    int ret = system("sync");
    return (ret == 0);
}

} // namespace HealthMonitor
