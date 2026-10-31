#include "logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace rx {

Logger& Logger::instance() {
    static Logger s_inst;
    return s_inst;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.flush();
        m_fileStream.close();
    }
}

void Logger::init(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
    m_fileStream.open(logFilePath, std::ios::out | std::ios::app);
    m_initialized = true;
}

void Logger::log(LogLevel level, std::string_view message, const char* file, int line) {
    if (level < m_minLevel) return;

    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tmNow{};
#if defined(_WIN32)
    localtime_s(&tmNow, &timeT);
#else
    localtime_r(&timeT, &tmNow);
#endif

    const char* lvlStr = "INFO";
    switch (level) {
        case LogLevel::Debug:   lvlStr = "DEBUG"; break;
        case LogLevel::Info:    lvlStr = "INFO";  break;
        case LogLevel::Warning: lvlStr = "WARN";  break;
        case LogLevel::Error:   lvlStr = "ERROR"; break;
    }

    std::ostringstream oss;
    oss << "[" << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
        << "[" << lvlStr << "] "
        << message;

    if (file && level >= LogLevel::Warning) {
        oss << " (" << file << ":" << line << ")";
    }
    oss << "\n";

    std::string formatted = oss.str();

    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << formatted;
    if (m_fileStream.is_open()) {
        m_fileStream << formatted;
        m_fileStream.flush();
    }
}

} // namespace rx
