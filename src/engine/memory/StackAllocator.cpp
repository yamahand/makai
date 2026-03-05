#include "StackAllocator.hpp"
#include <cstdlib>
#include <cstring>
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
    if (!m_buffer) return nullptr;  // malloc 失敗など使用不可状態のガード

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

    // ペイロード直前に prevOffset を書き込む（memcpy でアライメント非依存に）
    std::memcpy(reinterpret_cast<std::byte*>(payloadAddr) - sizeof(size_t), &m_offset, sizeof(size_t));

    m_offset += overhead + size;
    return reinterpret_cast<void*>(payloadAddr);
}

void StackAllocator::deallocate(void* ptr) {
    if (!ptr) return;
    if (!m_buffer) return;  // malloc 失敗など使用不可状態のガード（nullptr でポインタ演算を避ける）

    // ptr がバッファ範囲内かつヘッダ（prevOffset）を読み取れる位置か確認する
    auto* bytePtr  = static_cast<std::byte*>(ptr);
    auto* bufStart = static_cast<std::byte*>(m_buffer);
    auto* bufEnd   = bufStart + m_capacity;

    // ヘッダとして sizeof(size_t) バイト（prevOffset）がペイロード直前に存在する前提
    if (bytePtr < bufStart + sizeof(size_t) || bytePtr >= bufEnd) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: 範囲外ポインタ %p の解放（バッファ: [%p, %p)）",
                     ptr, static_cast<void*>(bufStart), static_cast<void*>(bufEnd));
        return;
    }

    // ペイロード直前の sizeof(size_t) バイトに
    //   [0]: prevOffset（この割り当て前のオフセット）
    // が格納されているものとして読み出す（memcpy でアライメント非依存に）
    size_t prevOffset;
    std::byte* headerPtr = bytePtr - sizeof(size_t);
    std::memcpy(&prevOffset, headerPtr, sizeof(size_t));

    // ヘッダ内容の妥当性チェック
    // prevOffset はバッファ先頭から現在の m_offset までの範囲にあるはず
    if (prevOffset > m_offset || prevOffset > m_capacity) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: 不正なヘッダ値 prevOffset=%zu (m_offset=%zu, capacity=%zu)",
                     prevOffset, m_offset, m_capacity);
        return;
    }

    // ptr 自身も、ヘッダを含む現在のスタック範囲内にあることを確認する
    if (bytePtr < bufStart + prevOffset || bytePtr >= bufStart + m_offset) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "StackAllocator: 不正なポインタ %p の解放 (prevOffset=%zu, m_offset=%zu)",
                     ptr, prevOffset, m_offset);
        return;
    }

    // 問題なければ、この割り当て前のオフセットに巻き戻す
#ifndef NDEBUG
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "StackAllocator: deallocate 実行 (ptr=%p, prevOffset=%zu -> m_offset=%zu)",
                ptr, m_offset, prevOffset);
#endif
    m_offset = prevOffset;
}

size_t StackAllocator::alignForward(size_t addr, size_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}

size_t StackAllocator::alignForward(size_t addr, size_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}
} // namespace mk::memory
