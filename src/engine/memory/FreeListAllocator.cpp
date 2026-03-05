#include "FreeListAllocator.hpp"
#include <cstdlib>
#include <cstring>
#include <typeinfo>
#include <SDL3/SDL_log.h>

namespace mk::memory {

// ブロックを分割するか否かの閾値（ヘッダ + 最小ペイロード）
static constexpr size_t MIN_SPLIT_PAYLOAD = 8;

// ─────────────────────────────────────────────
// ポリシー実装
// ─────────────────────────────────────────────

/// アライメントパディングを計算するヘルパー
/// padding は [1, alignment] の範囲（r = addr % alignment とすると padding = alignment - r）
/// ※ r == 0（既にアライン済み）でも最低 1 バイトのパディングを確保する（deallocate 用）
static size_t calcNeeded(FreeListBlockHeader* header, size_t size, size_t alignment) {
    auto* payloadStart = reinterpret_cast<std::byte*>(header) + sizeof(FreeListBlockHeader);
    std::uintptr_t addr    = reinterpret_cast<std::uintptr_t>(payloadStart);
    // 標準アライメント計算。addr が既にアライン済みの場合は aligned == addr になるため、
    // alignment を加算して最低 1 バイトのパディングを明示的に確保する。
    std::uintptr_t aligned = (addr + alignment - 1) & ~static_cast<std::uintptr_t>(alignment - 1);
    if (aligned == addr) aligned += alignment;
    size_t padding = static_cast<size_t>(aligned - addr);  // 常に [1, alignment]
    return padding + size;
}

FreeListSearchResult FirstFitPolicy::findBlock(FreeListBlockHeader* first,
                                               size_t size, size_t alignment,
                                               const std::byte* bufferEnd)
{
    auto* header = first;
    while (reinterpret_cast<const std::byte*>(header) < bufferEnd) {
        if (header->isFree) {
            size_t needed = calcNeeded(header, size, alignment);
            if (header->size >= needed) {
                return { header, needed };
            }
        }
        auto* next = reinterpret_cast<std::byte*>(header)
                     + sizeof(FreeListBlockHeader) + header->size;
        if (next >= bufferEnd) break;
        header = reinterpret_cast<FreeListBlockHeader*>(next);
    }
    return {};
}

FreeListSearchResult BestFitPolicy::findBlock(FreeListBlockHeader* first,
                                              size_t size, size_t alignment,
                                              const std::byte* bufferEnd)
{
    FreeListSearchResult best{};
    auto* header = first;

    while (reinterpret_cast<const std::byte*>(header) < bufferEnd) {
        if (header->isFree) {
            size_t needed = calcNeeded(header, size, alignment);
            if (header->size >= needed) {
                // 余剰が最も小さいブロックを選ぶ
                size_t surplus = header->size - needed;
                if (!best.header) {
                    best = { header, needed };
                } else {
                    size_t bestSurplus = best.header->size - best.needed;
                    if (surplus < bestSurplus) {
                        best = { header, needed };
                    }
                }
            }
        }
        auto* next = reinterpret_cast<std::byte*>(header)
                     + sizeof(FreeListBlockHeader) + header->size;
        if (next >= bufferEnd) break;
        header = reinterpret_cast<FreeListBlockHeader*>(next);
    }
    return best;
}

// ─────────────────────────────────────────────
// FreeListAllocator テンプレートメソッド実装
// ─────────────────────────────────────────────

template<typename SearchPolicy>
FreeListAllocator<SearchPolicy>::FreeListAllocator(size_t capacity)
    : m_buffer(nullptr)
    , m_capacity(capacity)
    , m_usedBytes(0)
    , m_allocationCount(0)
    , m_ownsBuffer(true)
{
    if (capacity > sizeof(BlockHeader)) {
        m_buffer = std::malloc(capacity);
        if (!m_buffer) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "FreeListAllocator: Failed to allocate %zu bytes", capacity);
            m_capacity = 0;
            return;
        }

        auto* header         = static_cast<BlockHeader*>(m_buffer);
        header->size         = capacity - sizeof(BlockHeader);
        header->isFree       = true;
        header->prevPhysical = nullptr;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "FreeListAllocator<%s>: Allocated %zu bytes (%.2f MB)",
                    typeid(SearchPolicy).name(),
                    capacity, capacity / (1024.0 * 1024.0));
    } else {
        // ヘッダ 1 つ分にも満たない容量は使用不可（capacity == 0 を含む）
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: 容量 (%zu B) が小さすぎます (最低 %zu B 必要)",
                     capacity, sizeof(BlockHeader) + 1);
        m_capacity = 0;
    }
}

