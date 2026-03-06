#pragma once
#include "LinearAllocator.hpp"
#include "StackAllocator.hpp"
#include "BuddyAllocator.hpp"
#include "PagedAllocator.hpp"
#include "DoubleFrameAllocator.hpp"
#include "FreeListMemoryResource.hpp"
#include "PoolAllocator.hpp"
#include "../core/Config.hpp"
#include <SDL3/SDL_log.h>
#include <memory_resource>
#include <new>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cstdlib>
#include <cassert>

namespace mk::memory {

/// MemoryManager - メモリアロケーターの中央管理
///
/// シングルトンパターンで、ゲーム全体のメモリ割り当てを管理する。
/// Game::Game() の先頭で init(config.memory) を呼ぶ必要がある。
///
/// 起動時に単一のマスターバッファを確保し、各アロケーターに分配する。
/// （マスターバッファの OS への malloc 呼び出しはこの 1 回のみであり、他に new や標準コンテナ等による動的確保が発生する可能性はある）
///
/// 使い方:
///   if (!MemoryManager::init(config.memory)) {
///       // 失敗時は起動を中止する
///   }
///   auto& mem = MemoryManager::instance();
///   void* ptr = mem.frameAllocator().allocate(1024);
class MemoryManager {
public:
    /// シングルトンインスタンスを取得（init() 呼び出し後のみ有効）
    static MemoryManager& instance();

    /// マスターバッファを確保してアロケーターを初期化する
    /// Game::Game() の先頭（他のどの初期化よりも前）に 1 度だけ呼ぶこと
    /// @return 成功時 true。失敗時 false（呼び出し側は起動を中止すること）
    static bool init(const mk::MemoryConfig& config);

    /// フレームアロケーターを取得
    /// 毎フレーム終了時にリセットされる
    LinearAllocator& frameAllocator() {
        assert(m_frameAllocator && "MemoryManager::init() が呼ばれていません");
        return *m_frameAllocator;
    }

    /// ダブルフレームアロケーターを取得（現フレームの割り当て先）
    /// 前フレームのデータに 1 フレーム間アクセスしたい場合に使う
    LinearAllocator& doubleFrameAllocator() {
        assert(m_doubleFrameAllocator && "MemoryManager::init() が呼ばれていません");
        return m_doubleFrameAllocator->current();
    }

    /// ダブルフレームアロケーターの前フレームバッファを取得（読み取り専用）
    const LinearAllocator& previousFrameAllocator() {
        assert(m_doubleFrameAllocator && "MemoryManager::init() が呼ばれていません");
        return m_doubleFrameAllocator->previous();
    }

    /// ダブルフレームアロケーターを手動で swap する（テスト・デバッグ用）
    /// 通常は onFrameEnd() が毎フレーム自動的に呼ぶため、ゲームコードから直接呼ぶ必要はない
    void swapDoubleFrameAllocator() {
        assert(m_doubleFrameAllocator && "MemoryManager::init() が呼ばれていません");
        m_doubleFrameAllocator->swap();
    }

    /// シーンアロケーターを取得
    /// シーン変更時にリセットされる
    LinearAllocator& sceneAllocator() {
        assert(m_sceneAllocator && "MemoryManager::init() が呼ばれていません");
        return *m_sceneAllocator;
    }

    /// ヒープアロケーターを取得
    /// 可変サイズ・個別解放に対応した汎用アロケーター（マスター FreeList の残余領域）
    FirstFitAllocator& heapAllocator() {
        assert(m_masterResource && "MemoryManager::init() が呼ばれていません");
        return m_masterResource->getAllocator();
    }

    /// ヒープの pmr リソースを取得
    /// std::pmr コンテナへそのまま渡せる
    std::pmr::memory_resource* heapMemoryResource() {
        assert(m_masterResource && "MemoryManager::init() が呼ばれていません");
        return m_masterResource.get();
    }

    /// スタックアロケーターを取得
    /// マーカーによる LIFO 一括解放をサポートする一時バッファ
    StackAllocator& stackAllocator() {
        assert(m_stackAllocator && "MemoryManager::init() が呼ばれていません");
        return *m_stackAllocator;
    }

    /// バディアロケーターを取得
    /// 2の累乗サイズのブロックを O(log N) で割り当て・解放する
    BuddyAllocator& buddyAllocator() {
        assert(m_buddyAllocator && "MemoryManager::init() が呼ばれていません");
        return *m_buddyAllocator;
    }

    /// ページドアロケーターを取得
    /// バッキングアロケーターからページを動的に追加する線形アロケーター
    PagedAllocator& pagedAllocator() {
        assert(m_pagedAllocator && "MemoryManager::init() が呼ばれていません");
        return *m_pagedAllocator;
    }

    /// 型Tのプールアロケーターを取得
    /// 初回アクセス時に自動的に生成される
    template<typename T, size_t PoolSize = 256>
    PoolAllocator<T, PoolSize>& getPool();

    /// フレーム終了時に呼ぶ（Game::render()から）
    void onFrameEnd();

    /// シーン変更時に呼ぶ（SceneManager::applyPendingChanges()から）
    void onSceneChange();

