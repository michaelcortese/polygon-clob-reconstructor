#include "logger/Logger.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (out_.is_open()) {
        out_.close();
    }
}

void Logger::init(const std::string& filePath, bool append) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (initialized_) {
        return;
    }
    if (append) {
        out_.open(filePath, std::ios::app);
    } else {
        out_.open(filePath, std::ios::trunc);
    }
    if (!out_.is_open()) {
        return;
    }
    initialized_ = true;
}

void Logger::log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!initialized_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

#ifdef _WIN32
    struct tm local_tm;
    localtime_s(&local_tm, &time_t_now);
#else
    struct tm local_tm;
    localtime_r(&time_t_now, &local_tm);
#endif

    out_ << std::put_time(&local_tm, "[%Y-%m-%d %H:%M:%S")
         << "." << std::setfill('0') << std::setw(3) << ms.count()
         << std::setfill(' ') << "] " << msg << std::endl;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (initialized_) {
        out_.flush();
    }
}