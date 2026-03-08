#include "Logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <vector>

namespace mk {

namespace {

// 初期化済みフラグ
bool m_initialized = false;

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
        // default を持たず、未対応の enum 値はコンパイラ警告で検出する
    }
    return spdlog::level::info; // 到達不能だが MSVC の警告抑制のため残す
}

} // anonymous namespace

void Logger::init(std::string_view logFile) {
    // 二重初期化を防ぐ
    if (m_initialized) return;

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
    m_initialized = true;
}

void Logger::shutdown() {
    spdlog::shutdown();
    m_initialized = false;
}

void Logger::setLevel(LogLevel level) {
    spdlog::set_level(toSpdlogLevel(level));
}

void Logger::logImpl(LogLevel level, std::string_view msg) {
    // "{}" 経由で渡すことで msg 中の {} をフォーマット文字として解釈させない
    spdlog::log(toSpdlogLevel(level), "{}", msg);
}

} // namespace mk
