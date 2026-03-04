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
/// padding は常に [1, alignment] の範囲に収まる（deallocate の調整量格納バイトを確保するため）
static size_t calcNeeded(FreeListBlockHeader* header, size_t size, size_t alignment) {
    auto* payloadStart = reinterpret_cast<std::byte*>(header) + sizeof(FreeListBlockHeader);
    size_t addr    = reinterpret_cast<size_t>(payloadStart);
    // addr+1 以上からアライメントすることで最低1バイトのパディングを保証する
    size_t aligned = (addr + alignment) & ~(alignment - 1);
    size_t padding = aligned - addr;  // 常に [1, alignment]
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
                if (!best.header || header->size < best.header->size) {
                    best = { header, needed };
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
    if (capacity > 0) {
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
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "FreeListAllocator<%s>: Using external buffer %zu bytes (%.2f MB)",
                typeid(SearchPolicy).name(),
                m_capacity, m_capacity / (1024.0 * 1024.0));
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
    m_usedBytes += header->size;
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
    if (bytePtr < bufStart || bytePtr >= bufEnd) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Attempt to deallocate pointer not owned by this allocator");
        return;
    }

    // ペイロード直前の 1 バイトからパディング量を読み取り、ヘッダを復元する
    size_t padding = static_cast<size_t>(*(bytePtr - 1));
    auto* header = reinterpret_cast<BlockHeader*>(bytePtr - padding - sizeof(BlockHeader));
    if (header->isFree) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "FreeListAllocator: Double-free detected");
        return;
    }

    m_usedBytes -= header->size;
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
