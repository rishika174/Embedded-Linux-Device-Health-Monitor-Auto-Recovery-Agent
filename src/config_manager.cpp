#include "config_manager.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>

namespace HealthMonitor {

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // Default fallback initialization
    m_config = MonitorConfig();
}

const MonitorConfig& ConfigManager::getConfig() const {
    return m_config;
}

void ConfigManager::setConfig(const MonitorConfig& newConfig) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = newConfig;
}

static std::string extractString(const std::string& json, const std::string& key, const std::string& defaultVal = "") {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return match[1].str();
    }
    return defaultVal;
}

static double extractDouble(const std::string& json, const std::string& key, double defaultVal = 0.0) {
    std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        try {
            return std::stod(match[1].str());
        } catch (...) {
            return defaultVal;
        }
    }
    return defaultVal;
}

static int extractInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        try {
            return std::stoi(match[1].str());
        } catch (...) {
            return defaultVal;
        }
    }
    return defaultVal;
}

static bool extractBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return match[1].str() == "true";
    }
    return defaultVal;
}

static std::string extractBlock(const std::string& json, const std::string& objectKey) {
    size_t keyPos = json.find("\"" + objectKey + "\"");
    if (keyPos == std::string::npos) return "";

    size_t startBrace = json.find('{', keyPos);
    if (startBrace == std::string::npos) return "";

    int depth = 0;
    size_t endBrace = startBrace;
    for (size_t i = startBrace; i < json.length(); ++i) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') {
            depth--;
            if (depth == 0) {
                endBrace = i;
                break;
            }
        }
    }
    return json.substr(startBrace, endBrace - startBrace + 1);
}