template<typename SearchPolicy>
FreeListAllocator<SearchPolicy>::FreeListAllocator(void* buf, size_t capacity)
    : m_buffer(buf)
    , m_capacity(buf ? capacity : 0)
    , m_usedBytes(0)
    , m_allocationCount(0)
    , m_ownsBuffer(false)
{
    if (m_buffer && m_capacity > sizeof(BlockHeader)) {
        // バッファ全体を単一のフリーブロックとして初期化
        auto* header         = static_cast<BlockHeader*>(m_buffer);
        header->size         = m_capacity - sizeof(BlockHeader);
        header->isFree       = true;
        header->prevPhysical = nullptr;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "FreeListAllocator<%s>: Using external buffer %zu bytes (%.2f MB)",
                    typeid(SearchPolicy).name(),
                    m_capacity, m_capacity / (1024.0 * 1024.0));
    } else if (!buf) {
        // nullptr バッファ
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: 外部バッファが nullptr です");
    } else {
        // 容量がヘッダ 1 つ分にも満たない場合は使用不可にする
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: バッファ容量 (%zu B) が小さすぎます (最低 %zu B 必要)",
                     capacity, sizeof(BlockHeader) + 1);
        m_buffer   = nullptr;
        m_capacity = 0;
    }
}

template<typename SearchPolicy>
FreeListAllocator<SearchPolicy>::~FreeListAllocator() {
    if (m_allocationCount > 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "FreeListAllocator: Destroyed with %zu allocations still active",
                    m_allocationCount);
    }
    if (m_ownsBuffer && m_buffer) {
        std::free(m_buffer);
        m_buffer = nullptr;
    }
}

template<typename SearchPolicy>
void* FreeListAllocator<SearchPolicy>::allocate(size_t size, size_t alignment) {
    if (size == 0 || !m_buffer) return nullptr;

    // alignment は2のべき乗かつ1以上でなければならない
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: alignment は2のべき乗でなければなりません (%zu)",
                     alignment);
        return nullptr;
    }
    // パディング量を uint8_t 1 バイトに格納するため alignment は 255 以下に制限する
    if (alignment > 255) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: alignment は 255 以下でなければなりません (%zu)",
                     alignment);
        return nullptr;
    }

    const std::byte* bufferEnd = static_cast<std::byte*>(m_buffer) + m_capacity;
    auto result = SearchPolicy::findBlock(static_cast<BlockHeader*>(m_buffer),
                                          size, alignment, bufferEnd);
    if (!result.header) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Out of memory (requested: %zu bytes)", size);
        return nullptr;
    }

    BlockHeader* header = result.header;
    size_t needed       = result.needed;
    size_t padding      = needed - size;  // calcNeeded により padding >= 1 が保証されている

    splitBlock(header, needed);
    header->isFree = false;
    m_usedBytes += sizeof(BlockHeader) + header->size;
    ++m_allocationCount;

    // ペイロード直前の 1 バイトにパディング量を格納する（deallocate でヘッダ位置を復元するため）
    auto* returnPtr = reinterpret_cast<std::byte*>(header) + sizeof(BlockHeader) + padding;
    *(returnPtr - 1) = static_cast<std::byte>(padding);

    return returnPtr;
}

template<typename SearchPolicy>
void FreeListAllocator<SearchPolicy>::deallocate(void* ptr) {
    if (!ptr || !m_buffer) return;

    auto* bytePtr  = static_cast<std::byte*>(ptr);
    auto* bufStart = static_cast<std::byte*>(m_buffer);
    auto* bufEnd   = bufStart + m_capacity;
    if (bytePtr <= bufStart || bytePtr >= bufEnd) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Attempt to deallocate pointer not owned by this allocator");
        return;
    }

    // ペイロード直前の 1 バイトからパディング量を読み取り、ヘッダを復元する
    // まずこの 1 バイト自体がバッファ範囲内にあることを確認する
    auto* paddingBytePtr = bytePtr - 1;
    if (paddingBytePtr < bufStart || paddingBytePtr >= bufEnd) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Invalid deallocation (padding byte out of range)");
        return;
    }

    size_t padding = static_cast<size_t>(*paddingBytePtr);
    // padding は [1, 255] の範囲であること、かつヘッダ復元時にバッファ先頭を下回らないことを確認
    if (padding == 0 || padding > 255 ||
        padding > static_cast<size_t>(bytePtr - bufStart)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Invalid deallocation (invalid padding value %zu)", padding);
        return;
    }

    auto* headerBytes = bytePtr - padding - sizeof(BlockHeader);
    // 復元したヘッダ自体がバッファ範囲内に収まっていることを確認
    if (headerBytes < bufStart ||
        headerBytes + sizeof(BlockHeader) > bufEnd) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Invalid deallocation (header out of range)");
        return;
    }

    auto* header = reinterpret_cast<BlockHeader*>(headerBytes);

    // ヘッダが示すブロック範囲がバッファ境界を越えないことを確認
    auto* payloadStart = reinterpret_cast<std::byte*>(header) + sizeof(BlockHeader);
    if (payloadStart >= bufEnd || header->size == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Invalid deallocation (zero-sized or out-of-range block)");
        return;
    }
    auto* blockEnd = payloadStart + header->size;
    if (blockEnd > bufEnd) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Invalid deallocation (block size out of range)");
        return;
    }

    // ヘッダとパディングから再構成したペイロードアドレスが ptr と一致することを確認
    if (payloadStart + padding != bytePtr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Invalid deallocation (pointer does not match header)");
        return;
    }

    if (header->isFree) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "FreeListAllocator: Double-free detected");
        return;
    }

    m_usedBytes -= sizeof(BlockHeader) + header->size;
    --m_allocationCount;
    header->isFree = true;

    // コアレス: 次の物理ブロックが free なら結合
    BlockHeader* next = nextPhysical(header);
    if (next && next->isFree) {
        mergeWithNext(header);
    }

    // コアレス: 前の物理ブロックが free なら結合
    if (header->prevPhysical && header->prevPhysical->isFree) {
        mergeWithNext(header->prevPhysical);
    }
}

