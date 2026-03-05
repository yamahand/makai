#pragma once
#include "FreeListAllocator.hpp"
#include <cstddef>

namespace mk::memory {

/// PagedAllocator（ページング型線形アロケーター）
///
/// バッキングアロケーター（マスター FreeList 等）からページを動的に追加しながら
/// メモリを割り当てる。ページ内はバンプアロケーターと同じ O(1) 割り当て。
///
/// - 個別解放は不可（no-op）。reset() で全体をリセット。
/// - reset() は先頭ページを残して残りをバッキングに返却する（再確保コストを削減）。
/// - デストラクタで全ページをバッキングに返却する。
///
/// 用途:
/// - オブジェクト数が動的に変わるシーンデータ
/// - ロード時の一時バッファ
///
/// 使い方:
/// @code
///   PagedAllocator pa(1024 * 1024, MemoryManager::instance().heapAllocator());
///   void* ptr = pa.allocate(sizeof(Enemy));
///   // ... フレームが終わったら
///   pa.reset();
/// @endcode
class PagedAllocator {
public:
    /// コンストラクタ
    /// @param pageSize  1 ページのバイト数（ヘッダ込み）
    /// @param backing   ページ取得元のアロケーター（通常はマスター FreeList）
    PagedAllocator(size_t pageSize, FirstFitAllocator& backing);

    ~PagedAllocator();

    /// メモリを割り当てる
    /// 現在のページに収まらない場合は新しいページをバッキングから取得する。
    /// @return 割り当てられたメモリへのポインタ（失敗時は nullptr）
    void* allocate(size_t size, size_t alignment = 16);

    /// 個別解放は不可（no-op）
    void deallocate(void* ptr) { (void)ptr; }

    /// リセット：先頭ページ以外をバッキングに返却し、先頭ページのオフセットを 0 に戻す
    void reset();

    /// 現在のページ数
    size_t getPageCount()   const { return m_pageCount; }

    /// 1 ページのバイト数（ヘッダ込み）
    size_t getPageSize()    const { return m_pageSize; }

    /// 割り当て済み合計バイト数（パディング含む）
    size_t getUsedBytes()   const { return m_usedBytes; }

    /// 全ページの使用可能容量合計（ヘッダを除く）
    size_t getTotalCapacity() const;

    /// 使用率（0.0 〜 1.0）
    float  getUsageRatio()  const;

    // コピー禁止
    PagedAllocator(const PagedAllocator&)            = delete;
    PagedAllocator& operator=(const PagedAllocator&) = delete;

private:
    /// 各ページバッファの先頭に埋め込まれるヘッダ
    struct PageHeader {
        PageHeader* next;     ///< 次ページへのリンクリスト
        size_t      capacity; ///< このページの使用可能バイト数（ヘッダを除く）
        size_t      offset;   ///< 現在のオフセット（バイト）
    };

    /// ページヘッダのサイズ（アライメント込み）
    static constexpr size_t kHeaderSize =
        (sizeof(PageHeader) + alignof(std::max_align_t) - 1)
        & ~(alignof(std::max_align_t) - 1);

    /// ページヘッダの直後（ペイロード先頭アドレス）を返す
    static std::byte* pagePayload(PageHeader* header) {
        return reinterpret_cast<std::byte*>(header) + kHeaderSize;
    }

    /// バッキングから新しいページを確保してリンクリストに追加する
    bool addPage();

    FirstFitAllocator& m_backing;   ///< ページ取得元アロケーター
    size_t             m_pageSize;  ///< 1 ページのバイト数（ヘッダ込み）
    PageHeader*        m_head;      ///< リンクリストの先頭（最古ページ）
    PageHeader*        m_current;   ///< 現在書き込み中のページ
    size_t             m_pageCount; ///< 確保済みページ数
    size_t             m_usedBytes; ///< 割り当て済み合計バイト数
};

} // namespace mk::memory
