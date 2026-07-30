#pragma once
#include <fstream>
#include <mutex>
#include <string>

class Logger {
public:
    static Logger& getInstance();
    void init(const std::string& filePath, bool append = true);
    void log(const std::string& msg);
    void flush();

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream out_;
    std::mutex mtx_;
    bool initialized_ = false;
};