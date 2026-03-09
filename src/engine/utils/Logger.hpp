#pragma once
#include <string_view>
#include <format>

namespace mk {

// ログレベル（spdlog を隠蔽するため独自定義）
enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off
};

// ロガーラッパー
// game 側は spdlog を直接インクルードせずこのクラスを使う
class Logger {
public:
    // 初期化（コンソール + ファイル出力）
    static void init(std::string_view logFile = "makai.log");
    // シャットダウン（flushして終了）
    static void shutdown();
    // 出力レベルを変更する
    static void setLevel(LogLevel level);

    // std::format スタイルの API（spdlog を隠蔽）
    template<typename... Args>
    static void trace(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(LogLevel::Trace, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void debug(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void critical(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(LogLevel::Critical, std::format(fmt, std::forward<Args>(args)...));
    }

private:
    // spdlog の実装を .cpp 側に隠蔽
    static void logImpl(LogLevel level, std::string_view msg);
};

} // namespace mk
