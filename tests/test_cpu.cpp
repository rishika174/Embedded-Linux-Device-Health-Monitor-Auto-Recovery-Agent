#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include "cpu_monitor.h"

using namespace HealthMonitor;

void testCpuSampling() {
    std::cout << "[TEST] Running testCpuSampling..." << std::endl;

    CpuThresholds thresholds;
    thresholds.warningPercent = 70.0;
    thresholds.criticalPercent = 90.0;
    thresholds.consecutiveBreachesToTrigger = 3;
    thresholds.autoRecoveryEnabled = false;

    CpuMonitor monitor(thresholds);

    // Initial sample
    CpuMetrics m1 = monitor.sample();
    std::cout << "  Sample 1 total usage: " << m1.totalUsagePercent << "%" << std::endl;

    // Sleep briefly to accumulate CPU ticks
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    CpuMetrics m2 = monitor.sample();
    std::cout << "  Sample 2 total usage: " << m2.totalUsagePercent << "% (Cores: " << m2.coreCount << ")" << std::endl;

    assert(m2.totalUsagePercent >= 0.0 && m2.totalUsagePercent <= 100.0);
    assert(m2.coreCount >= 1);
    std::cout << "[PASS] testCpuSampling succeeded." << std::endl;
}

void testCpuThresholdEvaluation() {
    std::cout << "[TEST] Running testCpuThresholdEvaluation..." << std::endl;

    CpuThresholds thresholds;
    thresholds.warningPercent = 1.0; // Force warning
    thresholds.criticalPercent = 99.9;
    thresholds.autoRecoveryEnabled = false;

    CpuMonitor monitor(thresholds);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CpuMetrics m = monitor.sample();

    if (m.totalUsagePercent >= thresholds.warningPercent) {
        assert(m.status == HealthStatus::WARNING || m.status == HealthStatus::CRITICAL);
    }
    std::cout << "[PASS] testCpuThresholdEvaluation succeeded." << std::endl;
}

int main() {
    std::cout << "=== CPU Monitor Test Suite ===" << std::endl;
    testCpuSampling();
    testCpuThresholdEvaluation();
    std::cout << "All CPU Monitor tests PASSED!" << std::endl;
    return 0;
}
