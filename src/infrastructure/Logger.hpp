#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <functional>
#include <string>

class Logger {
public:
    using LogCallback = std::function<void(const std::string&)>;

    static void init(const std::string& logFile = "chessbotx.log");
    static std::shared_ptr<spdlog::logger> get();
    static void setCallback(LogCallback callback);
    static void dispatchToCallback(const std::string& line);

private:
    static std::shared_ptr<spdlog::logger> s_logger;
    static LogCallback s_callback;
};

#define LOG_TRACE(...)    Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...)     Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...)     Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    Logger::get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::get()->critical(__VA_ARGS__)
