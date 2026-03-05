#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace mk::memory {

/// StackAllocator（スタック型アロケーター）
///
/// バンプアロケーターにマーカー機能を加えた、LIFO 単位の一括解放に対応するアロケーター。
///
/// - allocate(): O(1)
/// - deallocate(): 使用不可（assert で停止）。解放は pushMarker/popMarker または reset() を使うこと
/// - pushMarker() / popMarker() でネストした任意地点への一括巻き戻しが可能
/// - mark() / rewindTo() でオフセット値を直接扱う低レベル API も提供
///
/// 用途:
/// - 関数呼び出し単位の一時バッファ（AI 計算、経路探索など）
/// - ネストした一時割り当て
class StackAllocator {
public:
    /// マーカースタックの最大ネスト深度
    static constexpr size_t MAX_MARKER_DEPTH = 32;

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
    /// 解放には pushMarker()/popMarker() または reset() を使うこと。
    void deallocate(void* ptr);

    /// 全割り当てを無効化してオフセットを 0 に戻す
    void reset() { m_offset = 0; m_markerDepth = 0; }

    // ─── マーカー API（内部スタック管理） ───────────────────

    /// 現在のオフセットを内部スタックに保存する
    /// @note ネスト深度が MAX_MARKER_DEPTH を超えると失敗（エラーログ出力 + 無視）
    void pushMarker();

    /// 最後に pushMarker() した地点まで巻き戻す
    /// @note pushMarker() と対になって呼ぶこと。スタックが空の場合はエラー
    void popMarker();

    // ─── マーカー API（外部値管理・低レベル） ────────────────

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
    static std::uintptr_t alignForward(std::uintptr_t addr, size_t alignment);

    void*  m_buffer;
    size_t m_capacity;
    size_t m_offset;
    bool   m_ownsBuffer;

    /// 内部マーカースタック
    size_t m_markerStack[MAX_MARKER_DEPTH];
    size_t m_markerDepth = 0;
};

} // namespace mk::memory
