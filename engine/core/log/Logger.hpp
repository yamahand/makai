#pragma once
#include <string_view>
#include <format>
#include <memory_resource>
#include <utility>

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

// ログカテゴリ
enum class LogCategory {
    Core,
    Renderer,
    Physics,
    Audio,
    Game
};

// ---------------------------------------------------------------------------
// BootstrapLogger — spdlog 初期化前に使える簡易ロガー
// stderr に直接書き込む。Allocator/Logger/Engine の初期化フェーズ用。
// ---------------------------------------------------------------------------
class BootstrapLogger {
public:
    static void trace(std::string_view msg);
    static void info(std::string_view msg);
    static void warn(std::string_view msg);
    static void error(std::string_view msg);
};

// ---------------------------------------------------------------------------
// Logger — カテゴリロガー方式
// spdlog を完全に隠蔽し、ヘッダーに spdlog を露出させない。
// ---------------------------------------------------------------------------
class Logger {
public:
    // 初期化（コンソール + ファイル出力、カテゴリロガーを生成）
    // @param logFile ログファイルパス。空文字列（""）を指定した場合は、
    //                ロガー内部でタイムスタンプ付きのファイル名（例: makai-YYYYMMDD-HHMMSS.log）
    //                を自動生成して使用する。デフォルト引数 "makai.log" は固定ファイル名であり、
    //                タイムスタンプ付きファイル名を利用したい場合は明示的に空文字列を渡すこと。
    // @param memResource Logger 専用のメモリリソース（将来の spdlog 置き換え時に使用）
    static void init(std::string_view logFile = "makai.log",
                     std::pmr::memory_resource* memResource = nullptr);
    // シャットダウン（flush して終了）
    static void shutdown();
    // すべてのカテゴリロガーの出力レベルを変更する
    static void setLevel(LogLevel level);

    // カテゴリ別ログ出力（spdlog を .cpp に隠蔽）
    static void log(LogCategory category, LogLevel level, std::string_view msg);

    // 指定カテゴリ・レベルが出力対象かどうかを返す
    // テンプレート側で事前チェックし、無効レベルでの std::format コストを回避する
    static bool shouldLog(LogCategory category, LogLevel level);

    // std::format スタイルの便利テンプレート
    // shouldLog() が false の場合はフォーマット処理自体をスキップする
    template<typename... Args>
    static void log(LogCategory category, LogLevel level,
                    std::format_string<Args...> fmt, Args&&... args) {
        if (!shouldLog(category, level)) return;
        log(category, level, std::format(fmt, std::forward<Args>(args)...));
    }
};

} // namespace mk

// ===========================================================================
// ゲームエンジン用ログマクロ
// ===========================================================================

// --- Bootstrap ---
#define MK_BOOT_TRACE(msg) ::mk::BootstrapLogger::trace(msg)
#define MK_BOOT_INFO(msg)  ::mk::BootstrapLogger::info(msg)
#define MK_BOOT_WARN(msg)  ::mk::BootstrapLogger::warn(msg)
#define MK_BOOT_ERROR(msg) ::mk::BootstrapLogger::error(msg)

// --- Core ---
#define CORE_TRACE(...)    ::mk::Logger::log(::mk::LogCategory::Core, ::mk::LogLevel::Trace,    __VA_ARGS__)
#define CORE_DEBUG(...)    ::mk::Logger::log(::mk::LogCategory::Core, ::mk::LogLevel::Debug,    __VA_ARGS__)
#define CORE_INFO(...)     ::mk::Logger::log(::mk::LogCategory::Core, ::mk::LogLevel::Info,     __VA_ARGS__)
#define CORE_WARN(...)     ::mk::Logger::log(::mk::LogCategory::Core, ::mk::LogLevel::Warn,     __VA_ARGS__)
#define CORE_ERROR(...)    ::mk::Logger::log(::mk::LogCategory::Core, ::mk::LogLevel::Error,    __VA_ARGS__)
#define CORE_CRITICAL(...) ::mk::Logger::log(::mk::LogCategory::Core, ::mk::LogLevel::Critical, __VA_ARGS__)