bool ConfigManager::parseJsonString(const std::string& json, MonitorConfig& cfg) {
    if (json.empty()) return false;

    cfg.deviceId = extractString(json, "device_id", cfg.deviceId);
    cfg.pollingIntervalMs = static_cast<uint32_t>(extractInt(json, "polling_interval_ms", cfg.pollingIntervalMs));
    cfg.logFilePath = extractString(json, "log_file_path", cfg.logFilePath);
    cfg.logLevel = extractString(json, "log_level", cfg.logLevel);
    cfg.enableSyslog = extractBool(json, "enable_syslog", cfg.enableSyslog);
    cfg.enableStdout = extractBool(json, "enable_stdout", cfg.enableStdout);

    // CPU block
    std::string cpuBlock = extractBlock(json, "cpu");
    if (!cpuBlock.empty()) {
        cfg.cpu.warningPercent = extractDouble(cpuBlock, "warning_percent", cfg.cpu.warningPercent);
        cfg.cpu.criticalPercent = extractDouble(cpuBlock, "critical_percent", cfg.cpu.criticalPercent);
        cfg.cpu.consecutiveBreachesToTrigger = extractInt(cpuBlock, "consecutive_breaches_to_trigger", cfg.cpu.consecutiveBreachesToTrigger);
        cfg.cpu.autoRecoveryEnabled = extractBool(cpuBlock, "auto_recovery_enabled", cfg.cpu.autoRecoveryEnabled);
        cfg.cpu.action = extractString(cpuBlock, "action", cfg.cpu.action);
    }

    // Memory block
    std::string memBlock = extractBlock(json, "memory");
    if (!memBlock.empty()) {
        cfg.memory.warningPercent = extractDouble(memBlock, "warning_percent", cfg.memory.warningPercent);
        cfg.memory.criticalPercent = extractDouble(memBlock, "critical_percent", cfg.memory.criticalPercent);
        cfg.memory.minAvailableMb = static_cast<uint64_t>(extractInt(memBlock, "min_available_mb", static_cast<int>(cfg.memory.minAvailableMb)));
        cfg.memory.autoRecoveryEnabled = extractBool(memBlock, "auto_recovery_enabled", cfg.memory.autoRecoveryEnabled);
        cfg.memory.action = extractString(memBlock, "action", cfg.memory.action);
    }

    // Disk block
    std::string diskBlock = extractBlock(json, "disk");
    if (!diskBlock.empty()) {
        cfg.disk.monitoredMountPoint = extractString(diskBlock, "monitored_mount_point", cfg.disk.monitoredMountPoint);
        cfg.disk.warningPercent = extractDouble(diskBlock, "warning_percent", cfg.disk.warningPercent);
        cfg.disk.criticalPercent = extractDouble(diskBlock, "critical_percent", cfg.disk.criticalPercent);
        cfg.disk.autoRecoveryEnabled = extractBool(diskBlock, "auto_recovery_enabled", cfg.disk.autoRecoveryEnabled);
        cfg.disk.action = extractString(diskBlock, "action", cfg.disk.action);
    }

    // Temperature block
    std::string tempBlock = extractBlock(json, "temperature");
    if (!tempBlock.empty()) {
        cfg.temperature.thermalZonePath = extractString(tempBlock, "thermal_zone_path", cfg.temperature.thermalZonePath);
        cfg.temperature.warningCelsius = extractDouble(tempBlock, "warning_celsius", cfg.temperature.warningCelsius);
        cfg.temperature.criticalCelsius = extractDouble(tempBlock, "critical_celsius", cfg.temperature.criticalCelsius);
        cfg.temperature.autoRecoveryEnabled = extractBool(tempBlock, "auto_recovery_enabled", cfg.temperature.autoRecoveryEnabled);
        cfg.temperature.action = extractString(tempBlock, "action", cfg.temperature.action);
    }

    // Network block
    std::string netBlock = extractBlock(json, "network");
    if (!netBlock.empty()) {
        cfg.network.interfaceName = extractString(netBlock, "interface_name", cfg.network.interfaceName);
        cfg.network.pingTargetIp = extractString(netBlock, "ping_target_ip", cfg.network.pingTargetIp);
        cfg.network.pingTimeoutSeconds = extractInt(netBlock, "ping_timeout_seconds", cfg.network.pingTimeoutSeconds);
        cfg.network.maxConsecutiveFailures = extractInt(netBlock, "max_consecutive_failures", cfg.network.maxConsecutiveFailures);
        cfg.network.autoRecoveryEnabled = extractBool(netBlock, "auto_recovery_enabled", cfg.network.autoRecoveryEnabled);
        cfg.network.action = extractString(netBlock, "action", cfg.network.action);
    }

    // Critical service block
    std::string srvBlock = extractBlock(json, "critical_service");
    if (!srvBlock.empty()) {
        cfg.service.serviceName = extractString(srvBlock, "service_name", cfg.service.serviceName);
        cfg.service.checkMethod = extractString(srvBlock, "check_method", cfg.service.checkMethod);
        cfg.service.maxRestartAttempts = extractInt(srvBlock, "max_restart_attempts", cfg.service.maxRestartAttempts);
        cfg.service.cooldownSeconds = extractInt(srvBlock, "cooldown_seconds", cfg.service.cooldownSeconds);
        cfg.service.autoRecoveryEnabled = extractBool(srvBlock, "auto_recovery_enabled", cfg.service.autoRecoveryEnabled);
        cfg.service.action = extractString(srvBlock, "action", cfg.service.action);
    }

    return true;
}

bool ConfigManager::loadFromFile(const std::string& configFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_configPath = configFilePath;

    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        Logger::getInstance().warn("CONFIG", "Could not open config file " + configFilePath + ", fallback to embedded defaults.");
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    MonitorConfig loadedConfig = m_config;
    if (parseJsonString(content, loadedConfig)) {
        m_config = loadedConfig;
        Logger::getInstance().info("CONFIG", "Configuration loaded successfully from " + configFilePath);
        return true;
    }

    Logger::getInstance().warn("CONFIG", "Failed to parse JSON in " + configFilePath + ", retaining current configuration.");
    return false;
}

