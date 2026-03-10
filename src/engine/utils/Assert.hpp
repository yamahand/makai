#pragma once
// アサート用ヘルパー（日本語コメント）
#include <cstdlib>
#include <csignal>
#include <string>
#include <format>
#include "Logger.hpp"

namespace mk {
namespace assert_impl {

// デバッグブレーク（プラットフォーム選択）
inline void debugBreak() {
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(SIGTRAP)
    std::raise(SIGTRAP);
#else
    std::abort();
#endif
}

// Noreturn の失敗ハンドラ（フォーマット済メッセージ）
[[noreturn]] inline void fail(const char* expr, const char* file, int line, std::string_view msg) {
    auto full = std::format("Assertion failed: {} at {}:{}{}", expr, file, line,
                            msg.empty() ? std::string() : std::format(": {}", msg));
    if (Logger::shouldLog(LogCategory::Core, LogLevel::Error)) {
        Logger::log(LogCategory::Core, LogLevel::Error, full);
    } else {
        BootstrapLogger::error(full);
    }
    debugBreak();
    std::abort();
}

// 可変引数版ヘルパーテンプレート
template<typename... Args>
[[noreturn]] inline void failf(const char* expr, const char* file, int line,
                               std::format_string<Args...> fmt, Args&&... args) {
    auto msg = std::format(fmt, std::forward<Args>(args)...);
    fail(expr, file, line, msg);
}

} // namespace assert_impl
} // namespace mk

// マクロ（NDEBUG で無効化）
#ifndef NDEBUG
#define MK_ASSERT(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(#expr, __FILE__, __LINE__, std::string_view{}))

#define MK_ASSERT_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(#expr, __FILE__, __LINE__, (fmt), ##__VA_ARGS__))
#else
#define MK_ASSERT(expr) ((void)0)
#define MK_ASSERT_MSG(expr, fmt, ...) ((void)0)
#endif