#pragma once
// アサート用ヘルパー（日本語コメント）
#include <cstdlib>
#include <csignal>
#include <string>
#include <string_view>
#include <utility>
#include <format>
#include "Logger.hpp"
#ifdef _MSC_VER
#  include <intrin.h>
#endif

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
// Logger::shouldLog の結果に応じて Logger へ出力し、そうでない場合は BootstrapLogger にフォールバックする
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
    // フォーマット失敗時も debugBreak()→abort() に確実に到達させるため try/catch で保護
    std::string msg;
    try {
        msg = std::format(fmt, std::forward<Args>(args)...);
    } catch (const std::format_error& e) {
        msg = std::string("(フォーマットエラー: ") + e.what() + ")";
    } catch (...) {
        msg = "(不明なフォーマットエラー)";
    }
    fail(category, expr, file, line, msg);
}

// ensure 用の失敗ハンドラ（ログ出力して続行する。クラッシュしない）
// Logger::shouldLog の結果に応じて Logger へ出力し、そうでない場合は BootstrapLogger にフォールバックする
inline void ensureFail(LogCategory category, const char* expr, const char* file, int line, std::string_view msg) {
    auto full = std::format("Ensure failed: {} at {}:{}{}", expr, file, line,
                            msg.empty() ? std::string() : std::format(": {}", msg));
    if (Logger::shouldLog(category, LogLevel::Error)) {
        Logger::log(category, LogLevel::Error, full);
    } else {
        BootstrapLogger::error(full);
    }
}

// ensure 用の可変引数版ヘルパーテンプレート
template<typename... Args>
inline void ensureFailf(LogCategory category, const char* expr, const char* file, int line,
                        std::format_string<Args...> fmt, Args&&... args) {
    std::string msg;
    try {
        msg = std::format(fmt, std::forward<Args>(args)...);
    } catch (const std::format_error& e) {
        msg = std::string("(フォーマットエラー: ") + e.what() + ")";
    } catch (...) {
        msg = "(不明なフォーマットエラー)";
    }
    ensureFail(category, expr, file, line, msg);
}

// unreachable 用の失敗ハンドラ（到達してはいけないコードパス）
// Logger が未初期化の場合のみ BootstrapLogger にフォールバックする
[[noreturn]] inline void unreachableFail(LogCategory category, const char* file, int line, std::string_view msg) {
    auto full = std::format("Unreachable code reached at {}:{}{}", file, line,
                            msg.empty() ? std::string() : std::format(": {}", msg));
    if (Logger::shouldLog(category, LogLevel::Critical)) {
        Logger::log(category, LogLevel::Critical, full);
    } else {
        BootstrapLogger::error(full);
    }
    debugBreak();
    std::abort();
}

// unreachable 用の可変引数版ヘルパーテンプレート
template<typename... Args>
[[noreturn]] inline void unreachableFailf(LogCategory category, const char* file, int line,
                                          std::format_string<Args...> fmt, Args&&... args) {
    std::string msg;
    try {
        msg = std::format(fmt, std::forward<Args>(args)...);
    } catch (const std::format_error& e) {
        msg = std::string("(フォーマットエラー: ") + e.what() + ")";
    } catch (...) {
        msg = "(不明なフォーマットエラー)";
    }
    unreachableFail(category, file, line, msg);
}

// not implemented 用の失敗ハンドラ（未実装の機能に仮置き）
// Logger が未初期化の場合のみ BootstrapLogger にフォールバックする
[[noreturn]] inline void notImplementedFail(LogCategory category, const char* file, int line, std::string_view msg) {
    auto full = std::format("Not implemented at {}:{}{}", file, line,
                            msg.empty() ? std::string() : std::format(": {}", msg));
    if (Logger::shouldLog(category, LogLevel::Critical)) {
        Logger::log(category, LogLevel::Critical, full);
    } else {
        BootstrapLogger::error(full);
    }
    debugBreak();
    std::abort();
}

