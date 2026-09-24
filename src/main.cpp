#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>

#include "logger.h"
#include "config_manager.h"
#include "cpu_monitor.h"
#include "memory_monitor.h"
#include "disk_monitor.h"
#include "temperature_monitor.h"
#include "network_monitor.h"
#include "service_monitor.h"

using namespace HealthMonitor;

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_reloadConfig{false};

void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        g_running = false;
    } else if (signum == SIGHUP) {
        g_reloadConfig = true;
    }
}

int main(int argc, char* argv[]) {
    std::string configPath = "config/health_monitor.json";
    bool runAsDaemon = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc) configPath = argv[++i];
        } else if (arg == "--daemon" || arg == "-d") {
            runAsDaemon = true;
        }
    }

    if (runAsDaemon && daemon(0, 0) < 0) {
        std::cerr << "Failed to daemonize process\n";
        return 1;
    }

    ConfigManager& cfgMgr = ConfigManager::getInstance();
    cfgMgr.loadFromFile(configPath);
    const MonitorConfig& cfg = cfgMgr.getConfig();

    Logger& logger = Logger::getInstance();
    logger.init(cfg.logFilePath, Logger::stringToLevel(cfg.logLevel), cfg.enableStdout);

    logger.info("CORE", "Starting Embedded Linux Device Health Monitor & Recovery Agent");

    CpuMonitor cpuMon(cfg.cpu);
    MemoryMonitor memMon(cfg.memory);
    DiskMonitor diskMon(cfg.disk);
    TemperatureMonitor tempMon(cfg.temperature);
    NetworkMonitor netMon(cfg.network);
    ServiceMonitor srvMon(cfg.service);

    while (g_running) {
        cpuMon.sample();
        memMon.sample();
        diskMon.sample();
        tempMon.sample();
        netMon.sample();
        srvMon.sample();

        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.pollingIntervalMs));
    }

    return 0;
}