bool ConfigManager::saveToFile(const std::string& configFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(configFilePath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"device_id\": \"" << m_config.deviceId << "\",\n";
    file << "  \"polling_interval_ms\": " << m_config.pollingIntervalMs << ",\n";
    file << "  \"log_file_path\": \"" << m_config.logFilePath << "\",\n";
    file << "  \"log_level\": \"" << m_config.logLevel << "\",\n";
    file << "  \"enable_syslog\": " << (m_config.enableSyslog ? "true" : "false") << ",\n";
    file << "  \"enable_stdout\": " << (m_config.enableStdout ? "true" : "false") << ",\n";
    file << "  \"thresholds\": {\n";
    file << "    \"cpu\": {\n";
    file << "      \"warning_percent\": " << m_config.cpu.warningPercent << ",\n";
    file << "      \"critical_percent\": " << m_config.cpu.criticalPercent << ",\n";
    file << "      \"consecutive_breaches_to_trigger\": " << m_config.cpu.consecutiveBreachesToTrigger << ",\n";
    file << "      \"auto_recovery_enabled\": " << (m_config.cpu.autoRecoveryEnabled ? "true" : "false") << ",\n";
    file << "      \"action\": \"" << m_config.cpu.action << "\"\n";
    file << "    },\n";
    file << "    \"memory\": {\n";
    file << "      \"warning_percent\": " << m_config.memory.warningPercent << ",\n";
    file << "      \"critical_percent\": " << m_config.memory.criticalPercent << ",\n";
    file << "      \"min_available_mb\": " << m_config.memory.minAvailableMb << ",\n";
    file << "      \"auto_recovery_enabled\": " << (m_config.memory.autoRecoveryEnabled ? "true" : "false") << ",\n";
    file << "      \"action\": \"" << m_config.memory.action << "\"\n";
    file << "    },\n";
    file << "    \"disk\": {\n";
    file << "      \"monitored_mount_point\": \"" << m_config.disk.monitoredMountPoint << "\",\n";
    file << "      \"warning_percent\": " << m_config.disk.warningPercent << ",\n";
    file << "      \"critical_percent\": " << m_config.disk.criticalPercent << ",\n";
    file << "      \"auto_recovery_enabled\": " << (m_config.disk.autoRecoveryEnabled ? "true" : "false") << ",\n";
    file << "      \"action\": \"" << m_config.disk.action << "\"\n";
    file << "    },\n";
    file << "    \"temperature\": {\n";
    file << "      \"thermal_zone_path\": \"" << m_config.temperature.thermalZonePath << "\",\n";
    file << "      \"warning_celsius\": " << m_config.temperature.warningCelsius << ",\n";
    file << "      \"critical_celsius\": " << m_config.temperature.criticalCelsius << ",\n";
    file << "      \"auto_recovery_enabled\": " << (m_config.temperature.autoRecoveryEnabled ? "true" : "false") << ",\n";
    file << "      \"action\": \"" << m_config.temperature.action << "\"\n";
    file << "    },\n";
    file << "    \"network\": {\n";
    file << "      \"interface_name\": \"" << m_config.network.interfaceName << "\",\n";
    file << "      \"ping_target_ip\": \"" << m_config.network.pingTargetIp << "\",\n";
    file << "      \"ping_timeout_seconds\": " << m_config.network.pingTimeoutSeconds << ",\n";
    file << "      \"max_consecutive_failures\": " << m_config.network.maxConsecutiveFailures << ",\n";
    file << "      \"auto_recovery_enabled\": " << (m_config.network.autoRecoveryEnabled ? "true" : "false") << ",\n";
    file << "      \"action\": \"" << m_config.network.action << "\"\n";
    file << "    },\n";
    file << "    \"critical_service\": {\n";
    file << "      \"service_name\": \"" << m_config.service.serviceName << "\",\n";
    file << "      \"check_method\": \"" << m_config.service.checkMethod << "\",\n";
    file << "      \"max_restart_attempts\": " << m_config.service.maxRestartAttempts << ",\n";
    file << "      \"cooldown_seconds\": " << m_config.service.cooldownSeconds << ",\n";
    file << "      \"auto_recovery_enabled\": " << (m_config.service.autoRecoveryEnabled ? "true" : "false") << ",\n";
    file << "      \"action\": \"" << m_config.service.action << "\"\n";
    file << "    }\n";
    file << "  }\n";
    file << "}\n";

    return true;
}

} // namespace HealthMonitor
