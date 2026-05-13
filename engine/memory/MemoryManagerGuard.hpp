#pragma once
#include "MemoryManager.hpp"
#include "../Config.hpp"
#include <cstdio>
#include <cstdlib>

namespace mk::memory {

/// MemoryManager の初期化・シャットダウンを RAII で管理するガード
/// Game クラスのメンバーとして LoggerGuard より前に宣言し、
/// MemoryManager → Logger の初期化順序を保証する。
/// 破棄時は逆順（Logger → MemoryManager）でシャットダウンされる。
class MemoryManagerGuard {
public:
    /// Config のメモリ設定を受け取り MemoryManager を初期化する
    /// 初期化失敗は致命的エラーとして abort する
    /// OOM の可能性があるため std::format / ログ出力を伴わない固定メッセージで終了する
    explicit MemoryManagerGuard(const MemoryConfig& memConfig)
        : m_initializedByThisGuard(MemoryManager::init(memConfig)) {
        if (!m_initializedByThisGuard) {
            std::fputs("MemoryManager の初期化に失敗しました。詳細は BootstrapLogger / ログ出力を参照してください。\n", stderr);
            std::fflush(stderr);
            std::abort();
        }
    }

    ~MemoryManagerGuard() {
        /// 自分で初期化した場合のみシャットダウンを行う
        if (m_initializedByThisGuard) {
            MemoryManager::shutdown();
        }
    }

    MemoryManagerGuard(const MemoryManagerGuard&) = delete;
    MemoryManagerGuard& operator=(const MemoryManagerGuard&) = delete;
    MemoryManagerGuard(MemoryManagerGuard&&) = delete;
    MemoryManagerGuard& operator=(MemoryManagerGuard&&) = delete;

private:
    /// このガード自身が初期化を成功させたかどうか
    bool m_initializedByThisGuard{false};
};

} // namespace mk::memory