// not implemented 用の可変引数版ヘルパーテンプレート
template<typename... Args>
[[noreturn]] inline void notImplementedFailf(LogCategory category, const char* file, int line,
                                             std::format_string<Args...> fmt, Args&&... args) {
    std::string msg;
    try {
        msg = std::format(fmt, std::forward<Args>(args)...);
    } catch (const std::format_error& e) {
        msg = std::string("(フォーマットエラー: ") + e.what() + ")";
    } catch (...) {
        msg = "(不明なフォーマットエラー)";
    }
    notImplementedFail(category, file, line, msg);
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

// ===========================================================================
// VERIFY マクロ（リリースでも有効。失敗時クラッシュ）
// ===========================================================================

// --- 汎用（Core カテゴリ） ---
#define MK_VERIFY(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, std::string_view{}))

#define MK_VERIFY_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Core ---
#define CORE_VERIFY(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, std::string_view{}))

#define CORE_VERIFY_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Renderer ---
#define RENDERER_VERIFY(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Renderer, #expr, __FILE__, __LINE__, std::string_view{}))

#define RENDERER_VERIFY_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Renderer, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Physics ---
#define PHYSICS_VERIFY(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Physics, #expr, __FILE__, __LINE__, std::string_view{}))

#define PHYSICS_VERIFY_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Physics, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Audio ---
#define AUDIO_VERIFY(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Audio, #expr, __FILE__, __LINE__, std::string_view{}))

#define AUDIO_VERIFY_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Audio, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Game ---
#define GAME_VERIFY(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::fail(::mk::LogCategory::Game, #expr, __FILE__, __LINE__, std::string_view{}))

#define GAME_VERIFY_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::failf(::mk::LogCategory::Game, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// ===========================================================================
// ENSURE マクロ（リリースでも有効。失敗時ログして続行）
// ===========================================================================

// --- 汎用（Core カテゴリ） ---
#define MK_ENSURE(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFail(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, std::string_view{}))

#define MK_ENSURE_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFailf(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Core ---
#define CORE_ENSURE(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFail(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, std::string_view{}))

#define CORE_ENSURE_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFailf(::mk::LogCategory::Core, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Renderer ---
#define RENDERER_ENSURE(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFail(::mk::LogCategory::Renderer, #expr, __FILE__, __LINE__, std::string_view{}))

#define RENDERER_ENSURE_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFailf(::mk::LogCategory::Renderer, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Physics ---
#define PHYSICS_ENSURE(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFail(::mk::LogCategory::Physics, #expr, __FILE__, __LINE__, std::string_view{}))

#define PHYSICS_ENSURE_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFailf(::mk::LogCategory::Physics, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Audio ---
#define AUDIO_ENSURE(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFail(::mk::LogCategory::Audio, #expr, __FILE__, __LINE__, std::string_view{}))

#define AUDIO_ENSURE_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFailf(::mk::LogCategory::Audio, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// --- Game ---
#define GAME_ENSURE(expr) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFail(::mk::LogCategory::Game, #expr, __FILE__, __LINE__, std::string_view{}))

#define GAME_ENSURE_MSG(expr, fmt, ...) \
    ((expr) ? (void)0 : ::mk::assert_impl::ensureFailf(::mk::LogCategory::Game, #expr, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

// ===========================================================================
// UNREACHABLE / NOT_IMPLEMENTED マクロ（リリースでも有効。常にクラッシュ）
// ===========================================================================

// メッセージなし版
#define MK_UNREACHABLE() \
    ::mk::assert_impl::unreachableFail(::mk::LogCategory::Core, __FILE__, __LINE__, std::string_view{})

// メッセージ付き版（フォーマット対応）
#define MK_UNREACHABLE_MSG(fmt, ...) \
    ::mk::assert_impl::unreachableFailf(::mk::LogCategory::Core, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

// メッセージなし版
#define MK_NOT_IMPLEMENTED() \
    ::mk::assert_impl::notImplementedFail(::mk::LogCategory::Core, __FILE__, __LINE__, std::string_view{})

// メッセージ付き版（フォーマット対応）
#define MK_NOT_IMPLEMENTED_MSG(fmt, ...) \
    ::mk::assert_impl::notImplementedFailf(::mk::LogCategory::Core, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)