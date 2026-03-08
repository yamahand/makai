#pragma once
#include "LinearAllocator.hpp"
#include "DoubleFrameAllocator.hpp"
#include "FreeListMemoryResource.hpp"
#include "PoolAllocator.hpp"
#include "../core/Config.hpp"
#include <SDL3/SDL_log.h>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <optional>
#include <cstdlib>
#include <cassert>

namespace mk::memory {

/// MemoryManager - メモリアロケーターの中央管理
///
/// シングルトンパターンで、ゲーム全体のメモリ割り当てを管理する。
/// Game::Game() の先頭で init(config.memory) を呼ぶ必要がある。
///
/// 起動時に単一のマスターバッファを確保し、各アロケーターに分配する。
/// （マスターバッファの OS への malloc 呼び出しと、init() 内部の std::make_unique 等による初期化時の一時的な確保を除き、
///  初期化完了後のランタイム中は、getPool<T>() の PoolHolder も含めて OS ヒープへのアクセスは発生しないことを意図している）
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

    /// 型Tのプールアロケーターを取得
    /// 初回アクセス時に自動的に生成される
    template<typename T, size_t PoolSize = 256>
    PoolAllocator<T, PoolSize>& getPool();

    /// 型Tのプールアロケーターが既に生成済みかどうかを確認する（プールを生成しない）
    template<typename T>
    bool hasPool() const;

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
        const size_t poolSize;  ///< getPool<T, PoolSize>() 呼び出し時のテンプレート引数 PoolSize（異なる PoolSize での再取得を検出するために記録）
        explicit IPoolBase(size_t ps) : poolSize(ps) {}
        virtual ~IPoolBase() = default;

