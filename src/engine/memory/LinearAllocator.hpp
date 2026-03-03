#pragma once
#include <cstddef>
#include <cstdint>

namespace mk::memory {

/// LinearAllocator（バンプアロケーター）
///
/// 高速な一時メモリ割り当て用アロケーター。
/// ポインタを進めるだけのO(1)割り当て。個別解放は不可、reset()で全体をリセット。
///
/// 用途:
/// - フレーム毎の一時データ
/// - シーン毎の一時データ
/// - スクラッチバッファ
class LinearAllocator {
public:
    /// コンストラクタ（内部 malloc 版）
    /// @param capacity 割り当て可能な総バイト数
    explicit LinearAllocator(size_t capacity);

    /// コンストラクタ（外部バッファ版）
    /// バッファの所有権は呼び出し側が持つ（デストラクタで free しない）
    /// @param buf  外部から提供されるバッファ
    /// @param capacity バッファのバイト数
    LinearAllocator(void* buf, size_t capacity);

    /// デストラクタ（メモリを解放）
    ~LinearAllocator();

    /// メモリを割り当てる
    /// @param size 割り当てるバイト数
    /// @param alignment アライメント（デフォルト: 16バイト）
    /// @return 割り当てられたメモリへのポインタ（失敗時はnullptr）
    void* allocate(size_t size, size_t alignment = 16);

    /// 個別解放は不可（no-op）
    /// Linear allocatorは個別解放をサポートしない
    void deallocate(void* ptr) { (void)ptr; }

    /// アロケーターを初期状態にリセット
    /// 全ての割り当てを無効化する
    void reset();

    /// 現在使用中のバイト数を取得
    size_t getUsedBytes() const { return m_offset; }

    /// 総容量を取得
    size_t getCapacity() const { return m_capacity; }

    /// 使用率を取得（0.0~1.0）
    float getUsageRatio() const {
        return m_capacity > 0 ? static_cast<float>(m_offset) / m_capacity : 0.0f;
    }

    // コピー禁止
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

private:
    /// アライメントを調整
    static size_t alignForward(size_t addr, size_t alignment);

    void*  m_buffer;   ///< 割り当てバッファ
    size_t m_capacity; ///< 総容量（バイト）
    size_t m_offset;   ///< 現在のオフセット（バイト）
    bool   m_ownsBuffer; ///< true のとき デストラクタで free する
};

} // namespace mk::memory
