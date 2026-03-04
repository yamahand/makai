#include "StackAllocator.hpp"
#include <cstdlib>
#include <SDL3/SDL_log.h>

namespace mk::memory {

StackAllocator::StackAllocator(size_t capacity)
    : m_buffer(std::malloc(capacity))
    , m_capacity(capacity)
    , m_offset(0)
    , m_ownsBuffer(true)
{
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

    // alignment は2のべき乗かつ1以上でなければならない
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: alignment は2のべき乗でなければなりません (%zu)", alignment);
        return nullptr;
    }

    // ペイロードの直前に prevOffset (size_t) を格納する。
    // レイアウト: [...パディング...][prevOffset: size_t][ペイロード]
    //                                                   ↑ここを返す
    //
    // ペイロードアドレス = alignUp(base + sizeof(size_t), alignment)
    // prevOffset スロット = ペイロードアドレス - sizeof(size_t)

    size_t base       = reinterpret_cast<size_t>(m_buffer) + m_offset;
    size_t payloadAddr = (base + sizeof(size_t) + alignment - 1) & ~(alignment - 1);
    size_t overhead    = payloadAddr - base; // パディング + prevOffset スロット分

    if (m_offset + overhead + size > m_capacity) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: メモリ不足 (要求: %zu B, 残余: %zu B)",
                     overhead + size, m_capacity - m_offset);
        return nullptr;
    }

    // ペイロード直前に prevOffset を書き込む
    *reinterpret_cast<size_t*>(payloadAddr - sizeof(size_t)) = m_offset;

    m_offset += overhead + size;
    return reinterpret_cast<void*>(payloadAddr);
}

void StackAllocator::deallocate(void* ptr) {
    if (!ptr) return;

    // ペイロードの直前 sizeof(size_t) バイトに格納された prevOffset を読み出す
    size_t prevOffset = *reinterpret_cast<size_t*>(
        static_cast<std::byte*>(ptr) - sizeof(size_t));

    // バッファ範囲内の値であることを確認する
    if (prevOffset > m_offset) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: 不正な prevOffset (%zu), LIFO 順以外での解放",
                     prevOffset);
        return;
    }

    m_offset = prevOffset;
}

size_t StackAllocator::alignForward(size_t addr, size_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}

} // namespace mk::memory
