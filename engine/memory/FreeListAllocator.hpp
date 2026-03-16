#pragma once
#include <cstddef>
#include <cstdint>

namespace mk::memory {

// ─────────────────────────────────────────────
// 共通ブロックヘッダ（ポリシー間で共有）
// ─────────────────────────────────────────────

/// 各ブロックの先頭に埋め込まれるヘッダ
struct FreeListBlockHeader {
    size_t              size;          ///< ペイロードサイズ（ヘッダ含まず）
    bool                isFree;
    FreeListBlockHeader* prevPhysical; ///< 前の物理ブロック（コアレス用）
};

/// findBlock の戻り値（ヘッダと実際に必要なサイズをまとめる）
struct FreeListSearchResult {
    FreeListBlockHeader* header  = nullptr;
    size_t               needed  = 0;  ///< アライメントパディング込みの確保サイズ
};

// ─────────────────────────────────────────────
// 検索ポリシー
// ─────────────────────────────────────────────

/// First-Fit ポリシー: 先頭から走査して最初に合うブロックを使用する
struct FirstFitPolicy {
    static FreeListSearchResult findBlock(FreeListBlockHeader* first,
                                          size_t size, size_t alignment,
                                          const std::byte* bufferEnd);
};

/// Best-Fit ポリシー: 全フリーブロックを走査し、最小の余剰で収まるブロックを使用する
struct BestFitPolicy {
    static FreeListSearchResult findBlock(FreeListBlockHeader* first,
                                          size_t size, size_t alignment,
                                          const std::byte* bufferEnd);
};

// ─────────────────────────────────────────────
// FreeListAllocator
// ─────────────────────────────────────────────

/// FreeListAllocator（ポリシーテンプレート版）
///
/// 可変サイズの割り当てと個別解放をサポートする汎用アロケーター。
/// SearchPolicy を切り替えることで First-Fit / Best-Fit を選択できる。
///
/// 解放時に隣接するフリーブロックを結合（コアレス）して断片化を軽減する。
///
/// 分割: 余剰が sizeof(FreeListBlockHeader) + 最小ペイロード以上なら二分割。
///
/// @tparam SearchPolicy  FirstFitPolicy（デフォルト）または BestFitPolicy
template<typename SearchPolicy = FirstFitPolicy>
class FreeListAllocator {
public:
    /// コンストラクタ（内部 malloc 版）
    /// @param capacity バッキングバッファのバイト数
    explicit FreeListAllocator(size_t capacity);

    /// コンストラクタ（外部バッファ版）
    /// バッファの所有権は呼び出し側が持つ（デストラクタで free しない）
    FreeListAllocator(void* buf, size_t capacity);
    ~FreeListAllocator();

    /// メモリを割り当てる
    void* allocate(size_t size, size_t alignment = 16);

    /// メモリを解放し、隣接フリーブロックと結合する
    void deallocate(void* ptr);

    /// 全ブロックをリセット（デストラクタは呼ばれない）
    void reset();

    size_t getUsedBytes()       const { return m_usedBytes; }
    size_t getCapacity()        const { return m_capacity; }
    float  getUsageRatio()      const {
        return m_capacity > 0 ? static_cast<float>(m_usedBytes) / m_capacity : 0.0f;
    }
    size_t getAllocationCount() const { return m_allocationCount; }
    size_t getFreeBlockCount()  const;

    FreeListAllocator(const FreeListAllocator&)            = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

private:
    using BlockHeader = FreeListBlockHeader;

    BlockHeader* nextPhysical(BlockHeader* header) const;
    void         splitBlock(BlockHeader* header, size_t size);
    void         mergeWithNext(BlockHeader* header);

    void*  m_buffer;
    size_t m_capacity;
    size_t m_usedBytes;
    size_t m_allocationCount;
    bool   m_ownsBuffer; ///< true のときデストラクタで free する
};

// ─────────────────────────────────────────────
// Convenience aliases
// ─────────────────────────────────────────────

using FirstFitAllocator = FreeListAllocator<FirstFitPolicy>;
using BestFitAllocator  = FreeListAllocator<BestFitPolicy>;

} // namespace mk::memory
