#include "LinearAllocator.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL_log.h>

namespace mk::memory {

LinearAllocator::LinearAllocator(size_t capacity)
    : m_buffer(nullptr)
    , m_capacity(capacity)
    , m_offset(0)
    , m_ownsBuffer(true)
{
    if (capacity > 0) {
        m_buffer = std::malloc(capacity);
        if (!m_buffer) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "LinearAllocator: Failed to allocate %zu bytes", capacity);
            m_capacity = 0;
        } else {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                       "LinearAllocator: Allocated %zu bytes (%.2f MB)",
                       capacity, capacity / (1024.0 * 1024.0));
        }
    }
}

LinearAllocator::LinearAllocator(void* buf, size_t capacity)
    : m_buffer(buf)
    , m_capacity(buf ? capacity : 0)
    , m_offset(0)
    , m_ownsBuffer(false)
{
    if (!buf) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "LinearAllocator: External buffer is null. Allocator is unusable.");
    } else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "LinearAllocator: Using external buffer %zu bytes (%.2f MB)",
                    m_capacity, m_capacity / (1024.0 * 1024.0));
    }
}

LinearAllocator::~LinearAllocator() {
    if (m_ownsBuffer && m_buffer) {
        std::free(m_buffer);
        m_buffer = nullptr;
    }
}

void* LinearAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }

    // alignment は 2 のべき乗かつ 1 以上でなければならない（他のアロケーターと同じガード）
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "LinearAllocator: alignment は 2 のべき乗でなければなりません (%zu)", alignment);
        return nullptr;
    }

    if (!m_buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "LinearAllocator: Invalid buffer (null). Allocation request denied.");
        return nullptr;
    }

    // 現在のアドレスをアライメントに合わせる（uintptr_t でビット演算、ポインタへは元ポインタ+オフセットで戻す）
    auto* base = static_cast<std::byte*>(m_buffer) + m_offset;
    std::uintptr_t baseAddr    = reinterpret_cast<std::uintptr_t>(base);
    std::uintptr_t alignedAddr = alignForward(baseAddr, alignment);
    size_t padding = static_cast<size_t>(alignedAddr - baseAddr);

    // 必要な総サイズ
    size_t totalSize = padding + size;

    // 容量チェック
    if (m_offset + totalSize > m_capacity) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "LinearAllocator: Out of memory (requested: %zu bytes, available: %zu bytes)",
                    totalSize, m_capacity - m_offset);
        return nullptr;
    }

    // ポインタを進める（バンプ割り当て）。整数→ポインタ変換を使わずポインタ演算で返す
    void* ptr = base + padding;
    m_offset += totalSize;

    return ptr;
}

void LinearAllocator::reset() {
    m_offset = 0;

    #ifdef DEBUG
    // デバッグビルドでは、メモリをクリア（use-after-free検出用）
    if (m_buffer) {
        std::memset(m_buffer, 0xCD, m_capacity);
    }
    #endif
}

std::uintptr_t LinearAllocator::alignForward(std::uintptr_t addr, size_t alignment) {
    // アライメントは2の累乗である必要がある
    // (addr + alignment - 1) & ~(alignment - 1)
    return (addr + (alignment - 1)) & ~static_cast<std::uintptr_t>(alignment - 1);
}

} // namespace mk::memory
