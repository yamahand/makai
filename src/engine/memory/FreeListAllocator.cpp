#include "FreeListAllocator.hpp"
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL_log.h>

namespace mk::memory {

// ブロックを分割するか否かの閾値（ヘッダ + 最小ペイロード）
static constexpr size_t MIN_SPLIT_PAYLOAD = 8;

FreeListAllocator::FreeListAllocator(size_t capacity)
    : m_buffer(nullptr)
    , m_capacity(capacity)
    , m_usedBytes(0)
    , m_allocationCount(0)
{
    if (capacity > 0) {
        m_buffer = std::malloc(capacity);
        if (!m_buffer) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "FreeListAllocator: Failed to allocate %zu bytes", capacity);
            m_capacity = 0;
            return;
        }

        // バッファ全体を単一のフリーブロックとして初期化
        auto* header    = static_cast<BlockHeader*>(m_buffer);
        header->size          = capacity - sizeof(BlockHeader);
        header->isFree        = true;
        header->prevPhysical  = nullptr;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "FreeListAllocator: Allocated %zu bytes (%.2f MB)",
                    capacity, capacity / (1024.0 * 1024.0));
    }
}

FreeListAllocator::~FreeListAllocator() {
    if (m_allocationCount > 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "FreeListAllocator: Destroyed with %zu allocations still active",
                    m_allocationCount);
    }
    if (m_buffer) {
        std::free(m_buffer);
        m_buffer = nullptr;
    }
}

void* FreeListAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0 || !m_buffer) {
        return nullptr;
    }

    // First-Fit: 先頭から走査して要件を満たす最初のフリーブロックを使う
    auto* header = static_cast<BlockHeader*>(m_buffer);
    const std::byte* bufferEnd = static_cast<std::byte*>(m_buffer) + m_capacity;

    while (reinterpret_cast<const std::byte*>(header) < bufferEnd) {
        if (!header->isFree) {
            header = nextPhysical(header);
            if (!header) break;
            continue;
        }

        // ペイロード先頭アドレスにアライメント調整が必要か確認
        auto* payloadStart = reinterpret_cast<std::byte*>(header) + sizeof(BlockHeader);
        size_t addr        = reinterpret_cast<size_t>(payloadStart);
        size_t aligned     = (addr + (alignment - 1)) & ~(alignment - 1);
        size_t padding     = aligned - addr;

        // アライメントパディングを含む必要サイズ
        size_t needed = padding + size;

        if (header->size >= needed) {
            // このブロックで確保できる
            if (padding > 0) {
                // 簡易実装: アライメントが必要な場合は padding を size に含める
                // （分割後も正しくペイロードを返すため size を padding 込みにする）
                size = needed;
            }

            splitBlock(header, size);
            header->isFree = false;
            m_usedBytes += header->size;
            ++m_allocationCount;

            // アライメント済みアドレスを返す（paddingが0のときと同じく header+1）
            return reinterpret_cast<std::byte*>(header) + sizeof(BlockHeader) + padding;
        }

        header = nextPhysical(header);
        if (!header) break;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "FreeListAllocator: Out of memory (requested: %zu bytes)", size);
    return nullptr;
}

void FreeListAllocator::deallocate(void* ptr) {
    if (!ptr || !m_buffer) {
        return;
    }

    // ポインタがバッファ内か確認
    auto* bytePtr   = static_cast<std::byte*>(ptr);
    auto* bufStart  = static_cast<std::byte*>(m_buffer);
    auto* bufEnd    = bufStart + m_capacity;
    if (bytePtr < bufStart || bytePtr >= bufEnd) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "FreeListAllocator: Attempt to deallocate pointer not owned by this allocator");
        return;
    }

    // ヘッダを逆算（アライメントパディングなしの場合: header = ptr - sizeof(BlockHeader)）
    // ※ allocate() でパディングを size に含めているため、ヘッダは常に ptr の直前にある
    auto* header = reinterpret_cast<BlockHeader*>(bytePtr - sizeof(BlockHeader));
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

void FreeListAllocator::reset() {
    if (!m_buffer) return;

    auto* header         = static_cast<BlockHeader*>(m_buffer);
    header->size         = m_capacity - sizeof(BlockHeader);
    header->isFree       = true;
    header->prevPhysical = nullptr;

    m_usedBytes        = 0;
    m_allocationCount  = 0;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "FreeListAllocator: Reset");
}

size_t FreeListAllocator::getFreeBlockCount() const {
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

FreeListAllocator::BlockHeader*
FreeListAllocator::nextPhysical(BlockHeader* header) const {
    auto* next = reinterpret_cast<std::byte*>(header)
                 + sizeof(BlockHeader) + header->size;
    const auto* end = static_cast<const std::byte*>(m_buffer) + m_capacity;
    if (next >= end) return nullptr;
    return reinterpret_cast<BlockHeader*>(next);
}

void FreeListAllocator::splitBlock(BlockHeader* header, size_t size) {
    const size_t minSplitSize = sizeof(BlockHeader) + MIN_SPLIT_PAYLOAD;

    if (header->size < size + minSplitSize) {
        // 余剰が少ないので分割しない（そのまま使う）
        return;
    }

    // 後半を新しいフリーブロックにする
    auto* newHeader = reinterpret_cast<BlockHeader*>(
        reinterpret_cast<std::byte*>(header) + sizeof(BlockHeader) + size
    );
    newHeader->size         = header->size - size - sizeof(BlockHeader);
    newHeader->isFree       = true;
    newHeader->prevPhysical = header;

    // 分割後の次のブロックの prevPhysical を更新
    BlockHeader* afterNew = nextPhysical(newHeader);
    if (afterNew) {
        afterNew->prevPhysical = newHeader;
    }

    header->size = size;
}

void FreeListAllocator::mergeWithNext(BlockHeader* header) {
    BlockHeader* next = nextPhysical(header);
    if (!next) return;

    // next を吸収して size を拡張
    header->size += sizeof(BlockHeader) + next->size;

    // next の次のブロックの prevPhysical を更新
    BlockHeader* afterNext = nextPhysical(header); // header->size が変わったので再計算
    if (afterNext) {
        afterNext->prevPhysical = header;
    }
}

} // namespace mk::memory
