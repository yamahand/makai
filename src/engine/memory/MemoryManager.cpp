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

    const size_t frameSize = static_cast<size_t>(config.frameAllocatorMB) * 1024 * 1024;
    const size_t sceneSize = static_cast<size_t>(config.sceneAllocatorMB) * 1024 * 1024;
    const size_t heapSize  = static_cast<size_t>(config.heapAllocatorMB)  * 1024 * 1024;
    const size_t totalSize = frameSize + sceneSize + heapSize;

    mgr.m_masterBuffer = std::malloc(totalSize);
    mgr.m_masterSize   = totalSize;

    if (!mgr.m_masterBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager: Failed to allocate master buffer (%zu MB)",
                     totalSize / (1024 * 1024));
        return;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "MemoryManager: Master buffer %zu MB allocated at %p",
                totalSize / (1024 * 1024), mgr.m_masterBuffer);

    // マスターバッファを各アロケーターに分配
    auto* p = static_cast<std::byte*>(mgr.m_masterBuffer);
    mgr.m_frameAllocator = std::make_unique<LinearAllocator>(p,                      frameSize);
    mgr.m_sceneAllocator = std::make_unique<LinearAllocator>(p + frameSize,          sceneSize);
    mgr.m_heapResource   = std::make_unique<FirstFitMemoryResource>(
                               p + frameSize + sceneSize, heapSize);
}

MemoryManager::~MemoryManager() {
    // unique_ptr が各アロケーターを先に破棄する（外部バッファは free しない）
    m_frameAllocator.reset();
    m_sceneAllocator.reset();
    m_heapResource.reset();

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

    // シーンアロケーター
    stats.sceneBytes      = m_sceneAllocator->getUsedBytes();
    stats.sceneCapacity   = m_sceneAllocator->getCapacity();
    stats.sceneUsageRatio = m_sceneAllocator->getUsageRatio();

    // ヒープアロケーター（FreeList）
    const auto& heap          = m_heapResource->getAllocator();
    stats.heapBytes           = heap.getUsedBytes();
    stats.heapCapacity        = heap.getCapacity();
    stats.heapUsageRatio      = heap.getUsageRatio();
    stats.heapAllocationCount = heap.getAllocationCount();
    stats.heapFreeBlockCount  = heap.getFreeBlockCount();

    // 総割り当て回数
    stats.totalFrameAllocations = m_totalFrameAllocations;
    stats.totalSceneAllocations = m_totalSceneAllocations;

    return stats;
}

} // namespace mk::memory
