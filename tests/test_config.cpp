#include <iostream>
#include <cassert>
#include "config_manager.h"

using namespace HealthMonitor;

void testJsonParsing() {
    std::cout << "[TEST] Running testJsonParsing..." << std::endl;

    std::string sampleJson = R"({
        "device_id": "test-box-99",
        "polling_interval_ms": 1500,
        "log_level": "DEBUG",
        "thresholds": {
            "cpu": {
                "warning_percent": 65.5,
                "critical_percent": 88.0,
                "consecutive_breaches_to_trigger": 4,
                "auto_recovery_enabled": true,
                "action": "renice_low_priority"
            },
            "memory": {
                "warning_percent": 75.0,
                "critical_percent": 92.5,
                "min_available_mb": 128
            },
            "temperature": {
                "warning_celsius": 68.0,
                "critical_celsius": 79.5
            }
        }
    })";

    MonitorConfig cfg;
    bool ok = ConfigManager::parseJsonString(sampleJson, cfg);

    assert(ok == true);
    assert(cfg.deviceId == "test-box-99");
    assert(cfg.pollingIntervalMs == 1500);
    assert(cfg.logLevel == "DEBUG");
    assert(cfg.cpu.warningPercent == 65.5);
    assert(cfg.cpu.criticalPercent == 88.0);
    assert(cfg.cpu.consecutiveBreachesToTrigger == 4);
    assert(cfg.cpu.autoRecoveryEnabled == true);
    assert(cfg.memory.minAvailableMb == 128);
    assert(cfg.temperature.criticalCelsius == 79.5);

    std::cout << "[PASS] testJsonParsing succeeded with all fields validated." << std::endl;
}

void testDefaultConfigFallback() {
    std::cout << "[TEST] Running testDefaultConfigFallback..." << std::endl;

    MonitorConfig cfg;
    bool ok = ConfigManager::parseJsonString("", cfg);
    assert(ok == false);
    // Preserves default values
    assert(cfg.cpu.criticalPercent == 85.0);
    assert(cfg.pollingIntervalMs == 2000);

    std::cout << "[PASS] testDefaultConfigFallback succeeded." << std::endl;
}

int main() {
    std::cout << "=== Config Manager Test Suite ===" << std::endl;
    testJsonParsing();
    testDefaultConfigFallback();
    std::cout << "All Config Manager tests PASSED!" << std::endl;
    return 0;
}
