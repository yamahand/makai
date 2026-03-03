#include "BuddyAllocator.hpp"
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL_log.h>

namespace mk::memory {

// ─────────────────────────────────────────────
// ヘルパー
// ─────────────────────────────────────────────

/// size 以上の最小の 2 の累乗を返す
static size_t floorPow2(size_t v) {
    if (v == 0) return 0;
    size_t result = 1;
    while (result * 2 <= v) result *= 2;
    return result;
}

size_t BuddyAllocator::ceilOrder(size_t totalSize) {
    size_t order = BuddyAllocator::MIN_ORDER;
    while ((size_t(1) << order) < totalSize && order <= BuddyAllocator::MAX_ORDER) {
        ++order;
    }
    return order;
}

// ─────────────────────────────────────────────
// コンストラクタ / デストラクタ
// ─────────────────────────────────────────────

BuddyAllocator::BuddyAllocator(size_t size)
    : m_buffer(nullptr)
    , m_capacity(floorPow2(size))
    , m_order(0)
    , m_usedBytes(0)
    , m_ownsBuffer(true)
{
    m_buffer = std::malloc(m_capacity);
    initBuffer();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "BuddyAllocator: 内部バッファで初期化 (%zu KB, order %zu)",
                m_capacity / 1024, m_order);
}

BuddyAllocator::BuddyAllocator(void* buf, size_t size)
    : m_buffer(buf)
    , m_capacity(floorPow2(size))
    , m_order(0)
    , m_usedBytes(0)
    , m_ownsBuffer(false)
{
    initBuffer();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "BuddyAllocator: 外部バッファで初期化 (%zu KB, order %zu)",
                m_capacity / 1024, m_order);
}

BuddyAllocator::~BuddyAllocator() {
    if (m_ownsBuffer) {
        std::free(m_buffer);
    }
}

void BuddyAllocator::initBuffer() {
    // フリーリストをクリアする
    std::memset(m_freeLists, 0, sizeof(m_freeLists));

    if (!m_buffer || m_capacity < blockSize(MIN_ORDER)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "BuddyAllocator: バッファサイズが小さすぎます (%zu B)", m_capacity);
        return;
    }

    // m_capacity = 2^m_order を求める
    m_order = MIN_ORDER;
    while (blockSize(m_order + 1) <= m_capacity && m_order < MAX_ORDER) {
        ++m_order;
    }

    // 実際にバディシステムが管理する容量はルートブロックのサイズに制限する
    // （m_capacity > 2^m_order の場合、超過分にバディヘッダを書いてしまうのを防ぐ）
    m_capacity = blockSize(m_order);

    // バッファ全体を 1 つのフリーブロック（最大オーダー）として登録する
    auto* header   = reinterpret_cast<BlockHeader*>(m_buffer);
    header->order  = static_cast<uint32_t>(m_order);
    header->isFree = 1;
    header->_pad   = 0;

    pushFree(m_buffer, m_order);
}

// ─────────────────────────────────────────────
// フリーリスト操作
// ─────────────────────────────────────────────

void BuddyAllocator::pushFree(void* blockPtr, size_t order) {
    size_t idx   = order - MIN_ORDER;
    auto*  node  = reinterpret_cast<FreeNode*>(static_cast<std::byte*>(blockPtr) + HEADER_SIZE);
    node->prev   = nullptr;
    node->next   = m_freeLists[idx];
    if (m_freeLists[idx]) m_freeLists[idx]->prev = node;
    m_freeLists[idx] = node;
}

void* BuddyAllocator::popFree(size_t order) {
    size_t idx = order - MIN_ORDER;
    if (!m_freeLists[idx]) return nullptr;

    FreeNode* node = m_freeLists[idx];
    m_freeLists[idx] = node->next;
    if (m_freeLists[idx]) m_freeLists[idx]->prev = nullptr;

    // ノードの前の HEADER_SIZE バイトがブロック先頭
    return reinterpret_cast<std::byte*>(node) - HEADER_SIZE;
}

