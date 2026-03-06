#include "MemoryManager.hpp"
#include <cstdlib>
#include <new>
#include <SDL3/SDL_log.h>

// MemoryManager は 64bit 環境専用（MB→byte 変換で size_t オーバーフローを起こさないための前提）
static_assert(sizeof(size_t) >= 8, "MemoryManager は 64bit 環境でのみ使用可能です");

namespace mk::memory {

MemoryManager& MemoryManager::instance() {
    static MemoryManager inst;
    return inst;
}

bool MemoryManager::init(const mk::MemoryConfig& config) {
    auto& mgr = instance();

    // 多重初期化を防ぐ（2回目以降は無視する）
    if (mgr.m_masterBuffer != nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "MemoryManager::init が複数回呼ばれました。2回目以降は無視します。");
        return true;
    }

    // MemoryConfig の値が負または 0 でないことを検証する
    if (config.frameAllocatorMB <= 0 || config.sceneAllocatorMB <= 0 ||
        config.heapAllocatorMB  <= 0 || config.doubleFrameAllocatorMB <= 0 ||
        config.stackAllocatorMB <= 0 || config.buddyAllocatorMB <= 0 ||
        config.pagedAllocatorPageKB <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: MemoryConfig に無効な値が含まれています "
                     "(frame=%d, doubleFrame=%d, scene=%d, heap=%d, stack=%d, buddy=%d, pagedPageKB=%d)",
                     config.frameAllocatorMB, config.doubleFrameAllocatorMB,
                     config.sceneAllocatorMB, config.heapAllocatorMB,
                     config.stackAllocatorMB, config.buddyAllocatorMB,
                     config.pagedAllocatorPageKB);
        return false;
    }

    // 各アロケーターサイズの上限チェック（乗算オーバーフロー防止）
    // 1 アロケーターあたり 4096 MB を上限とする（ゲームエンジンの実用範囲）
    static constexpr int MAX_ALLOCATOR_MB  = 4096;
    // pagedAllocatorPageKB は 1 KB〜1 GB（= 1024*1024 KB）を上限とする
    static constexpr int MAX_PAGE_KB       = 1024 * 1024;
    if (config.frameAllocatorMB       > MAX_ALLOCATOR_MB ||
        config.doubleFrameAllocatorMB > MAX_ALLOCATOR_MB ||
        config.sceneAllocatorMB       > MAX_ALLOCATOR_MB ||
        config.heapAllocatorMB        > MAX_ALLOCATOR_MB ||
        config.stackAllocatorMB       > MAX_ALLOCATOR_MB ||
        config.buddyAllocatorMB       > MAX_ALLOCATOR_MB) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: MemoryConfig の値が上限(%d MB)を超えています",
                     MAX_ALLOCATOR_MB);
        return false;
    }
    if (config.pagedAllocatorPageKB > MAX_PAGE_KB) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: pagedAllocatorPageKB の値が上限(%d KB)を超えています",
                     MAX_PAGE_KB);
        return false;
    }

    const size_t frameSize       = static_cast<size_t>(config.frameAllocatorMB)       * 1024 * 1024;
    const size_t doubleFrameSize = static_cast<size_t>(config.doubleFrameAllocatorMB) * 1024 * 1024;
    const size_t sceneSize       = static_cast<size_t>(config.sceneAllocatorMB)       * 1024 * 1024;
    const size_t heapSize        = static_cast<size_t>(config.heapAllocatorMB)        * 1024 * 1024;
    const size_t stackSize       = static_cast<size_t>(config.stackAllocatorMB)       * 1024 * 1024;
    const size_t buddySize       = static_cast<size_t>(config.buddyAllocatorMB)       * 1024 * 1024;

    // doubleFrameSize*2 の乗算オーバーフロー検出
    if (doubleFrameSize > (SIZE_MAX / 2)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: doubleFrameAllocatorMB の 2 倍がオーバーフローします");
        return false;
    }
    const size_t doubleFrameTotal = doubleFrameSize * 2;
    const size_t totalSize        = frameSize + doubleFrameTotal + sceneSize + heapSize
                                    + stackSize + buddySize;

    // 加算オーバーフロー検出（各項より合計が小さくなった場合に折り返しを検知）
    if (totalSize < frameSize || totalSize < heapSize) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: MemoryConfig の合計サイズがオーバーフローしました");
        return false;
    }

    mgr.m_masterBuffer = std::malloc(totalSize);
    mgr.m_masterSize   = totalSize;

    if (!mgr.m_masterBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: マスターバッファの確保に失敗 (%zu MB)",
                     totalSize / (1024 * 1024));
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "MemoryManager: マスターバッファ %zu MB を %p に確保",
                totalSize / (1024 * 1024), mgr.m_masterBuffer);

    // make_unique / emplace は bad_alloc を投げ得る。
    // 例外発生時に masterBuffer リーク＋次回 init 不可を防ぐため try/catch で包む。
    auto rollback = [&]() {
        mgr.m_pagedAllocator.reset();
        mgr.m_buddyAllocator.reset();
        mgr.m_stackAllocator.reset();
        if (mgr.m_pools.has_value()) {
            mgr.m_pools->clear();  // PoolHolderDeleter を起動（まだ空なので no-op）
            mgr.m_pools.reset();   // optional 自体を破棄
        }
        mgr.m_frameAllocator.reset();
        mgr.m_doubleFrameAllocator.reset();
        mgr.m_sceneAllocator.reset();
        mgr.m_masterResource.reset();
        std::free(mgr.m_masterBuffer);
        mgr.m_masterBuffer = nullptr;
        mgr.m_masterSize   = 0;
    };

    try {
        // マスター FreeList がバッファ全体を管理する
        mgr.m_masterResource = std::make_unique<FirstFitMemoryResource>(mgr.m_masterBuffer, totalSize);
        auto& master = mgr.m_masterResource->getAllocator();

        // m_pools をマスター FreeList の pmr リソースで初期化する（OS ヒープを使わない）
        mgr.m_pools.emplace(mgr.m_masterResource.get());

        // 各サブアロケーターはマスター FreeList からバッファを借りる
        // （払い出したバッファは永続使用のため deallocate しない）
        void* frameBuf = master.allocate(frameSize,        alignof(std::max_align_t));
        void* dframe0  = master.allocate(doubleFrameSize,  alignof(std::max_align_t));
        void* dframe1  = master.allocate(doubleFrameSize,  alignof(std::max_align_t));
        void* sceneBuf = master.allocate(sceneSize,        alignof(std::max_align_t));
        void* stackBuf = master.allocate(stackSize,        alignof(std::max_align_t));
        void* buddyBuf = master.allocate(buddySize,        alignof(std::max_align_t));

        // いずれかのサブアロケーター用バッファ確保に失敗した場合は即座に失敗として処理する
        if (!frameBuf || !dframe0 || !dframe1 || !sceneBuf || !stackBuf || !buddyBuf) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "MemoryManager: サブアロケーター用バッファの確保に失敗");
            rollback();
            return false;
        }

        mgr.m_frameAllocator = std::make_unique<LinearAllocator>(frameBuf, frameSize);
        mgr.m_doubleFrameAllocator = std::make_unique<DoubleFrameAllocator>(
            dframe0, doubleFrameSize, dframe1, doubleFrameSize);
        mgr.m_sceneAllocator = std::make_unique<LinearAllocator>(sceneBuf, sceneSize);
        mgr.m_stackAllocator = std::make_unique<StackAllocator>(stackBuf, stackSize);
        mgr.m_buddyAllocator = std::make_unique<BuddyAllocator>(buddyBuf, buddySize);

        // 固定バッファのサブアロケーター予約分をここで記録する
        // （PagedAllocator の初期ページ確保分をヒープ統計から除外しないため、
        //   PagedAllocator 生成より前に記録しなければならない）
        mgr.m_subAllocatorReservedBytes = master.getUsedBytes();

        // ページドアロケーターはバッキングとしてヒープ（マスター FreeList の残余）を使う
        const size_t pageSize = static_cast<size_t>(config.pagedAllocatorPageKB) * 1024;
        mgr.m_pagedAllocator = std::make_unique<PagedAllocator>(pageSize, master);
    } catch (const std::bad_alloc& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: bad_alloc 例外によりリソース確保に失敗: %s", e.what());
        rollback();
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "MemoryManager: サブアロケーター予約 %zu MB / ヒープ残余 %zu MB",
                mgr.m_subAllocatorReservedBytes / (1024 * 1024),
                (totalSize - mgr.m_subAllocatorReservedBytes) / (1024 * 1024));

    return true;
}

