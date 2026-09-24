#include "logger.h"
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace HealthMonitor {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_logFilePath("/var/log/device-health-monitor.log"),
      m_minLevel(LogLevel::INFO),
      m_enableStdout(true),
      m_maxBytes(5 * 1024 * 1024) // 5 MB rotation limit
{}

Logger::~Logger() {
    if (m_fileStream.is_open()) {
        m_fileStream.flush();
        m_fileStream.close();
    }
}

void Logger::init(const std::string& logFilePath, LogLevel minLevel, bool enableStdout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logFilePath = logFilePath;
    m_minLevel = minLevel;
    m_enableStdout = enableStdout;

    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }

    if (!m_logFilePath.empty()) {
        m_fileStream.open(m_logFilePath, std::ios::out | std::ios::app);
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm timeInfo{};
    localtime_r(&in_time_t, &timeInfo);

    std::ostringstream ss;
    ss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARN:     return "WARN";
        case LogLevel::ALERT:    return "ALERT";
        case LogLevel::RECOVERY: return "RECOVERY";
        case LogLevel::FATAL:    return "FATAL";
        default:                 return "UNKNOWN";
    }
}

LogLevel Logger::stringToLevel(const std::string& str) {
    if (str == "DEBUG") return LogLevel::DEBUG;
    if (str == "INFO") return LogLevel::INFO;
    if (str == "WARN") return LogLevel::WARN;
    if (str == "ALERT") return LogLevel::ALERT;
    if (str == "RECOVERY") return LogLevel::RECOVERY;
    if (str == "FATAL") return LogLevel::FATAL;
    return LogLevel::INFO;
}

void Logger::rotateLogIfNeeded() {
    if (m_logFilePath.empty()) return;

    struct stat st{};
    if (stat(m_logFilePath.c_str(), &st) == 0 && static_cast<size_t>(st.st_size) > m_maxBytes) {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        std::string rotated = m_logFilePath + ".1";
        rename(m_logFilePath.c_str(), rotated.c_str());
        m_fileStream.open(m_logFilePath, std::ios::out | std::ios::trunc);
    }
}

void Logger::log(LogLevel level, const std::string& subsystem, const std::string& message) {
    if (level < m_minLevel) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = levelToString(level);

    std::ostringstream formatted;
    formatted << "[" << timestamp << "] "
              << "[" << std::setw(8) << std::left << levelStr << "] "
              << "[" << subsystem << "] "
              << message;

    std::string logLine = formatted.str();

    if (m_enableStdout) {
        const char* color = "\033[0m";
        switch (level) {
            case LogLevel::DEBUG:    color = "\033[90m"; break; // Dark gray
            case LogLevel::INFO:     color = "\033[32m"; break; // Green
            case LogLevel::WARN:     color = "\033[33m"; break; // Yellow
            case LogLevel::ALERT:    color = "\033[31m"; break; // Red
            case LogLevel::RECOVERY: color = "\033[36m\033[1m"; break; // Bold Cyan
            case LogLevel::FATAL:    color = "\033[41m\033[37m"; break; // Red background
        }
        std::cout << color << logLine << "\033[0m" << std::endl;
    }

    if (m_fileStream.is_open()) {
        rotateLogIfNeeded();
        m_fileStream << logLine << "\n";
        m_fileStream.flush();
    }
}

} // namespace HealthMonitor
