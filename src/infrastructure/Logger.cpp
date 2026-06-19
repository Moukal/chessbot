#include "Logger.hpp"
#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <vector>

std::shared_ptr<spdlog::logger> Logger::s_logger;
Logger::LogCallback Logger::s_callback;

namespace {
class CallbackSink final : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        Logger::dispatchToCallback(fmt::to_string(formatted));
    }

    void flush_() override {}
};
}

void Logger::init(const std::string& logFile) {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::debug);

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
    fileSink->set_level(spdlog::level::trace);

    auto callbackSink = std::make_shared<CallbackSink>();
    callbackSink->set_level(spdlog::level::debug);

    s_logger = std::make_shared<spdlog::logger>("chessbotx",
        spdlog::sinks_init_list{consoleSink, fileSink, callbackSink});
    s_logger->set_level(spdlog::level::trace);
    s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    s_logger->flush_on(spdlog::level::debug);
    spdlog::flush_every(std::chrono::seconds(1));
    spdlog::register_logger(s_logger);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!s_logger)
        init();
    return s_logger;
}

void Logger::setCallback(LogCallback callback) {
    s_callback = std::move(callback);
}

void Logger::dispatchToCallback(const std::string& line) {
    if (s_callback)
        s_callback(line);
}