MemoryManager::~MemoryManager() {
    // PoolHolderDeleter がマスター FreeList に deallocate するため
    // m_masterResource より先にプールを破棄しなければならない
    if (m_pools.has_value()) {
        m_pools->clear();   // 各 PoolHolderDeleter を起動（PoolAllocator + ブロック配列 + PoolHolder 本体を FreeList に返却）
        m_pools.reset();    // pmr マップのバケット配列も FreeList に返却
    }

    // PagedAllocator のデストラクタがマスター FreeList に deallocate するため先に破棄する
    m_pagedAllocator.reset();

    // サブアロケーターを破棄する（外部バッファなので free は呼ばない）
    m_frameAllocator.reset();
    m_doubleFrameAllocator.reset();
    m_sceneAllocator.reset();
    m_stackAllocator.reset();
    m_buddyAllocator.reset();

    // マスターリソース自体を破棄する（FreeListMemoryResource は reset 不要、直後に破棄される）
    // マスター FreeListAllocator のカウンタをクリアしてからリソース自体を破棄する
    if (m_masterResource) {
        m_masterResource->reset();
        m_masterResource.reset();
    }
    // マスターバッファを解放する
    if (m_masterBuffer) {
        std::free(m_masterBuffer);
        m_masterBuffer = nullptr;
    }
}

