#include "Logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <vector>

namespace mk {

namespace {

// spdlog レベルへの変換
spdlog::level::level_enum toSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return spdlog::level::trace;
        case LogLevel::Debug:    return spdlog::level::debug;
        case LogLevel::Info:     return spdlog::level::info;
        case LogLevel::Warn:     return spdlog::level::warn;
        case LogLevel::Error:    return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Off:      return spdlog::level::off;
        default:                 return spdlog::level::info;
    }
}

} // anonymous namespace

void Logger::init(std::string_view logFile) {
    // コンソールシンク（カラー付き）
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);

    // ファイルシンク
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        std::string(logFile), /*truncate=*/true);
    fileSink->set_level(spdlog::level::trace);

    // 両シンクを持つマルチシンクロガーを作成してデフォルトに設定
    std::vector<spdlog::sink_ptr> sinks{ consoleSink, fileSink };
    auto logger = std::make_shared<spdlog::logger>(
        "makai", sinks.begin(), sinks.end());

    logger->set_level(spdlog::level::trace);
    logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_default_logger(logger);

    spdlog::info("Logger initialized");
}

void Logger::shutdown() {
    spdlog::shutdown();
}

void Logger::setLevel(LogLevel level) {
    spdlog::set_level(toSpdlogLevel(level));
}

void Logger::logImpl(LogLevel level, std::string_view msg) {
    spdlog::log(toSpdlogLevel(level), msg);
}

} // namespace mk
