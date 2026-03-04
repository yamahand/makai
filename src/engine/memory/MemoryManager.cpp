#include "MemoryManager.hpp"
#include <cstdlib>
#include <SDL3/SDL_log.h>

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
        config.heapAllocatorMB  <= 0 || config.doubleFrameAllocatorMB <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: MemoryConfig に無効な値が含まれています "
                     "(frame=%d, doubleFrame=%d, scene=%d, heap=%d)",
                     config.frameAllocatorMB, config.doubleFrameAllocatorMB,
                     config.sceneAllocatorMB, config.heapAllocatorMB);
        return false;
    }

    // 各アロケーターサイズの上限チェック（乗算オーバーフロー防止）
    // 1 アロケーターあたり 4096 MB を上限とする（ゲームエンジンの実用範囲）
    static constexpr int MAX_ALLOCATOR_MB = 4096;
    if (config.frameAllocatorMB       > MAX_ALLOCATOR_MB ||
        config.doubleFrameAllocatorMB > MAX_ALLOCATOR_MB ||
        config.sceneAllocatorMB       > MAX_ALLOCATOR_MB ||
        config.heapAllocatorMB        > MAX_ALLOCATOR_MB) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: MemoryConfig の値が上限(%d MB)を超えています",
                     MAX_ALLOCATOR_MB);
        return false;
    }

    const size_t frameSize       = static_cast<size_t>(config.frameAllocatorMB)       * 1024 * 1024;
    const size_t doubleFrameSize = static_cast<size_t>(config.doubleFrameAllocatorMB) * 1024 * 1024;
    const size_t sceneSize       = static_cast<size_t>(config.sceneAllocatorMB)       * 1024 * 1024;
    const size_t heapSize        = static_cast<size_t>(config.heapAllocatorMB)        * 1024 * 1024;

    // doubleFrameSize*2 の乗算オーバーフロー検出
    if (doubleFrameSize > (SIZE_MAX / 2)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: doubleFrameAllocatorMB の 2 倍がオーバーフローします");
        return false;
    }
    const size_t doubleFrameTotal = doubleFrameSize * 2;
    const size_t totalSize        = frameSize + doubleFrameTotal + sceneSize + heapSize;

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

    // マスター FreeList がバッファ全体を管理する
    mgr.m_masterResource = std::make_unique<FirstFitMemoryResource>(mgr.m_masterBuffer, totalSize);
    auto& master = mgr.m_masterResource->getAllocator();

    // 各サブアロケーターはマスター FreeList からバッファを借りる
    // （払い出したバッファは永続使用のため deallocate しない）
    void* frameBuf = master.allocate(frameSize,        alignof(std::max_align_t));
    void* dframe0  = master.allocate(doubleFrameSize,  alignof(std::max_align_t));
    void* dframe1  = master.allocate(doubleFrameSize,  alignof(std::max_align_t));
    void* sceneBuf = master.allocate(sceneSize,        alignof(std::max_align_t));

    // いずれかのサブアロケーター用バッファ確保に失敗した場合は即座に失敗として処理する
    if (!frameBuf || !dframe0 || !dframe1 || !sceneBuf) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: サブアロケーター用バッファの確保に失敗");
        mgr.m_masterResource.reset();
        std::free(mgr.m_masterBuffer);
        mgr.m_masterBuffer = nullptr;
        mgr.m_masterSize   = 0;
        return false;
    }

    mgr.m_frameAllocator = std::make_unique<LinearAllocator>(frameBuf, frameSize);
    mgr.m_doubleFrameAllocator = std::make_unique<DoubleFrameAllocator>(
        dframe0, doubleFrameSize, dframe1, doubleFrameSize);
    mgr.m_sceneAllocator = std::make_unique<LinearAllocator>(sceneBuf, sceneSize);

    // サブアロケーター予約分を記録（ヒープ統計の補正に使う）
    mgr.m_subAllocatorReservedBytes = master.getUsedBytes();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "MemoryManager: サブアロケーター予約 %zu MB / ヒープ残余 %zu MB",
                mgr.m_subAllocatorReservedBytes / (1024 * 1024),
                (totalSize - mgr.m_subAllocatorReservedBytes) / (1024 * 1024));

    return true;
}

MemoryManager::~MemoryManager() {
    // PoolAllocator デストラクタがマスター FreeList に deallocate するため
    // m_masterResource より先にプールを破棄しなければならない
    m_pools.clear();

    // サブアロケーターを破棄する（外部バッファなので free は呼ばない）
    m_frameAllocator.reset();
    m_doubleFrameAllocator.reset();
    m_sceneAllocator.reset();

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

    Stats stats;

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

    // 総割り当て回数
    stats.totalFrameAllocations = m_totalFrameAllocations;
    stats.totalSceneAllocations = m_totalSceneAllocations;

    return stats;
}

} // namespace mk::memory
