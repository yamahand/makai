#include "Logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <iostream>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#  include <Windows.h>
#  include "msvc_sink.hpp"
#endif

namespace mk {

// ---------------------------------------------------------------------------
// BootstrapLogger — stderr に直接書き込む簡易ロガー
// Visual Studio デバッガー起動中は OutputDebugStringW にも出力する
// ---------------------------------------------------------------------------
namespace {
// デバッガー向け出力（Windows 以外では何もしない）
void outputToDebugger(std::string_view prefix, std::string_view msg) {
#ifdef _WIN32
    if (::IsDebuggerPresent()) {
        auto line = std::string(prefix) + std::string(msg) + "\n";
        // UTF-8 → UTF-16 変換して OutputDebugStringW に渡す
        const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            std::wstring wline(static_cast<std::size_t>(wlen), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, wline.data(), wlen);
            ::OutputDebugStringW(wline.c_str());
        }
    }
#else
    (void)prefix; (void)msg;
#endif
}
} // anonymous namespace

void BootstrapLogger::trace(std::string_view msg) {
    std::cerr << "[BOOT] [trace] " << msg << '\n';
    outputToDebugger("[BOOT] [trace] ", msg);
}
void BootstrapLogger::info(std::string_view msg) {
    std::cerr << "[BOOT] [info]  " << msg << '\n';
    outputToDebugger("[BOOT] [info]  ", msg);
}
void BootstrapLogger::warn(std::string_view msg) {
    std::cerr << "[BOOT] [warn]  " << msg << '\n';
    outputToDebugger("[BOOT] [warn]  ", msg);
}
void BootstrapLogger::error(std::string_view msg) {
    std::cerr << "[BOOT] [error] " << msg << '\n';
    outputToDebugger("[BOOT] [error] ", msg);
}