    /// メモリ統計情報
    struct Stats {
        // フレームアロケーター
        size_t frameBytes;
        size_t frameCapacity;
        float  frameUsageRatio;

        // ダブルフレームアロケーター（現フロント）
        size_t doubleFrameCurrentBytes;
        size_t doubleFrameCapacity;
        float  doubleFrameCurrentUsageRatio;

        // シーンアロケーター
        size_t sceneBytes;
        size_t sceneCapacity;
        float  sceneUsageRatio;

        // ヒープ（FreeList）
        size_t heapBytes;
        size_t heapCapacity;     ///< サブアロケーター予約分を除いた実効容量
        float  heapUsageRatio;
        size_t heapAllocationCount;
        size_t heapFreeBlockCount;
        size_t masterTotalBytes;   ///< マスターバッファ全体の使用量（デバッグ用）
        size_t masterTotalCapacity;

        // スタックアロケーター
        size_t stackBytes;
        size_t stackCapacity;
        float  stackUsageRatio;

        // バディアロケーター
        size_t buddyBytes;
        size_t buddyCapacity;
        float  buddyUsageRatio;

        // ページドアロケーター
        size_t pagedBytes;
        size_t pagedTotalCapacity;
        size_t pagedPageCount;
        float  pagedUsageRatio;

        // 総割り当て回数（デバッグ用）
        size_t totalFrameAllocations;
        size_t totalSceneAllocations;
    };

    /// 統計情報を取得（ImGuiデバッグ表示用）
    Stats getStats() const;

    // コピー禁止
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

private:
    MemoryManager() = default;
    ~MemoryManager();

    // 型消去されたプールの基底クラス
    struct IPoolBase {
        virtual ~IPoolBase() = default;
    };

    // 型付きプールラッパー
    template<typename T, size_t PoolSize>
    struct PoolHolder : IPoolBase {
        PoolAllocator<T, PoolSize> pool;

        /// 外部バッファ版（マスター FreeList からブロック配列を受け取る）
        PoolHolder(typename PoolAllocator<T, PoolSize>::Block* blocks, FirstFitAllocator& backing)
            : pool(blocks, backing) {}
    };

    // マスターバッファ（起動時に 1 度だけ malloc する）
    void*  m_masterBuffer = nullptr;
    size_t m_masterSize   = 0;

    /// マスター FreeList（バッファ全体を管理し、各アロケーターにバッファを払い出す）
    std::unique_ptr<FirstFitMemoryResource> m_masterResource;

    // 各アロケーター（マスター FreeList からバッファを借りる）
    std::unique_ptr<LinearAllocator>      m_frameAllocator;
    std::unique_ptr<DoubleFrameAllocator> m_doubleFrameAllocator;
    std::unique_ptr<LinearAllocator>      m_sceneAllocator;
    std::unique_ptr<StackAllocator>       m_stackAllocator;
    std::unique_ptr<BuddyAllocator>       m_buddyAllocator;
    std::unique_ptr<PagedAllocator>       m_pagedAllocator;

    // プールアロケーターマップ（型IDで管理）
    std::unordered_map<std::type_index, std::unique_ptr<IPoolBase>> m_pools;

    // 統計情報
    size_t m_totalFrameAllocations    = 0;
    size_t m_totalSceneAllocations    = 0;
    size_t m_subAllocatorReservedBytes = 0; ///< サブアロケーターが予約した合計バイト数
};

// ========================================
// テンプレート実装
// ========================================

template<typename T, size_t PoolSize>
PoolAllocator<T, PoolSize>& MemoryManager::getPool() {
    assert(m_masterResource && "MemoryManager::init() が呼ばれていません");

    std::type_index typeIdx = typeid(T);

    auto it = m_pools.find(typeIdx);
    if (it == m_pools.end()) {
        // マスター FreeList からブロック配列を確保する
        using Block = typename PoolAllocator<T, PoolSize>::Block;
        static_assert(alignof(Block) <= 255,
                      "PoolAllocator のブロックアライメントは 255 以下でなければなりません "
                      "(FreeListAllocator は alignment <= 255 のみサポートします)");
        auto& master = m_masterResource->getAllocator();
        void* blockBuf = master.allocate(sizeof(Block) * PoolSize, alignof(Block));
        if (!blockBuf) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "MemoryManager::getPool: ブロック配列の確保に失敗 (型: %s, %zu 個)",
                         typeid(T).name(), PoolSize);
            // map に登録しない（次回呼び出しで再試行できるようにする）
            assert(false && "MemoryManager::getPool: ブロック配列の確保に失敗");
            throw std::bad_alloc{};
        }
        auto* blocks = static_cast<Block*>(blockBuf);
        auto holder = std::make_unique<PoolHolder<T, PoolSize>>(blocks, master);
        auto* poolPtr = &holder->pool;
        m_pools[typeIdx] = std::move(holder);
        return *poolPtr;
    }

    // 既存プールを返す
    auto* holder = static_cast<PoolHolder<T, PoolSize>*>(it->second.get());
    return holder->pool;
}

} // namespace mk::memory
