#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <mutex>
#include <memory>

namespace rx {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& instance();

    void init(const std::string& logFilePath = "rx_player.log");
    void log(LogLevel level, std::string_view message, const char* file = nullptr, int line = 0);

    void setLevel(LogLevel level) { m_minLevel = level; }
    LogLevel getLevel() const { return m_minLevel; }

private:
    Logger() = default;
    ~Logger();

    std::mutex m_mutex;
    std::ofstream m_fileStream;
    LogLevel m_minLevel = LogLevel::Info;
    bool m_initialized = false;
};

#define RX_LOG_DEBUG(msg) rx::Logger::instance().log(rx::LogLevel::Debug, msg, __FILE__, __LINE__)
#define RX_LOG_INFO(msg)  rx::Logger::instance().log(rx::LogLevel::Info,  msg, __FILE__, __LINE__)
#define RX_LOG_WARN(msg)  rx::Logger::instance().log(rx::LogLevel::Warning, msg, __FILE__, __LINE__)
#define RX_LOG_ERROR(msg) rx::Logger::instance().log(rx::LogLevel::Error, msg, __FILE__, __LINE__)

} // namespace rx