void MemoryManager::onFrameEnd() {
    // init() 未実行／失敗時はアロケーターが nullptr のためガードする
    if (!m_frameAllocator || !m_doubleFrameAllocator) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MemoryManager::onFrameEnd: 未初期化のためスキップ");
        return;
    }

    size_t usedBytes = m_frameAllocator->getUsedBytes();
    if (usedBytes > 0) {
        m_totalFrameAllocations++;
    }
    m_frameAllocator->reset();

    // ダブルフレームアロケーターをスワップ（新フロントをリセット）
    m_doubleFrameAllocator->swap();

    #ifdef DEBUG_MEMORY_VERBOSE
    if (usedBytes > 0) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                    "MemoryManager: Frame allocator reset (used: %zu bytes)", usedBytes);
    }
    #endif
}

void MemoryManager::onSceneChange() {
    // init() 未実行／失敗時はアロケーターが nullptr のためガードする
    if (!m_sceneAllocator) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MemoryManager::onSceneChange: 未初期化のためスキップ");
        return;
    }

    size_t usedBytes = m_sceneAllocator->getUsedBytes();
    if (usedBytes > 0) {
        m_totalSceneAllocations++;
    }

    m_sceneAllocator->reset();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
               "MemoryManager: Scene allocator reset (used: %zu bytes, %.2f MB)",
               usedBytes, usedBytes / (1024.0 * 1024.0));
}

MemoryManager::Stats MemoryManager::getStats() const {
    // init() 未呼び出しまたは失敗時はゼロ値を返す
    if (!m_masterResource || !m_frameAllocator ||
        !m_doubleFrameAllocator || !m_sceneAllocator) {
        return Stats{};
    }

    Stats stats{};

    // フレームアロケーター
    stats.frameBytes      = m_frameAllocator->getUsedBytes();
    stats.frameCapacity   = m_frameAllocator->getCapacity();
    stats.frameUsageRatio = m_frameAllocator->getUsageRatio();

    // ダブルフレームアロケーター（現フロント）
    const auto& dfa = m_doubleFrameAllocator->current();
    stats.doubleFrameCurrentBytes        = dfa.getUsedBytes();
    stats.doubleFrameCapacity            = dfa.getCapacity();
    stats.doubleFrameCurrentUsageRatio   = dfa.getUsageRatio();

    // シーンアロケーター
    stats.sceneBytes      = m_sceneAllocator->getUsedBytes();
    stats.sceneCapacity   = m_sceneAllocator->getCapacity();
    stats.sceneUsageRatio = m_sceneAllocator->getUsageRatio();

    // ヒープアロケーター（マスター FreeList のサブアロケーター予約分を除いた実効値）
    const auto& master             = m_masterResource->getAllocator();
    const size_t masterUsed        = master.getUsedBytes();
    stats.heapBytes                = masterUsed > m_subAllocatorReservedBytes
                                         ? masterUsed - m_subAllocatorReservedBytes : 0;
    stats.heapCapacity             = master.getCapacity() > m_subAllocatorReservedBytes
                                         ? master.getCapacity() - m_subAllocatorReservedBytes : 0;
    stats.heapUsageRatio           = stats.heapCapacity > 0
                                         ? static_cast<float>(stats.heapBytes) / stats.heapCapacity : 0.0f;
    stats.heapAllocationCount      = master.getAllocationCount();
    stats.heapFreeBlockCount       = master.getFreeBlockCount();
    stats.masterTotalBytes         = masterUsed;
    stats.masterTotalCapacity      = master.getCapacity();

    // スタックアロケーター
    if (m_stackAllocator) {
        stats.stackBytes      = m_stackAllocator->getUsedBytes();
        stats.stackCapacity   = m_stackAllocator->getCapacity();
        stats.stackUsageRatio = m_stackAllocator->getUsageRatio();
    }

    // バディアロケーター
    if (m_buddyAllocator) {
        stats.buddyBytes      = m_buddyAllocator->getUsedBytes();
        stats.buddyCapacity   = m_buddyAllocator->getCapacity();
        stats.buddyUsageRatio = m_buddyAllocator->getUsageRatio();
    }

    // ページドアロケーター
    if (m_pagedAllocator) {
        stats.pagedBytes         = m_pagedAllocator->getUsedBytes();
        stats.pagedTotalCapacity = m_pagedAllocator->getTotalCapacity();
        stats.pagedPageCount     = m_pagedAllocator->getPageCount();
        stats.pagedUsageRatio    = m_pagedAllocator->getUsageRatio();
    }

    // 総割り当て回数
    stats.totalFrameAllocations = m_totalFrameAllocations;
    stats.totalSceneAllocations = m_totalSceneAllocations;

    return stats;
}

} // namespace mk::memory
