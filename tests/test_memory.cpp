#include <iostream>
#include <cassert>
#include "memory_monitor.h"

using namespace HealthMonitor;

void testMemorySampling() {
    std::cout << "[TEST] Running testMemorySampling..." << std::endl;

    MemoryThresholds thresholds;
    thresholds.warningPercent = 80.0;
    thresholds.criticalPercent = 95.0;
    thresholds.minAvailableMb = 32;
    thresholds.autoRecoveryEnabled = false;

    MemoryMonitor monitor(thresholds);
    MemoryMetrics m = monitor.sample();

    std::cout << "  Total RAM: " << (m.totalBytes / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Available RAM: " << (m.availableBytes / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Used Percent: " << m.usedPercent << "%" << std::endl;

    assert(m.totalBytes > 0);
    assert(m.availableBytes <= m.totalBytes);
    assert(m.usedPercent >= 0.0 && m.usedPercent <= 100.0);

    std::cout << "[PASS] testMemorySampling succeeded." << std::endl;
}

void testMemoryRecoveryMock() {
    std::cout << "[TEST] Running testMemoryRecoveryMock..." << std::endl;

    MemoryThresholds thresholds;
    thresholds.autoRecoveryEnabled = true;
    thresholds.action = "drop_caches";

    MemoryMonitor monitor(thresholds);
    // Execute action verification
    bool result = monitor.executeRecoveryAction("drop_caches");
    // Result may be true or false depending on root permissions, but should not crash
    std::cout << "  Recovery action returned: " << (result ? "success" : "unprivileged fallback") << std::endl;

    std::cout << "[PASS] testMemoryRecoveryMock passed safely without faults." << std::endl;
}

int main() {
    std::cout << "=== Memory Monitor Test Suite ===" << std::endl;
    testMemorySampling();
    testMemoryRecoveryMock();
    std::cout << "All Memory Monitor tests PASSED!" << std::endl;
    return 0;
}
