#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <SDL3/SDL_log.h>

namespace makai::memory {

/// PoolAllocator - 固定サイズオブジェクトプール
///
/// 同じサイズのオブジェクトを高速に割り当て/解放するアロケーター。
/// フリーリストで管理し、O(1)の割り当て/解放を実現。
///
/// 用途:
/// - GameObject派生クラス（Player, Enemy等）
/// - UI要素
/// - パーティクル
///
/// @tparam T プールするオブジェクトの型
/// @tparam PoolSize プールの最大サイズ（デフォルト: 256）
template<typename T, size_t PoolSize = 256>
class PoolAllocator {
public:
    PoolAllocator();
    ~PoolAllocator();

    /// オブジェクトを割り当てる（コンストラクタは呼ばない）
    /// @return 割り当てられたメモリへのポインタ（失敗時はnullptr）
    T* allocate();

    /// オブジェクトを解放する（デストラクタは呼ばない）
    /// @param ptr 解放するメモリへのポインタ
    void deallocate(T* ptr);

    /// プールをリセット（全てのブロックを解放）
    /// 注意: デストラクタは呼ばれないので、使用前に手動で呼ぶこと
    void reset();

    /// 使用中のブロック数を取得
    size_t getUsedCount() const { return m_usedCount; }

    /// 総容量を取得
    constexpr size_t getCapacity() const { return PoolSize; }

    /// 空きブロック数を取得
    size_t getFreeCount() const { return PoolSize - m_usedCount; }

    /// 使用率を取得（0.0~1.0）
    float getUsageRatio() const {
        return static_cast<float>(m_usedCount) / PoolSize;
    }

    /// プールが満杯か確認
    bool isFull() const { return m_usedCount >= PoolSize; }

    /// プールが空か確認
    bool isEmpty() const { return m_usedCount == 0; }

    // コピー禁止
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

private:
    /// プールブロック（フリーリストノード）
    struct Block {
        alignas(T) std::byte storage[sizeof(T)];  ///< オブジェクト格納領域
        Block* next;  ///< 次の空きブロック
    };

    std::array<Block, PoolSize> m_blocks;  ///< ブロック配列
    Block* m_freeList;  ///< フリーリストの先頭
    size_t m_usedCount; ///< 使用中のブロック数

    /// ブロックがこのプールに属するか確認
    bool ownsBlock(const T* ptr) const;
};

// ========================================
// テンプレート実装
// ========================================

template<typename T, size_t PoolSize>
PoolAllocator<T, PoolSize>::PoolAllocator()
    : m_freeList(nullptr)
    , m_usedCount(0)
{
    // フリーリストを初期化
    for (size_t i = 0; i < PoolSize; ++i) {
        m_blocks[i].next = m_freeList;
        m_freeList = &m_blocks[i];
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
               "PoolAllocator<%s>: Initialized with %zu blocks (%.2f KB total)",
               typeid(T).name(), PoolSize, (sizeof(Block) * PoolSize) / 1024.0);
}

template<typename T, size_t PoolSize>
PoolAllocator<T, PoolSize>::~PoolAllocator() {
    if (m_usedCount > 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                   "PoolAllocator<%s>: Destroyed with %zu blocks still in use (memory leak?)",
                   typeid(T).name(), m_usedCount);
    }
}

template<typename T, size_t PoolSize>
T* PoolAllocator<T, PoolSize>::allocate() {
    if (!m_freeList) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "PoolAllocator<%s>: Out of memory (pool exhausted: %zu/%zu)",
                    typeid(T).name(), m_usedCount, PoolSize);
        return nullptr;
    }

    // フリーリストから1ブロック取得
    Block* block = m_freeList;
    m_freeList = block->next;
    m_usedCount++;

    // ストレージをTポインタとして返す
    return reinterpret_cast<T*>(block->storage);
}

template<typename T, size_t PoolSize>
void PoolAllocator<T, PoolSize>::deallocate(T* ptr) {
    if (!ptr) {
        return;
    }

    // このプールに属するブロックか確認
    if (!ownsBlock(ptr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "PoolAllocator<%s>: Attempt to deallocate pointer not owned by this pool",
                    typeid(T).name());
        return;
    }

    // ブロックをフリーリストに戻す
    Block* block = reinterpret_cast<Block*>(
        reinterpret_cast<std::byte*>(ptr) - offsetof(Block, storage)
    );
    block->next = m_freeList;
    m_freeList = block;
    m_usedCount--;
}

template<typename T, size_t PoolSize>
void PoolAllocator<T, PoolSize>::reset() {
    // フリーリストを再初期化
    m_freeList = nullptr;
    for (size_t i = 0; i < PoolSize; ++i) {
        m_blocks[i].next = m_freeList;
        m_freeList = &m_blocks[i];
    }
    m_usedCount = 0;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
               "PoolAllocator<%s>: Reset (all blocks freed)",
               typeid(T).name());
}

template<typename T, size_t PoolSize>
bool PoolAllocator<T, PoolSize>::ownsBlock(const T* ptr) const {
    const std::byte* bytePtr = reinterpret_cast<const std::byte*>(ptr);
    const std::byte* poolStart = reinterpret_cast<const std::byte*>(m_blocks.data());
    const std::byte* poolEnd = poolStart + (sizeof(Block) * PoolSize);

    return bytePtr >= poolStart && bytePtr < poolEnd;
}

} // namespace makai::memory