// --- Renderer ---
#define RENDERER_TRACE(...)    ::mk::Logger::log(::mk::LogCategory::Renderer, ::mk::LogLevel::Trace,    __VA_ARGS__)
#define RENDERER_DEBUG(...)    ::mk::Logger::log(::mk::LogCategory::Renderer, ::mk::LogLevel::Debug,    __VA_ARGS__)
#define RENDERER_INFO(...)     ::mk::Logger::log(::mk::LogCategory::Renderer, ::mk::LogLevel::Info,     __VA_ARGS__)
#define RENDERER_WARN(...)     ::mk::Logger::log(::mk::LogCategory::Renderer, ::mk::LogLevel::Warn,     __VA_ARGS__)
#define RENDERER_ERROR(...)    ::mk::Logger::log(::mk::LogCategory::Renderer, ::mk::LogLevel::Error,    __VA_ARGS__)
#define RENDERER_CRITICAL(...) ::mk::Logger::log(::mk::LogCategory::Renderer, ::mk::LogLevel::Critical, __VA_ARGS__)

// --- Physics ---
#define PHYSICS_TRACE(...)    ::mk::Logger::log(::mk::LogCategory::Physics, ::mk::LogLevel::Trace,    __VA_ARGS__)
#define PHYSICS_DEBUG(...)    ::mk::Logger::log(::mk::LogCategory::Physics, ::mk::LogLevel::Debug,    __VA_ARGS__)
#define PHYSICS_INFO(...)     ::mk::Logger::log(::mk::LogCategory::Physics, ::mk::LogLevel::Info,     __VA_ARGS__)
#define PHYSICS_WARN(...)     ::mk::Logger::log(::mk::LogCategory::Physics, ::mk::LogLevel::Warn,     __VA_ARGS__)
#define PHYSICS_ERROR(...)    ::mk::Logger::log(::mk::LogCategory::Physics, ::mk::LogLevel::Error,    __VA_ARGS__)
#define PHYSICS_CRITICAL(...) ::mk::Logger::log(::mk::LogCategory::Physics, ::mk::LogLevel::Critical, __VA_ARGS__)

// --- Audio ---
#define AUDIO_TRACE(...)    ::mk::Logger::log(::mk::LogCategory::Audio, ::mk::LogLevel::Trace,    __VA_ARGS__)
#define AUDIO_DEBUG(...)    ::mk::Logger::log(::mk::LogCategory::Audio, ::mk::LogLevel::Debug,    __VA_ARGS__)
#define AUDIO_INFO(...)     ::mk::Logger::log(::mk::LogCategory::Audio, ::mk::LogLevel::Info,     __VA_ARGS__)
#define AUDIO_WARN(...)     ::mk::Logger::log(::mk::LogCategory::Audio, ::mk::LogLevel::Warn,     __VA_ARGS__)
#define AUDIO_ERROR(...)    ::mk::Logger::log(::mk::LogCategory::Audio, ::mk::LogLevel::Error,    __VA_ARGS__)
#define AUDIO_CRITICAL(...) ::mk::Logger::log(::mk::LogCategory::Audio, ::mk::LogLevel::Critical, __VA_ARGS__)

// --- Game ---
#define GAME_TRACE(...)    ::mk::Logger::log(::mk::LogCategory::Game, ::mk::LogLevel::Trace,    __VA_ARGS__)
#define GAME_DEBUG(...)    ::mk::Logger::log(::mk::LogCategory::Game, ::mk::LogLevel::Debug,    __VA_ARGS__)
#define GAME_INFO(...)     ::mk::Logger::log(::mk::LogCategory::Game, ::mk::LogLevel::Info,     __VA_ARGS__)
#define GAME_WARN(...)     ::mk::Logger::log(::mk::LogCategory::Game, ::mk::LogLevel::Warn,     __VA_ARGS__)
#define GAME_ERROR(...)    ::mk::Logger::log(::mk::LogCategory::Game, ::mk::LogLevel::Error,    __VA_ARGS__)
#define GAME_CRITICAL(...) ::mk::Logger::log(::mk::LogCategory::Game, ::mk::LogLevel::Critical, __VA_ARGS__)