// ---------------------------------------------------------------------------
// カテゴリロガーの内部ストレージ（spdlog をヘッダーに露出させない）
// ---------------------------------------------------------------------------
namespace {

// カテゴリ名（LogCategory の順序に対応）
// ※ kCategoryCount は kCategoryNames のサイズから導出することで
//   LogCategory への要素追加時の不整合を防ぐ
constexpr std::array<const char*, 5> kCategoryNames = {
    "Core", "Renderer", "Physics", "Audio", "Game"
};

// カテゴリ数（配列サイズから自動取得）
constexpr std::size_t kCategoryCount = kCategoryNames.size();

// LogCategory の要素数と一致していることをコンパイル時に保証
static_assert(kCategoryCount == static_cast<std::size_t>(LogCategory::Game) + 1,
    "kCategoryNames と LogCategory の要素数が一致しません");

// カテゴリロガーを保持する配列（spdlog::get() を毎回呼ばない）
std::array<std::shared_ptr<spdlog::logger>, kCategoryCount> s_loggers;

// Logger 専用のメモリリソース（将来の spdlog 置き換え時に使用）
std::pmr::memory_resource* s_memResource = nullptr;

// 初期化済みフラグ
bool s_initialized = false;

#ifdef _WIN32
// Logger::init() 前のコンソールコードページを保存する変数。
// shutdown() で元の値に戻すために使用する。
UINT s_prevOutputCP = 0;
UINT s_prevInputCP  = 0;

// コンソールコードページの RAII ガード
// 初期化成功時に commit() を呼ぶとデストラクタでの復元を抑止する
class CodePageGuard {
public:
    CodePageGuard()
        : m_prevOutputCP(::GetConsoleOutputCP())
        , m_prevInputCP(::GetConsoleCP())
        , m_committed(false)
    {
        ::SetConsoleOutputCP(CP_UTF8);
        ::SetConsoleCP(CP_UTF8);
    }
    ~CodePageGuard() {
        if (!m_committed) {
            ::SetConsoleOutputCP(m_prevOutputCP);
            ::SetConsoleCP(m_prevInputCP);
        }
    }
    // 初期化成功時に呼ぶ。復元値を保存し、デストラクタでの復元を抑止する
    void commit(UINT& outPrevOutput, UINT& outPrevInput) {
        outPrevOutput = m_prevOutputCP;
        outPrevInput  = m_prevInputCP;
        m_committed = true;
    }
    CodePageGuard(const CodePageGuard&) = delete;
    CodePageGuard& operator=(const CodePageGuard&) = delete;
private:
    UINT m_prevOutputCP;
    UINT m_prevInputCP;
    bool m_committed;
};
#endif

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

// カテゴリロガーを 1 つ生成するヘルパー
std::shared_ptr<spdlog::logger> createCategoryLogger(
    const std::string& name,
    const std::vector<spdlog::sink_ptr>& sinks)
{
    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    // [時刻] [カテゴリ名] [レベル] メッセージ
    logger->set_pattern("[%H:%M:%S.%e] [%n] [%^%l%$] %v");
    // spdlog のレジストリに登録（名前の衝突を避けるため drop してから）
    spdlog::drop(name);
    spdlog::register_logger(logger);
    return logger;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Logger::init — カテゴリロガーを生成
// ---------------------------------------------------------------------------
void Logger::init(std::string_view logFile, std::pmr::memory_resource* memResource) {
    // 二重初期化を防ぐ
    if (s_initialized) return;

    // Logger 専用メモリリソースを保持する（将来の spdlog 置き換え時に使用）
    s_memResource = memResource;

#ifdef _WIN32
    // コードページを UTF-8 に切り替える。例外時は CodePageGuard のデストラクタで自動復元される
    CodePageGuard cpGuard;
#endif

    // 共有シンク（コンソール + ファイル）
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        std::string(logFile), /*truncate=*/true);
    fileSink->set_level(spdlog::level::trace);

    std::vector<spdlog::sink_ptr> sinks{ consoleSink, fileSink };

#ifdef _WIN32
    // windowsのときはVisual Studio用のシンクを追加する
    auto msvcSink = std::make_shared<mk::msvc_sink_mt>();
    msvcSink->set_level(spdlog::level::trace);
    sinks.push_back(msvcSink);
#endif

    // 各カテゴリロガーを生成
    for (std::size_t i = 0; i < kCategoryCount; ++i) {
        s_loggers[i] = createCategoryLogger(kCategoryNames[i], sinks);
    }

    s_initialized = true;
    s_loggers[static_cast<std::size_t>(LogCategory::Core)]->info("Logger initialized");

#ifdef _WIN32
    // 初期化成功: コードページの復元値を記録し、デストラクタでの復元を抑止する
    // ※ ログ出力より後に commit することで、シンク例外時もコードページが復元される
    cpGuard.commit(s_prevOutputCP, s_prevInputCP);
#endif
}

// ---------------------------------------------------------------------------
// Logger::shutdown
// ---------------------------------------------------------------------------
void Logger::shutdown() {
    if (!s_initialized) return;

    // static ポインタをリセットしてから spdlog を停止
    for (auto& logger : s_loggers) {
        logger.reset();
    }

    spdlog::shutdown();
    s_initialized = false;
    s_memResource = nullptr;

#ifdef _WIN32
    // init() で変更したコードページを元の値に戻す
    ::SetConsoleOutputCP(s_prevOutputCP);
    ::SetConsoleCP(s_prevInputCP);
#endif
}

// ---------------------------------------------------------------------------
// Logger::setLevel — すべてのカテゴリロガーのレベルを一括変更
// ---------------------------------------------------------------------------
void Logger::setLevel(LogLevel level) {
    if (!s_initialized) return;

    auto spdLevel = toSpdlogLevel(level);
    for (auto& logger : s_loggers) {
        if (logger) {
            logger->set_level(spdLevel);
        }
    }
}

// ---------------------------------------------------------------------------
// Logger::shouldLog — 指定カテゴリ・レベルが出力対象かどうかを返す
// テンプレートの log() がフォーマット前に呼び出し、
// 無効レベルでの std::format コストを回避するために使う
// ---------------------------------------------------------------------------
bool Logger::shouldLog(LogCategory category, LogLevel level) {
    if (!s_initialized) return false;

    const auto idx = static_cast<std::size_t>(category);
    if (idx >= kCategoryCount) return false;

    const auto& logger = s_loggers[idx];
    if (!logger) return false;

    return logger->should_log(toSpdlogLevel(level));
}

// ---------------------------------------------------------------------------
// Logger::log — カテゴリ別ログ出力
// ---------------------------------------------------------------------------
void Logger::log(LogCategory category, LogLevel level, std::string_view msg) {
    if (!s_initialized) return;

    // 不正な enum 値による範囲外アクセスを防ぐ
    const auto idx = static_cast<std::size_t>(category);
    if (idx >= kCategoryCount) return;

    const auto& logger = s_loggers[idx];
    if (logger) {
        // "{}" 経由で渡すことで msg 中の {} をフォーマット文字として解釈させない
        logger->log(toSpdlogLevel(level), "{}", msg);
    }
}

} // namespace mk
