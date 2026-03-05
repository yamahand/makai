#include "StackAllocator.hpp"
#include <cstdint>
#include <cstdlib>
#include <SDL3/SDL_log.h>

namespace mk::memory {

StackAllocator::StackAllocator(size_t capacity)
    : m_buffer(std::malloc(capacity))
    , m_capacity(capacity)
    , m_offset(0)
    , m_ownsBuffer(true)
{
    if (!m_buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: メモリ確保に失敗しました (%zu KB)", capacity / 1024);
        m_capacity = 0;
        return;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "StackAllocator: 内部バッファで初期化 (%zu KB)", capacity / 1024);
}

StackAllocator::StackAllocator(void* buf, size_t capacity)
    : m_buffer(buf)
    , m_capacity(capacity)
    , m_offset(0)
    , m_ownsBuffer(false)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "StackAllocator: 外部バッファで初期化 (%zu KB)", capacity / 1024);
}

StackAllocator::~StackAllocator() {
    if (m_ownsBuffer) {
        std::free(m_buffer);
    }
}

void* StackAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) return nullptr;
    if (!m_buffer) return nullptr;

    // alignment は2のべき乗かつ1以上でなければならない
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: alignment は2のべき乗でなければなりません (%zu)", alignment);
        return nullptr;
    }

    // アライメント計算は uintptr_t で行い、返却ポインタは元ポインタ + オフセットで構築する
    // （整数→ポインタ変換を避ける。prevOffset ヘッダは持たない）
    auto* base = static_cast<std::byte*>(m_buffer) + m_offset;
    std::uintptr_t baseAddr    = reinterpret_cast<std::uintptr_t>(base);
    std::uintptr_t alignedAddr = (baseAddr + alignment - 1)
                                 & ~static_cast<std::uintptr_t>(alignment - 1);
    size_t padding = static_cast<size_t>(alignedAddr - baseAddr);

    if (m_offset + padding + size > m_capacity) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: メモリ不足 (要求: %zu B, 残余: %zu B)",
                     padding + size, m_capacity - m_offset);
        return nullptr;
    }

    m_offset += padding + size;
    return base + padding;
}

void StackAllocator::deallocate(void* /*ptr*/) {
    // StackAllocator は個別解放をサポートしない。
    // 解放には StackAllocatorMarker または reset() を使うこと。
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "StackAllocator::deallocate() は使用禁止です。StackAllocatorMarker または reset() を使ってください。");
    assert(false && "StackAllocator::deallocate() is not supported");
}

std::uintptr_t StackAllocator::alignForward(std::uintptr_t addr, size_t alignment) {
    return (addr + alignment - 1) & ~static_cast<std::uintptr_t>(alignment - 1);
}

} // namespace mk::memory
