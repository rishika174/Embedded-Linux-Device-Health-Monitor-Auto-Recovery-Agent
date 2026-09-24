#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <chrono>
#include <ctime>

namespace HealthMonitor {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ALERT = 3,
    RECOVERY = 4,
    FATAL = 5
};

class Logger {
public:
    static Logger& getInstance();

    void init(const std::string& logFilePath, LogLevel minLevel = LogLevel::INFO, bool enableStdout = true);
    void log(LogLevel level, const std::string& subsystem, const std::string& message);

    void debug(const std::string& subsystem, const std::string& message);
    void info(const std::string& subsystem, const std::string& message);
    void warn(const std::string& subsystem, const std::string& message);
    void alert(const std::string& subsystem, const std::string& message);
    void recovery(const std::string& subsystem, const std::string& message);
    void fatal(const std::string& subsystem, const std::string& message);

    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

    static std::string levelToString(LogLevel level);
    static LogLevel stringToLevel(const std::string& str);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string getCurrentTimestamp();
    void rotateLogIfNeeded();

    std::string m_logFilePath;
    LogLevel m_minLevel;
    bool m_enableStdout;
    std::ofstream m_fileStream;
    std::mutex m_mutex;
    size_t m_maxBytes;
};

} // namespace HealthMonitor