void BuddyAllocator::removeFree(void* blockPtr, size_t order) {
    size_t idx  = order - MIN_ORDER;
    auto*  node = reinterpret_cast<FreeNode*>(static_cast<std::byte*>(blockPtr) + HEADER_SIZE);

    if (node->prev) node->prev->next = node->next;
    else            m_freeLists[idx] = node->next;

    if (node->next) node->next->prev = node->prev;
}

// ─────────────────────────────────────────────
// allocate / deallocate / reset
// ─────────────────────────────────────────────

void* BuddyAllocator::allocate(size_t size, size_t /*alignment*/) {
    if (size == 0) return nullptr;

    size_t totalSize  = HEADER_SIZE + size;
    size_t order      = ceilOrder(totalSize);
    if (order < MIN_ORDER) order = MIN_ORDER;

    if (order > m_order) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "BuddyAllocator: 要求サイズ(%zu B)がバッファ容量を超えています", size);
        return nullptr;
    }

    // 要求オーダー以上の最小空きブロックを探す
    size_t k = order;
    while (k <= m_order && !m_freeLists[k - MIN_ORDER]) ++k;

    if (k > m_order) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "BuddyAllocator: メモリ不足 (要求オーダー: %zu)", order);
        return nullptr;
    }

    // ブロックをフリーリストから取り出す
    void* blockPtr = popFree(k);

    // 目的オーダーになるまでブロックを半分に分割する
    while (k > order) {
        --k;
        // 後半をバディとして空きリストに追加する
        void* buddyPtr  = bufAt(offsetOf(blockPtr) + blockSize(k));
        auto* buddyHdr  = reinterpret_cast<BlockHeader*>(buddyPtr);
        buddyHdr->order = static_cast<uint32_t>(k);
        buddyHdr->isFree = 1;
        buddyHdr->_pad   = 0;
        pushFree(buddyPtr, k);
    }

    // ヘッダを使用中に設定する
    auto* header    = reinterpret_cast<BlockHeader*>(blockPtr);
    header->order   = static_cast<uint32_t>(order);
    header->isFree  = 0;

    m_usedBytes += blockSize(order);

    return static_cast<std::byte*>(blockPtr) + HEADER_SIZE;
}

void BuddyAllocator::deallocate(void* ptr) {
    if (!ptr) return;

    void* blockPtr = static_cast<std::byte*>(ptr) - HEADER_SIZE;
    auto* header   = reinterpret_cast<BlockHeader*>(blockPtr);
    size_t order   = header->order;

    header->isFree = 1;
    m_usedBytes   -= blockSize(order);

    // バディとマージできる限り繰り返す
    while (order < m_order) {
        size_t blockOff  = offsetOf(blockPtr);
        size_t buddyOff  = buddyOffset(blockOff, order);

        // バディがバッファ外ならマージ不可
        if (buddyOff + blockSize(order) > m_capacity) break;

        auto* buddyHdr = reinterpret_cast<BlockHeader*>(bufAt(buddyOff));

        // バディが同オーダーかつ空きでなければマージ不可
        if (buddyHdr->order != order || !buddyHdr->isFree) break;

        // バディをフリーリストから除去してマージする
        removeFree(bufAt(buddyOff), order);

        // アドレスの小さい方をマージ後ブロックとして使う
        if (buddyOff < blockOff) blockPtr = bufAt(buddyOff);

        ++order;
        reinterpret_cast<BlockHeader*>(blockPtr)->order = static_cast<uint32_t>(order);
    }

    reinterpret_cast<BlockHeader*>(blockPtr)->isFree = 1;
    pushFree(blockPtr, order);
}

void BuddyAllocator::reset() {
    std::memset(m_freeLists, 0, sizeof(m_freeLists));
    m_usedBytes = 0;

    auto* header   = reinterpret_cast<BlockHeader*>(m_buffer);
    header->order  = static_cast<uint32_t>(m_order);
    header->isFree = 1;
    header->_pad   = 0;

    pushFree(m_buffer, m_order);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "BuddyAllocator: リセット完了");
}

} // namespace mk::memory
