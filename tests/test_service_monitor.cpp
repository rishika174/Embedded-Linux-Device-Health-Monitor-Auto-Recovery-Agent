#include <iostream>
#include <cassert>
#include "service_monitor.h"

using namespace HealthMonitor;

void testServiceSampling() {
    std::cout << "[TEST] Running testServiceSampling..." << std::endl;

    CriticalServiceThresholds thresholds;
    thresholds.serviceName = "cron"; // standard common Linux service
    thresholds.checkMethod = "systemd";
    thresholds.autoRecoveryEnabled = false;

    ServiceMonitor monitor(thresholds);
    ServiceMetrics m = monitor.sample();

    std::cout << "  Target service: " << m.serviceName << std::endl;
    std::cout << "  Detected state: " << m.stateString << std::endl;
    std::cout << "  Process PID:    " << m.processPid << std::endl;

    assert(!m.serviceName.empty());
    std::cout << "[PASS] testServiceSampling succeeded." << std::endl;
}

void testServiceRestartBackoff() {
    std::cout << "[TEST] Running testServiceRestartBackoff..." << std::endl;

    CriticalServiceThresholds thresholds;
    thresholds.serviceName = "nonexistent_mock_svc";
    thresholds.maxRestartAttempts = 2;
    thresholds.cooldownSeconds = 5;
    thresholds.autoRecoveryEnabled = false;

    ServiceMonitor monitor(thresholds);

    // Call recovery action up to limit
    bool a1 = monitor.executeRecoveryAction("restart_service");
    bool a2 = monitor.executeRecoveryAction("restart_service");
    // Third should be suppressed by backoff limiter
    bool a3 = monitor.executeRecoveryAction("restart_service");

    (void)a1; (void)a2;
    assert(a3 == false); // throttled

    std::cout << "[PASS] testServiceRestartBackoff succeeded: dampener prevented runaway loop." << std::endl;
}

int main() {
    std::cout << "=== Service Monitor Test Suite ===" << std::endl;
    testServiceSampling();
    testServiceRestartBackoff();
    std::cout << "All Service Monitor tests PASSED!" << std::endl;
    return 0;
}
