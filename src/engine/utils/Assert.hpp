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
[[noreturn]] inline void fail(LogCategory category, const char* expr, const char* file, int line, std::string_view msg) {
    auto full = std::format("Assertion failed: {} at {}:{}{}", expr, file, line,
                            msg.empty() ? std::string() : std::format(": {}", msg));
    // アサートはログレベル設定に関係なく必ずカテゴリロガーへ出力する
    Logger::log(category, LogLevel::Error, full);
    // spdlog 初期化前のケースでもログが残るよう、ブートストラップ側にも出力しておく
    BootstrapLogger::error(full);
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