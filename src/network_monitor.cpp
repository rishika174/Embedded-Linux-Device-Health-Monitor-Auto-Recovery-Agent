#include "network_monitor.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

namespace HealthMonitor {

NetworkMonitor::NetworkMonitor(const NetworkThresholds& thresholds)
    : m_thresholds(thresholds) {}

void NetworkMonitor::updateThresholds(const NetworkThresholds& thresholds) {
    m_thresholds = thresholds;
}

bool NetworkMonitor::checkCarrier(const std::string& iface) {
    std::string carrierPath = "/sys/class/net/" + iface + "/carrier";
    std::ifstream f(carrierPath);
    if (!f.is_open()) return false;
    int val = 0;
    f >> val;
    return (val == 1);
}

std::string NetworkMonitor::readOperState(const std::string& iface) {
    std::string path = "/sys/class/net/" + iface + "/operstate";
    std::ifstream f(path);
    if (!f.is_open()) return "unknown";
    std::string state;
    f >> state;
    return state;
}

void NetworkMonitor::readDevStats(const std::string& iface, NetworkMetrics& metrics) {
    std::ifstream f("/proc/net/dev");
    if (!f.is_open()) return;

    std::string line;
    // Skip 2 header lines
    std::getline(f, line);
    std::getline(f, line);

    while (std::getline(f, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string devName = line.substr(0, colon);
        // Trim leading spaces
        devName.erase(0, devName.find_first_not_of(" \t"));

        if (devName == iface) {
            std::string stats = line.substr(colon + 1);
            std::istringstream ss(stats);

            // rx: bytes packets errs drop fifo frame compressed multicast
            // tx: bytes packets errs drop fifo colls carrier compressed
            uint64_t rxPkts, rxDrop, rxFifo, rxFrame, rxComp, rxMulti;
            uint64_t txPkts, txDrop, txFifo, txColls, txCarrier, txComp;

            ss >> metrics.rxBytes >> rxPkts >> metrics.rxErrors >> rxDrop >> rxFifo >> rxFrame >> rxComp >> rxMulti
               >> metrics.txBytes >> txPkts >> metrics.txErrors >> txDrop >> txFifo >> txColls >> txCarrier >> txComp;
            break;
        }
    }
}

bool NetworkMonitor::checkPingSocket(const std::string& ip, int timeoutSeconds, double& latencyMs) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(53); // Standard DNS port for reliable gateway/internet ping
    inet_pton(AF_INET, ip.c_str(), &target.sin_addr);

    auto start = std::chrono::steady_clock::now();
    int res = connect(sock, reinterpret_cast<struct sockaddr*>(&target), sizeof(target));

    if (res < 0 && errno == EINPROGRESS) {
        struct pollfd pfd{};
        pfd.fd = sock;
        pfd.events = POLLOUT;

        int pollRes = poll(&pfd, 1, timeoutSeconds * 1000);
        if (pollRes > 0 && (pfd.revents & POLLOUT)) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err == 0 || err == ECONNREFUSED) { // Connection refused still implies target IP is live & routing!
                auto end = std::chrono::steady_clock::now();
                latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
                close(sock);
                return true;
            }
        }
    } else if (res == 0) {
        auto end = std::chrono::steady_clock::now();
        latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
        close(sock);
        return true;
    }

    close(sock);
    return false;
}

NetworkMetrics NetworkMonitor::sample() {
    NetworkMetrics metrics;
    metrics.interfaceName = m_thresholds.interfaceName;

    // Check sysfs carrier & operstate
    metrics.carrierDetected = checkCarrier(m_thresholds.interfaceName);
    metrics.operstate = readOperState(m_thresholds.interfaceName);

    // Fallback if specific interface (e.g. eth0) is not primary, check loopback/wlan/any active interface
    if (metrics.operstate == "unknown" || metrics.operstate == "down") {
        for (const auto& alt : {"eth0", "enp0s3", "end0", "wlan0", "lo"}) {
            std::string state = readOperState(alt);
            if (state == "up") {
                metrics.interfaceName = alt;
                metrics.operstate = "up";
                metrics.carrierDetected = true;
                break;
            }
        }
    }

    readDevStats(metrics.interfaceName, metrics);

    // Perform connectivity check
    double latency = 0.0;
    metrics.gatewayReachable = checkPingSocket(m_thresholds.pingTargetIp, m_thresholds.pingTimeoutSeconds, latency);
    metrics.pingLatencyMs = latency;

    if (!metrics.carrierDetected && metrics.operstate != "up") {
        m_consecutiveFailures++;
        metrics.status = HealthStatus::CRITICAL;
        Logger::getInstance().alert("NET", "Physical carrier link down on " + metrics.interfaceName);
    } else if (!metrics.gatewayReachable) {
        m_consecutiveFailures++;
        if (m_consecutiveFailures >= m_thresholds.maxConsecutiveFailures) {
            metrics.status = HealthStatus::CRITICAL;
            Logger::getInstance().alert("NET", "Gateway ping unreachable to " + m_thresholds.pingTargetIp +
                " for " + std::to_string(m_consecutiveFailures) + " attempts.");
        } else {
            metrics.status = HealthStatus::WARNING;
        }
    } else {
        m_consecutiveFailures = 0;
        metrics.status = HealthStatus::NOMINAL;
    }

    metrics.consecutiveFailures = m_consecutiveFailures;

    if (m_thresholds.autoRecoveryEnabled &&
        m_consecutiveFailures >= m_thresholds.maxConsecutiveFailures) {
        executeRecoveryAction(m_thresholds.action);
        m_consecutiveFailures = 0; // Reset after recovery action
    }

    return metrics;
}

bool NetworkMonitor::executeRecoveryAction(const std::string& action) {
    Logger::getInstance().recovery("NET", "Executing Network Auto-Recovery: [" + action + "] on " + m_thresholds.interfaceName);

    if (action == "restart_interface" || action == "renew_dhcp") {
        // Step 1: Bounce interface link down and up
        std::string cmd = "ip link set " + m_thresholds.interfaceName + " down && sleep 1 && ip link set " + m_thresholds.interfaceName + " up 2>/dev/null";
        int ret = system(cmd.c_str());

        // Step 2: Request DHCP lease renew
        std::string dhcpCmd = "dhclient -r " + m_thresholds.interfaceName + " 2>/dev/null || udhcpc -i " + m_thresholds.interfaceName + " -n 2>/dev/null";
        int retDhcp = system(dhcpCmd.c_str());

        (void)ret; (void)retDhcp;
        Logger::getInstance().recovery("NET", "Network interface cycle command issued.");
        return true;
    }

    return false;
}

} // namespace HealthMonitor
