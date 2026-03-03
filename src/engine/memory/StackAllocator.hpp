#pragma once
#include <cstddef>
#include <cstdlib>

namespace mk::memory {

/// StackAllocator（スタック型アロケーター）
///
/// LinearAllocator の変形で、LIFO 順の個別解放に対応する。
/// 各割り当ての直前に prevOffset を埋め込み、deallocate() でその地点まで巻き戻す。
///
/// - allocate(): O(1)
/// - deallocate(): O(1)、ただし LIFO 順でのみ正しく動作する
/// - mark() / rewindTo() で任意の地点への一括巻き戻しも可能
///
/// 用途:
/// - 関数呼び出し単位の一時バッファ（AI 計算、経路探索など）
/// - ネストした一時割り当て
class StackAllocator {
public:
    /// コンストラクタ（内部 malloc 版）
    explicit StackAllocator(size_t capacity);

    /// コンストラクタ（外部バッファ版）
    /// バッファの所有権は呼び出し側が持つ（デストラクタで free しない）
    StackAllocator(void* buf, size_t capacity);

    ~StackAllocator();

    /// メモリを割り当てる
    /// 割り当て直前に prevOffset をペイロードの手前に埋め込む
    /// @param size      割り当てるバイト数
    /// @param alignment アライメント（デフォルト: 16バイト）
    /// @return 割り当てられたメモリへのポインタ（失敗時は nullptr）
    void* allocate(size_t size, size_t alignment = 16);

    /// 個別解放（LIFO 順でのみ正しく動作する）
    /// 最後に allocate() したポインタから順に解放すること
    void deallocate(void* ptr);

    /// 全割り当てを無効化してオフセットを 0 に戻す
    void reset() { m_offset = 0; }

    /// 現在のオフセットをマーカーとして返す
    size_t mark() const { return m_offset; }

    /// マーカーまで一括巻き戻し（mark() 以降の割り当てをすべて無効化）
    void rewindTo(size_t marker) {
        if (marker <= m_offset) m_offset = marker;
    }

    /// 現在使用中のバイト数を取得
    size_t getUsedBytes() const { return m_offset; }

    /// 総容量を取得
    size_t getCapacity() const { return m_capacity; }

    /// 使用率を取得（0.0〜1.0）
    float getUsageRatio() const {
        return m_capacity > 0 ? static_cast<float>(m_offset) / m_capacity : 0.0f;
    }

    // コピー禁止
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

private:
    static size_t alignForward(size_t addr, size_t alignment);

    void*  m_buffer;
    size_t m_capacity;
    size_t m_offset;
    bool   m_ownsBuffer;
};

} // namespace mk::memory