        /// 動的型のデストラクタを呼び出し、領域を allocator に返却する
        /// PoolHolderDeleter から呼ばれる。明示的なデストラクタ呼び出しでは
        /// 仮想ディスパッチが保証されないため、この仮想関数を経由する。
        virtual void destroy(FirstFitAllocator& allocator) noexcept = 0;
    };

    // 型付きプールラッパー
    template<typename T, size_t PoolSize>
    struct PoolHolder : IPoolBase {
        PoolAllocator<T, PoolSize> pool;

        /// 外部バッファ版（マスター FreeList からブロック配列を受け取る）
        PoolHolder(typename PoolAllocator<T, PoolSize>::Block* blocks, FirstFitAllocator& backing)
            : IPoolBase(PoolSize), pool(blocks, backing) {}

        /// PoolAllocator（ブロック配列返却を含む）を破棄し、自身の領域を返却する
        void destroy(FirstFitAllocator& allocator) noexcept override {
            this->~PoolHolder();        // PoolHolder（と PoolAllocator）のデストラクタを呼ぶ
            allocator.deallocate(this); // PoolHolder 本体の領域をマスター FreeList に返却
        }
    };

    /// PoolHolder 本体をマスター FreeList へ返却するカスタムデリーター
    struct PoolHolderDeleter {
        FirstFitAllocator* allocator = nullptr;  ///< 返却先アロケーター
        void operator()(IPoolBase* ptr) const {
            if (!ptr || !allocator) return;
            ptr->destroy(*allocator);  // 仮想 destroy() 経由で動的型を破棄し領域を返却
        }
    };

    /// OS ヒープを使わない PoolHolder スマートポインタ型
    using PoolHolderPtr = std::unique_ptr<IPoolBase, PoolHolderDeleter>;

    // マスターバッファ（起動時に 1 度だけ malloc する）
    void*  m_masterBuffer = nullptr;
    size_t m_masterSize   = 0;

    /// マスター FreeList（バッファ全体を管理し、各アロケーターにバッファを払い出す）
    std::unique_ptr<FirstFitMemoryResource> m_masterResource;

    // 各アロケーター（マスター FreeList からバッファを借りる）
    std::unique_ptr<LinearAllocator>      m_frameAllocator;
    std::unique_ptr<DoubleFrameAllocator> m_doubleFrameAllocator;
    std::unique_ptr<LinearAllocator>      m_sceneAllocator;

    /// プールアロケーターマップ（型IDで管理）
    /// std::pmr::unordered_map はマスター FreeList からノード・バケットを確保するため OS ヒープを使わない
    /// m_masterResource より後に初期化する必要があるため std::optional で遅延初期化する
    std::optional<std::pmr::unordered_map<std::type_index, PoolHolderPtr>> m_pools;

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
    assert(m_pools.has_value() && "MemoryManager::init() が呼ばれていません");

    std::type_index typeIdx = typeid(T);

    auto it = m_pools->find(typeIdx);
    if (it == m_pools->end()) {
        using Block = typename PoolAllocator<T, PoolSize>::Block;
        static_assert(alignof(Block) <= 255,
                      "PoolAllocator のブロックアライメントは 255 以下でなければなりません "
                      "(FreeListAllocator は alignment <= 255 のみサポートします)");
        static_assert(alignof(PoolHolder<T, PoolSize>) <= 255,
                      "PoolHolder のアライメントが FreeListAllocator の上限 (255) を超えています");

        auto& master = m_masterResource->getAllocator();

        // (1) ブロック配列をマスター FreeList から確保する
        void* blockBuf = master.allocate(sizeof(Block) * PoolSize, alignof(Block));
        if (!blockBuf) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "MemoryManager::getPool: ブロック配列の確保に失敗 (型: %s, %zu 個)",
                         typeid(T).name(), PoolSize);
            assert(false && "MemoryManager::getPool: ブロック配列の確保に失敗");
            throw std::bad_alloc{};
        }

        // (2) PoolHolder 本体をマスター FreeList から確保する
        void* holderBuf = master.allocate(
            sizeof(PoolHolder<T, PoolSize>),
            alignof(PoolHolder<T, PoolSize>));
        if (!holderBuf) {
            master.deallocate(blockBuf);  // ブロック配列のリーク防止
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "MemoryManager::getPool: PoolHolder の確保に失敗 (型: %s)",
                         typeid(T).name());
            assert(false && "MemoryManager::getPool: PoolHolder の確保に失敗");
            throw std::bad_alloc{};
        }

        // (3) placement new で PoolHolder を構築する（OS ヒープ不使用）
        auto* holder = ::new(holderBuf)
            PoolHolder<T, PoolSize>(static_cast<Block*>(blockBuf), master);

        // (4) 即座に PoolHolderPtr に所有権を渡す
        //     以降は ptr のデストラクタが holder + blockBuf を確実に解放するため
        //     次の emplace が bad_alloc を投げてもリークしない
        PoolHolderPtr ptr(static_cast<IPoolBase*>(holder), PoolHolderDeleter{&master});

        // (5) emplace で挿入する
        //     operator[] を使うと「ノード確保 → デフォルト構築 → 代入」の順に動くため、
        //     中間のデフォルト構築された PoolHolderPtr が発生して余分なムーブ代入が行われる。
        //     ここでは emplace を使うことで、マップ内の要素を一度の構築で済ませて効率化している。
        //     なお、この時点で ptr は既に holder + blockBuf の所有権を持っているため、
        //     ノード確保中に bad_alloc が投げられてもスタックアンワインド時に ptr のデストラクタで正しく解放され、リークは発生しない。
        auto* poolPtr = &holder->pool;
        m_pools->emplace(typeIdx, std::move(ptr));
        return *poolPtr;
    }

    // 既存プールを返す（PoolSize が一致していることを確認する）
    auto* base = it->second.get();
    if (base->poolSize != PoolSize) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "MemoryManager::getPool: PoolSize 不一致 (型: %s, 既存: %zu, 要求: %zu)",
                     typeid(T).name(), base->poolSize, PoolSize);
        assert(false && "MemoryManager::getPool: PoolSize mismatch");
        throw std::logic_error("MemoryManager::getPool: PoolSize mismatch");
    }
    auto* holder = static_cast<PoolHolder<T, PoolSize>*>(base);
    return holder->pool;
}

template<typename T>
bool MemoryManager::hasPool() const {
    if (!m_pools.has_value()) return false;
    return m_pools->find(std::type_index(typeid(T))) != m_pools->end();
}

} // namespace mk::memory