template<typename SearchPolicy>
void FreeListAllocator<SearchPolicy>::reset() {
    if (!m_buffer) return;

    auto* header         = static_cast<BlockHeader*>(m_buffer);
    header->size         = m_capacity - sizeof(BlockHeader);
    header->isFree       = true;
    header->prevPhysical = nullptr;

    m_usedBytes       = 0;
    m_allocationCount = 0;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "FreeListAllocator: Reset");
}

template<typename SearchPolicy>
size_t FreeListAllocator<SearchPolicy>::getFreeBlockCount() const {
    if (!m_buffer) return 0;

    size_t count = 0;
    const auto* header   = static_cast<const BlockHeader*>(m_buffer);
    const std::byte* end = static_cast<const std::byte*>(m_buffer) + m_capacity;

    while (reinterpret_cast<const std::byte*>(header) < end) {
        if (header->isFree) ++count;
        const auto* next = reinterpret_cast<const std::byte*>(header)
                           + sizeof(BlockHeader) + header->size;
        if (next >= end) break;
        header = reinterpret_cast<const BlockHeader*>(next);
    }
    return count;
}

// ─────────────────────────────────────────────
// プライベートヘルパー
// ─────────────────────────────────────────────

template<typename SearchPolicy>
FreeListBlockHeader*
FreeListAllocator<SearchPolicy>::nextPhysical(BlockHeader* header) const {
    auto* next = reinterpret_cast<std::byte*>(header)
                 + sizeof(BlockHeader) + header->size;
    const auto* end = static_cast<const std::byte*>(m_buffer) + m_capacity;
    if (next >= end) return nullptr;
    return reinterpret_cast<BlockHeader*>(next);
}

template<typename SearchPolicy>
void FreeListAllocator<SearchPolicy>::splitBlock(BlockHeader* header, size_t size) {
    // 次ブロックのヘッダが正しく整列されるよう size を alignof(BlockHeader) 倍数に切り上げる
    const size_t align = alignof(BlockHeader);
    size = (size + align - 1) & ~(align - 1);

    const size_t minSplitSize = sizeof(BlockHeader) + MIN_SPLIT_PAYLOAD;
    if (header->size < size + minSplitSize) return;

    auto* newHeader         = reinterpret_cast<BlockHeader*>(
        reinterpret_cast<std::byte*>(header) + sizeof(BlockHeader) + size);
    newHeader->size         = header->size - size - sizeof(BlockHeader);
    newHeader->isFree       = true;
    newHeader->prevPhysical = header;

    BlockHeader* afterNew = nextPhysical(newHeader);
    if (afterNew) afterNew->prevPhysical = newHeader;

    header->size = size;
}

template<typename SearchPolicy>
void FreeListAllocator<SearchPolicy>::mergeWithNext(BlockHeader* header) {
    BlockHeader* next = nextPhysical(header);
    if (!next) return;

    header->size += sizeof(BlockHeader) + next->size;

    BlockHeader* afterNext = nextPhysical(header);
    if (afterNext) afterNext->prevPhysical = header;
}

// ─────────────────────────────────────────────
// 明示的インスタンス化
// ─────────────────────────────────────────────

template class FreeListAllocator<FirstFitPolicy>;
template class FreeListAllocator<BestFitPolicy>;

} // namespace mk::memory
