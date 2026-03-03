#include "MemoryManager.hpp"
#include <SDL3/SDL_log.h>

namespace mk::memory {

MemoryManager::MemoryManager()
    : m_frameAllocator(4 * 1024 * 1024)   // 4MB
    , m_sceneAllocator(16 * 1024 * 1024)  // 16MB
    , m_heapAllocator(32 * 1024 * 1024)   // 32MB
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "MemoryManager: Initialized");
}

MemoryManager& MemoryManager::instance() {
    static MemoryManager instance;
    return instance;
}

void MemoryManager::onFrameEnd() {
    // フレームアロケーターをリセット
    size_t usedBytes = m_frameAllocator.getUsedBytes();
    if (usedBytes > 0) {
        m_totalFrameAllocations++;
    }

    m_frameAllocator.reset();

    #ifdef DEBUG_MEMORY_VERBOSE
    if (usedBytes > 0) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                    "MemoryManager: Frame allocator reset (used: %zu bytes)", usedBytes);
    }
    #endif
}

void MemoryManager::onSceneChange() {
    // シーンアロケーターをリセット
    size_t usedBytes = m_sceneAllocator.getUsedBytes();
    if (usedBytes > 0) {
        m_totalSceneAllocations++;
    }

    m_sceneAllocator.reset();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
               "MemoryManager: Scene allocator reset (used: %zu bytes, %.2f MB)",
               usedBytes, usedBytes / (1024.0 * 1024.0));
}

MemoryManager::Stats MemoryManager::getStats() const {
    Stats stats;

    // フレームアロケーター
    stats.frameBytes = m_frameAllocator.getUsedBytes();
    stats.frameCapacity = m_frameAllocator.getCapacity();
    stats.frameUsageRatio = m_frameAllocator.getUsageRatio();

    // シーンアロケーター
    stats.sceneBytes = m_sceneAllocator.getUsedBytes();
    stats.sceneCapacity = m_sceneAllocator.getCapacity();
    stats.sceneUsageRatio = m_sceneAllocator.getUsageRatio();

    // ヒープアロケーター（FreeList）
    stats.heapBytes           = m_heapAllocator.getUsedBytes();
    stats.heapCapacity        = m_heapAllocator.getCapacity();
    stats.heapUsageRatio      = m_heapAllocator.getUsageRatio();
    stats.heapAllocationCount = m_heapAllocator.getAllocationCount();
    stats.heapFreeBlockCount  = m_heapAllocator.getFreeBlockCount();

    // 総割り当て回数
    stats.totalFrameAllocations = m_totalFrameAllocations;
    stats.totalSceneAllocations = m_totalSceneAllocations;

    return stats;
}

} // namespace mk::memory
