#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace mk::memory {

/// StackAllocator（スタック型アロケーター）
///
/// 純粋なバンプアロケーター。マーカーを使った LIFO 一括解放をサポートする。
///
/// - allocate(): O(1)
/// - deallocate(): 使用不可（assert で停止）。解放は StackAllocatorMarker または reset() を使うこと
/// - mark() / rewindTo() で任意地点への一括巻き戻しが可能
///
/// ## 推奨パターン（RAII スコープガード）
/// ```cpp
/// {
///     StackAllocatorMarker marker(stackAllocator);  // 現在地を保存
///     void* p = stackAllocator.allocate(256);
///     // ... 処理 ...
/// }  // スコープ終了で自動 rewind
/// ```
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
    /// @param size      割り当てるバイト数
    /// @param alignment アライメント（デフォルト: 16バイト、2のべき乗必須）
    /// @return 割り当てられたメモリへのポインタ（失敗時は nullptr）
    void* allocate(size_t size, size_t alignment = 16);

    /// 個別解放は非サポート。呼び出すと assert で停止する。
    /// 解放には StackAllocatorMarker または reset() を使うこと。
    void deallocate(void* ptr);

    /// 全割り当てを無効化してオフセットを 0 に戻す
    void reset() { m_offset = 0; }

    // ─── マーカー API ──────────────────────────────────────

    /// 現在のオフセットをマーカー値として返す
    size_t mark() const { return m_offset; }

    /// 指定したマーカー値まで一括巻き戻す（mark() 以降の割り当てをすべて無効化）
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
    void*  m_buffer;
    size_t m_capacity;
    size_t m_offset;
    bool   m_ownsBuffer;
};

/// StackAllocator の RAII スコープガード
///
/// コンストラクタで現在地（mark）を保存し、デストラクタで自動的に rewind する。
/// スコープを抜けると（例外時も含め）確保した領域が一括解放される。
///
/// ```cpp
/// {
///     StackAllocatorMarker marker(allocator);
///     void* p = allocator.allocate(1024);
/// }  // ← ここで自動 rewind
/// ```
class StackAllocatorMarker {
public:
    explicit StackAllocatorMarker(StackAllocator& allocator)
        : m_allocator(allocator), m_marker(allocator.mark()) {}

    ~StackAllocatorMarker() { m_allocator.rewindTo(m_marker); }

    // コピー・ムーブ禁止（二重 rewind 防止）
    StackAllocatorMarker(const StackAllocatorMarker&) = delete;
    StackAllocatorMarker& operator=(const StackAllocatorMarker&) = delete;

private:
    StackAllocator& m_allocator;
    size_t          m_marker;
};

} // namespace mk::memory
