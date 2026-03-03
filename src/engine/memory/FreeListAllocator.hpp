#pragma once
#include <cstddef>
#include <cstdint>

namespace mk::memory {

/// FreeListAllocator（First-Fit フリーリストアロケーター）
///
/// 可変サイズの割り当てと個別解放をサポートする汎用アロケーター。
/// 解放時に隣接するフリーブロックを結合（コアレス）することで断片化を軽減する。
///
/// 方針:
/// - ブロックヘッダをペイロード直前に埋め込む
/// - First-Fit: 先頭から走査して最初に合うブロックを使用
/// - 分割: 余剰が sizeof(BlockHeader) + 最小ペイロード以上なら二分割
/// - コアレス: 解放時に前後の物理ブロックが free であれば結合
///
/// 用途:
/// - 動的かつ様々なサイズのオブジェクト群（非フレーム寿命）
/// - STL コンテナのバッキングアロケーターとして
class FreeListAllocator {
public:
    /// @param capacity バッキングバッファのバイト数
    explicit FreeListAllocator(size_t capacity);
    ~FreeListAllocator();

    /// メモリを割り当てる
    /// @param size  割り当てるバイト数
    /// @param alignment アライメント（デフォルト: 16バイト）
    /// @return 割り当てられたメモリ（失敗時は nullptr）
    void* allocate(size_t size, size_t alignment = 16);

    /// メモリを解放し、隣接フリーブロックと結合する
    void deallocate(void* ptr);

    /// 全ブロックをリセット（デストラクタは呼ばれない）
    void reset();

    /// 現在使用中のペイロードバイト数を取得
    size_t getUsedBytes()        const { return m_usedBytes; }

    /// バッキングバッファの総容量を取得
    size_t getCapacity()         const { return m_capacity; }

    /// 使用率を取得（0.0〜1.0）
    float  getUsageRatio()       const {
        return m_capacity > 0 ? static_cast<float>(m_usedBytes) / m_capacity : 0.0f;
    }

    /// アクティブな割り当て数を取得
    size_t getAllocationCount()  const { return m_allocationCount; }

    /// フリーブロック数を取得（デバッグ・断片化確認用）
    size_t getFreeBlockCount()   const;

    // コピー禁止
    FreeListAllocator(const FreeListAllocator&)            = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

private:
    /// 各ブロックの先頭に置かれるヘッダ
    struct BlockHeader {
        size_t       size;          ///< ペイロードサイズ（ヘッダ含まず）
        bool         isFree;
        BlockHeader* prevPhysical;  ///< 前の物理ブロック（コアレス用）
    };

    /// ブロックの次の物理ブロックを返す（範囲外なら nullptr）
    BlockHeader* nextPhysical(BlockHeader* header) const;

    /// 指定ブロックを分割し、余剰を新しいフリーブロックにする
    void splitBlock(BlockHeader* header, size_t size);

    /// header とその次の物理ブロックを結合する（両方 free のこと）
    void mergeWithNext(BlockHeader* header);

    void*  m_buffer;            ///< バッキングバッファ
    size_t m_capacity;          ///< バッファ総容量
    size_t m_usedBytes;         ///< 使用中ペイロードバイト数
    size_t m_allocationCount;   ///< アクティブ割り当て数
};

} // namespace mk::memory
