#pragma once
#include "FreeListAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <SDL3/SDL_log.h>
#include <typeinfo>

namespace mk::memory {

/// PoolAllocator - 固定サイズオブジェクトプール
///
/// 同じサイズのオブジェクトを高速に割り当て/解放するアロケーター。
/// フリーリストで管理し、O(1)の割り当て/解放を実現。
///
/// ブロック配列は外部バッファ（マスター FreeList 等）または内部 malloc で確保できる。
/// 外部バッファ版では、デストラクタ時にバッキングアロケーターへ自動返却する。
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
    /// プールブロック（フリーリストノード）
    /// getPool<T>() がマスター FreeList から確保するために public に公開する
    struct Block {
        alignas(T) std::byte storage[sizeof(T)];  ///< オブジェクト格納領域
        Block* next;  ///< 次の空きブロック
    };

    /// コンストラクタ（内部 malloc 版）
    /// ブロック配列を内部で malloc して確保する
    PoolAllocator();

    /// コンストラクタ（外部バッファ版）
    /// ブロック配列の所有権はバッキングアロケーターが持つ。
    /// デストラクタで backing.deallocate(blocks) を呼ぶ。
    /// @param blocks  事前確保済みのブロック配列（PoolSize 個以上）
    /// @param backing ブロック配列の取得元アロケーター（返却先）
    PoolAllocator(Block* blocks, FirstFitAllocator& backing);

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
    /// フリーリストを初期化する共通処理
    void initFreeList();

    /// ブロックがこのプールに属するか確認
    bool ownsBlock(const T* ptr) const;

    Block*             m_blocks;      ///< ブロック配列（内部 malloc または外部バッファ）
    Block*             m_freeList;    ///< フリーリストの先頭
    size_t             m_usedCount;   ///< 使用中のブロック数
    bool               m_ownsBuffer;  ///< true のとき デストラクタで free する
    FirstFitAllocator* m_backing;     ///< 外部バッファ版の返却先（nullptr = 内部 malloc）
};

// ========================================
// テンプレート実装
// ========================================

template<typename T, size_t PoolSize>
void PoolAllocator<T, PoolSize>::initFreeList() {
    // ブロック配列全体をフリーリストとして繋ぐ
    m_freeList = nullptr;
    for (size_t i = 0; i < PoolSize; ++i) {
        m_blocks[i].next = m_freeList;
        m_freeList = &m_blocks[i];
    }
}

template<typename T, size_t PoolSize>
PoolAllocator<T, PoolSize>::PoolAllocator()
    : m_freeList(nullptr)
    , m_usedCount(0)
    , m_ownsBuffer(true)
    , m_backing(nullptr)
{
    // ブロック配列を内部 malloc で確保する
    m_blocks = static_cast<Block*>(std::malloc(sizeof(Block) * PoolSize));
    initFreeList();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
               "PoolAllocator<%s>: 内部バッファで初期化 (%zu 個, %.2f KB)",
               typeid(T).name(), PoolSize, (sizeof(Block) * PoolSize) / 1024.0);
}

template<typename T, size_t PoolSize>
PoolAllocator<T, PoolSize>::PoolAllocator(Block* blocks, FirstFitAllocator& backing)
    : m_blocks(blocks)
    , m_freeList(nullptr)
    , m_usedCount(0)
    , m_ownsBuffer(false)
    , m_backing(&backing)
{
    initFreeList();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
               "PoolAllocator<%s>: 外部バッファで初期化 (%zu 個, %.2f KB)",
               typeid(T).name(), PoolSize, (sizeof(Block) * PoolSize) / 1024.0);
}

template<typename T, size_t PoolSize>
PoolAllocator<T, PoolSize>::~PoolAllocator() {
    if (m_usedCount > 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                   "PoolAllocator<%s>: %zu 個のブロックが未解放のまま破棄されました",
                   typeid(T).name(), m_usedCount);
    }

    // ブロック配列をバッキングアロケーターに返却する
    if (m_ownsBuffer) {
        std::free(m_blocks);
    } else if (m_backing) {
        m_backing->deallocate(m_blocks);
    }
    m_blocks = nullptr;
}

template<typename T, size_t PoolSize>
T* PoolAllocator<T, PoolSize>::allocate() {
    if (!m_freeList) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "PoolAllocator<%s>: プール枯渇 (%zu/%zu)",
                    typeid(T).name(), m_usedCount, PoolSize);
        return nullptr;
    }

    // フリーリストから1ブロック取得
    Block* block = m_freeList;
    m_freeList = block->next;
    m_usedCount++;

    return reinterpret_cast<T*>(block->storage);
}

template<typename T, size_t PoolSize>
void PoolAllocator<T, PoolSize>::deallocate(T* ptr) {
    if (!ptr) return;

    if (!ownsBlock(ptr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "PoolAllocator<%s>: このプールに属さないポインタの解放を試みました",
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
    initFreeList();
    m_usedCount = 0;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
               "PoolAllocator<%s>: リセット完了", typeid(T).name());
}

template<typename T, size_t PoolSize>
bool PoolAllocator<T, PoolSize>::ownsBlock(const T* ptr) const {
    const std::byte* bytePtr   = reinterpret_cast<const std::byte*>(ptr);
    const std::byte* poolStart = reinterpret_cast<const std::byte*>(m_blocks);
    const std::byte* poolEnd   = poolStart + (sizeof(Block) * PoolSize);

    return bytePtr >= poolStart && bytePtr < poolEnd;
}

} // namespace mk::memory
