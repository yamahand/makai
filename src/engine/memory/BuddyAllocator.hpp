#pragma once
#include <cstddef>
#include <cstdint>

namespace mk::memory {

/// BuddyAllocator（バディアロケーター）
///
/// メモリを 2 の累乗サイズのブロックに分割・結合して管理する。
/// 解放時に隣接するバディブロックが空であれば自動的にマージする。
///
/// - allocate(): O(log N)
/// - deallocate(): O(log N)（バディとのマージ判定込み）
/// - 内部断片化あり（要求サイズを 2 の累乗に切り上げる）
/// - 外部断片化が少ない（バディペアのマージが高速）
///
/// バッファサイズは必ず 2 の累乗であること。
/// 2 の累乗でない場合はコンストラクタ内で切り捨てる。
///
/// 主な用途:
/// - テクスチャ・オーディオなど大きめのリソース管理
/// - 一定サイズで分割・返却するメモリプール
class BuddyAllocator {
public:
    /// 最小ブロックサイズ = 2^MIN_ORDER バイト（64B）
    static constexpr size_t MIN_ORDER = 6;
    /// 対応できる最大バッファサイズ = 2^MAX_ORDER バイト（1GB）
    static constexpr size_t MAX_ORDER = 30;
    /// オーダー数（フリーリスト配列のサイズ）
    static constexpr size_t NUM_ORDERS = MAX_ORDER - MIN_ORDER + 1;
    /// ブロックヘッダのバイト数（16B、ペイロードは 16B アライン）
    static constexpr size_t HEADER_SIZE = 16;

    /// コンストラクタ（内部 malloc 版）
    /// @param size バッファサイズ（2 の累乗に切り捨て）
    explicit BuddyAllocator(size_t size);

    /// コンストラクタ（外部バッファ版）
    /// @param buf  外部バッファ（呼び出し側が所有権を持つ）
    /// @param size バッファサイズ（2 の累乗に切り捨て）
    BuddyAllocator(void* buf, size_t size);

    ~BuddyAllocator();

    /// メモリを割り当てる
    /// 要求サイズ + ヘッダを収容できる最小の 2 の累乗ブロックを割り当てる
    /// @return 割り当てられたポインタ（失敗時は nullptr）
    void* allocate(size_t size, size_t alignment = 16);

    /// メモリを解放し、バディブロックと可能な限りマージする
    void deallocate(void* ptr);

    /// 全ブロックをリセットして初期状態に戻す
    void reset();

    size_t getUsedBytes()  const { return m_usedBytes; }
    size_t getCapacity()   const { return m_capacity; }
    float  getUsageRatio() const {
        return m_capacity > 0 ? static_cast<float>(m_usedBytes) / m_capacity : 0.0f;
    }

    // コピー禁止
    BuddyAllocator(const BuddyAllocator&) = delete;
    BuddyAllocator& operator=(const BuddyAllocator&) = delete;

private:
    /// ブロック先頭に埋め込まれるヘッダ（16B）
    struct BlockHeader {
        uint32_t order;    ///< log2(ブロックサイズ)
        uint32_t isFree;   ///< 空きブロックか
        uint64_t _pad;     ///< 16B に揃えるためのパディング
    };
    static_assert(sizeof(BlockHeader) == HEADER_SIZE, "BlockHeader must be 16 bytes");

    /// 空きブロック内（ヘッダ直後）に存在するフリーリストノード
    struct FreeNode {
        FreeNode* prev;
        FreeNode* next;
    };

    /// 指定サイズを収容できる最小オーダーを返す
    static size_t ceilOrder(size_t totalSize);

    /// オーダー k のブロックサイズ（バイト）
    static size_t blockSize(size_t order) { return size_t(1) << order; }

    /// バッファ先頭からのオフセットでアドレスを計算する
    std::byte* bufAt(size_t offset) const {
        return static_cast<std::byte*>(m_buffer) + offset;
    }

    /// ポインタのバッファ先頭からのオフセットを返す
    size_t offsetOf(void* ptr) const {
        return static_cast<std::byte*>(ptr) - static_cast<std::byte*>(m_buffer);
    }

    /// バディのオフセットを XOR で計算する
    size_t buddyOffset(size_t offset, size_t order) const {
        return offset ^ blockSize(order);
    }

    /// フリーリストにブロックを追加する
    void pushFree(void* blockPtr, size_t order);

    /// フリーリストの先頭からブロックを取り出す
    void* popFree(size_t order);

    /// フリーリストから特定のブロックを削除する
    void removeFree(void* blockPtr, size_t order);

    /// バッファを初期化してルートブロックを 1 つ作る
    void initBuffer();

    void*  m_buffer;
    size_t m_capacity;    ///< 実際に使用するバッファサイズ（2 の累乗）
    size_t m_order;       ///< log2(m_capacity)
    size_t m_usedBytes;
    bool   m_ownsBuffer;

    /// オーダーごとの空きブロックリスト（インデックス = order - MIN_ORDER）
    FreeNode* m_freeLists[NUM_ORDERS];
};

} // namespace mk::memory
