#pragma once
#include "MemoryManager.hpp"
#include "../Config.hpp"
#include <stdexcept>

namespace mk::memory {

/// MemoryManager の初期化・シャットダウンを RAII で管理するガード
/// Game クラスのメンバーとして LoggerGuard より前に宣言し、
/// MemoryManager → Logger の初期化順序を保証する。
/// 破棄時は逆順（Logger → MemoryManager）でシャットダウンされる。
class MemoryManagerGuard {
public:
    /// Config のメモリ設定を受け取り MemoryManager を初期化する
    explicit MemoryManagerGuard(const MemoryConfig& memConfig) {
        if (!MemoryManager::init(memConfig)) {
            throw std::runtime_error("MemoryManager initialization failed");
        }
    }

    ~MemoryManagerGuard() {
        MemoryManager::shutdown();
    }

    MemoryManagerGuard(const MemoryManagerGuard&) = delete;
    MemoryManagerGuard& operator=(const MemoryManagerGuard&) = delete;
    MemoryManagerGuard(MemoryManagerGuard&&) = delete;
    MemoryManagerGuard& operator=(MemoryManagerGuard&&) = delete;
};

} // namespace mk::memory
