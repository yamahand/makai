#include "MemoryManager.hpp"
#include <cstdlib>
#include <SDL3/SDL_log.h>

namespace mk::memory {

MemoryManager& MemoryManager::instance() {
    static MemoryManager inst;
    return inst;
}

void MemoryManager::init(const mk::MemoryConfig& config) {
    auto& mgr = instance();

    const size_t frameSize       = static_cast<size_t>(config.frameAllocatorMB)       * 1024 * 1024;
    const size_t doubleFrameSize = static_cast<size_t>(config.doubleFrameAllocatorMB) * 1024 * 1024;
    const size_t sceneSize       = static_cast<size_t>(config.sceneAllocatorMB)       * 1024 * 1024;
    const size_t heapSize        = static_cast<size_t>(config.heapAllocatorMB)        * 1024 * 1024;
    const size_t totalSize       = frameSize + doubleFrameSize * 2 + sceneSize + heapSize;

    mgr.m_masterBuffer = std::malloc(totalSize);
    mgr.m_masterSize   = totalSize;

    if (!mgr.m_masterBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: マスターバッファの確保に失敗 (%zu MB)",
                     totalSize / (1024 * 1024));
        return;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "MemoryManager: マスターバッファ %zu MB を %p に確保",
                totalSize / (1024 * 1024), mgr.m_masterBuffer);

    // マスター FreeList がバッファ全体を管理する
    mgr.m_masterResource = std::make_unique<FirstFitMemoryResource>(mgr.m_masterBuffer, totalSize);
    auto& master = mgr.m_masterResource->getAllocator();

    // 各サブアロケーターはマスター FreeList からバッファを借りる
    // （払い出したバッファは永続使用のため deallocate しない）
    void* frameBuf   = master.allocate(frameSize,   alignof(std::max_align_t));
    mgr.m_frameAllocator = std::make_unique<LinearAllocator>(frameBuf, frameSize);

    void* dframe0 = master.allocate(doubleFrameSize, alignof(std::max_align_t));
    void* dframe1 = master.allocate(doubleFrameSize, alignof(std::max_align_t));
    mgr.m_doubleFrameAllocator = std::make_unique<DoubleFrameAllocator>(
        dframe0, doubleFrameSize, dframe1, doubleFrameSize);

    void* sceneBuf = master.allocate(sceneSize, alignof(std::max_align_t));
    mgr.m_sceneAllocator = std::make_unique<LinearAllocator>(sceneBuf, sceneSize);

    // サブアロケーター予約分を記録（ヒープ統計の補正に使う）
    mgr.m_subAllocatorReservedBytes = master.getUsedBytes();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "MemoryManager: サブアロケーター予約 %zu MB / ヒープ残余 %zu MB",
                mgr.m_subAllocatorReservedBytes / (1024 * 1024),
                (totalSize - mgr.m_subAllocatorReservedBytes) / (1024 * 1024));
}

MemoryManager::~MemoryManager() {
    // サブアロケーターを先に破棄（外部バッファなので free は呼ばない）
    m_frameAllocator.reset();
    m_doubleFrameAllocator.reset();
    m_sceneAllocator.reset();
    m_masterResource.reset();

    // マスターバッファを解放
    if (m_masterBuffer) {
        std::free(m_masterBuffer);
        m_masterBuffer = nullptr;
    }
}

void MemoryManager::onFrameEnd() {
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
