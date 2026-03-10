#pragma once
// アサート用ヘルパー（日本語コメント）
#include <cstdlib>
#include <csignal>
#include <string>
#include <string_view>
#include <utility>
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
[[noreturn]] inline void fail(LogCategory category, const char* expr, const char* file, int line, std::string_view msg) {
    auto full = std::format("Assertion failed: {} at {}:{}{}", expr, file, line,
                            msg.empty() ? std::string() : std::format(": {}", msg));
    if (Logger::shouldLog(category, LogLevel::Error)) {
        Logger::log(category, LogLevel::Error, full);
    } else {
        BootstrapLogger::error(full);
    }
    debugBreak();
    std::abort();
}

// 可変引数版ヘルパーテンプレート
template<typename... Args>
[[noreturn]] inline void failf(LogCategory category, const char* expr, const char* file, int line,
                               std::format_string<Args...> fmt, Args&&... args) {
    auto msg = std::format(fmt, std::forward<Args>(args)...);
    fail(category, expr, file, line, msg);
}

} // namespace assert_impl
} // namespace mk

// ===========================================================================
// アサートマクロ（NDEBUG で無効化）
// ===========================================================================

#ifndef NDEBUG

// --- 汎用（Core カテゴリ） ---
#define MK_ASSERT(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, std::string_view{}))

#define MK_ASSERT_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Core ---
#define CORE_ASSERT(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, std::string_view{}))

#define CORE_ASSERT_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Renderer ---
#define RENDERER_ASSERT(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Renderer, #expr, __FILE__, __LINE__, std::string_view{}))

#define RENDERER_ASSERT_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Renderer, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Physics ---
#define PHYSICS_ASSERT(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Physics, #expr, __FILE__, __LINE__, std::string_view{}))

#define PHYSICS_ASSERT_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Physics, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Audio ---
#define AUDIO_ASSERT(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Audio, #expr, __FILE__, __LINE__, std::string_view{}))

#define AUDIO_ASSERT_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Audio, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Game ---
#define GAME_ASSERT(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Game, #expr, __FILE__, __LINE__, std::string_view{}))

#define GAME_ASSERT_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Game, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

#else

// --- 汎用 ---
#define MK_ASSERT(expr)               ((void)0)
#define MK_ASSERT_MSG(expr, fmt, ...) ((void)0)

// --- Core ---
#define CORE_ASSERT(expr)               ((void)0)
#define CORE_ASSERT_MSG(expr, fmt, ...) ((void)0)

// --- Renderer ---
#define RENDERER_ASSERT(expr)               ((void)0)
#define RENDERER_ASSERT_MSG(expr, fmt, ...) ((void)0)

// --- Physics ---
#define PHYSICS_ASSERT(expr)               ((void)0)
#define PHYSICS_ASSERT_MSG(expr, fmt, ...) ((void)0)

// --- Audio ---
#define AUDIO_ASSERT(expr)               ((void)0)
#define AUDIO_ASSERT_MSG(expr, fmt, ...) ((void)0)

// --- Game ---
#define GAME_ASSERT(expr)               ((void)0)
#define GAME_ASSERT_MSG(expr, fmt, ...) ((void)0)

#endif