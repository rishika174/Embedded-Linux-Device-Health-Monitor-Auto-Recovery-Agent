#include "service_monitor.h"
#include "logger.h"
#include <array>
#include <memory>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

namespace HealthMonitor {

ServiceMonitor::ServiceMonitor(const CriticalServiceThresholds& thresholds)
    : m_thresholds(thresholds),
      m_lastRestartTime(std::chrono::steady_clock::now()) {}

void ServiceMonitor::updateThresholds(const CriticalServiceThresholds& thresholds) {
    m_thresholds = thresholds;
}

static std::string execCommand(const std::string& cmd) {
    std::array<char, 128> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

ServiceState ServiceMonitor::querySystemdStatus(const std::string& serviceName, int& pid, std::string& rawStatus) {
    std::string cmd = "systemctl is-active " + serviceName + " 2>/dev/null";
    rawStatus = execCommand(cmd);

    if (rawStatus == "active") {
        std::string pidCmd = "systemctl show -p MainPID --value " + serviceName + " 2>/dev/null";
        std::string pidStr = execCommand(pidCmd);
        try {
            pid = std::stoi(pidStr);
        } catch (...) {
            pid = -1;
        }
        return ServiceState::ACTIVE_RUNNING;
    } else if (rawStatus == "failed") {
        return ServiceState::FAILED;
    } else if (rawStatus == "inactive") {
        return ServiceState::INACTIVE_DEAD;
    } else if (rawStatus == "activating") {
        return ServiceState::ACTIVATING;
    } else if (rawStatus == "deactivating") {
        return ServiceState::DEACTIVATING;
    }

    // Process table fallback if systemd unit is not registered (e.g. running standalone process)
    std::string pgrepCmd = "pgrep -f " + serviceName + " | head -n 1 2>/dev/null";
    std::string foundPid = execCommand(pgrepCmd);
    if (!foundPid.empty()) {
        try {
            pid = std::stoi(foundPid);
            rawStatus = "active (pid match)";
            return ServiceState::ACTIVE_RUNNING;
        } catch (...) {}
    }

    rawStatus = "not-found";
    return ServiceState::NOT_FOUND;
}

ServiceMetrics ServiceMonitor::sample() {
    ServiceMetrics metrics;
    metrics.serviceName = m_thresholds.serviceName;
    metrics.restartCount = m_totalRestarts;

    int pid = -1;
    std::string rawState;
    metrics.state = querySystemdStatus(m_thresholds.serviceName, pid, rawState);
    metrics.stateString = rawState;
    metrics.processPid = pid;

    if (metrics.state == ServiceState::ACTIVE_RUNNING) {
        metrics.status = HealthStatus::NOMINAL;
    } else {
        metrics.status = HealthStatus::CRITICAL;
        metrics.lastFailureReason = "Service state: " + rawState;

        Logger::getInstance().alert("SERVICE", "Critical service [" + m_thresholds.serviceName +
            "] is not active! Detected state: " + rawState);

        if (m_thresholds.autoRecoveryEnabled) {
            executeRecoveryAction(m_thresholds.action);
        }
    }

    return metrics;
}

bool ServiceMonitor::executeRecoveryAction(const std::string& action) {
    auto now = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastRestartTime).count();

    // Reset attempt window if cooldown elapsed
    if (elapsedSeconds > m_thresholds.cooldownSeconds) {
        m_restartAttemptsInWindow = 0;
    }

    if (m_restartAttemptsInWindow >= m_thresholds.maxRestartAttempts) {
        Logger::getInstance().fatal("SERVICE", "Service [" + m_thresholds.serviceName +
            "] exceeded max recovery attempts (" + std::to_string(m_thresholds.maxRestartAttempts) +
            ") within " + std::to_string(m_thresholds.cooldownSeconds) + "s cooldown. Dampening restart to prevent thrashing.");
        return false;
    }

    Logger::getInstance().recovery("SERVICE", "Executing Service Auto-Recovery: [" + action +
        "] on service " + m_thresholds.serviceName + " (Attempt " +
        std::to_string(m_restartAttemptsInWindow + 1) + "/" +
        std::to_string(m_thresholds.maxRestartAttempts) + ")");

    m_restartAttemptsInWindow++;
    m_totalRestarts++;
    m_lastRestartTime = now;

    return restartSystemdService(m_thresholds.serviceName);
}

bool ServiceMonitor::restartSystemdService(const std::string& serviceName) {
    std::string cmd = "systemctl restart " + serviceName + " 2>/dev/null";
    int ret = system(cmd.c_str());
    if (ret == 0) {
        Logger::getInstance().recovery("SERVICE", "Successfully triggered systemctl restart on " + serviceName);
        return true;
    }

    // Fallback: try starting binary if known in /usr/bin or /usr/local/bin
    std::string fallbackCmd = "/usr/local/bin/" + serviceName + " & 2>/dev/null";
    int fRet = system(fallbackCmd.c_str());
    if (fRet == 0) {
        Logger::getInstance().recovery("SERVICE", "Spawned fallback process for " + serviceName);
        return true;
    }

    Logger::getInstance().warn("SERVICE", "Failed to restart " + serviceName + " (check permissions or unit existence)");
    return false;
}

} // namespace HealthMonitor
