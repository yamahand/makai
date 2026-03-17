#pragma once
#include "Logger.hpp"
#include <memory_resource>

namespace mk {

/// Logger の初期化・シャットダウンを RAII で管理するガード
/// MemoryManagerGuard より後に宣言すること（MemoryManager → Logger の初期化順序を保証）
class LoggerGuard {
public:
    /// @param memResource MemoryManager の Logger 専用リソース（nullptr の場合は OS ヒープを使用）
    explicit LoggerGuard(std::pmr::memory_resource* memResource = nullptr) {
        Logger::init("makai.log", memResource);
    }
    ~LoggerGuard() {
        Logger::shutdown();
    }

    LoggerGuard(const LoggerGuard&) = delete;
    LoggerGuard& operator=(const LoggerGuard&) = delete;
    LoggerGuard(LoggerGuard&&) = delete;
    LoggerGuard& operator=(LoggerGuard&&) = delete;
};

} // namespace mk
