#pragma once
#include "LinearAllocator.hpp"
#include "FreeListMemoryResource.hpp"
#include "PoolAllocator.hpp"
#include "../core/Config.hpp"
#include <memory_resource>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cstdlib>

namespace mk::memory {

/// MemoryManager - メモリアロケーターの中央管理
///
/// シングルトンパターンで、ゲーム全体のメモリ割り当てを管理する。
/// Game::Game() の先頭で init(config.memory) を呼ぶ必要がある。
///
/// 起動時に単一のマスターバッファを確保し、各アロケーターに分配する。
/// （OS の malloc 呼び出しはこの 1 回のみ）
///
/// 使い方:
///   MemoryManager::init(config.memory);   // Game::Game() で 1 回だけ
///   auto& mem = MemoryManager::instance();
///   void* ptr = mem.frameAllocator().allocate(1024);
class MemoryManager {
public:
    /// シングルトンインスタンスを取得（init() 呼び出し後のみ有効）
    static MemoryManager& instance();

    /// マスターバッファを確保してアロケーターを初期化する
    /// Game::Game() の先頭（他のどの初期化よりも前）に 1 度だけ呼ぶこと
    static void init(const mk::MemoryConfig& config);

    /// フレームアロケーターを取得
    /// 毎フレーム終了時にリセットされる
    LinearAllocator& frameAllocator() { return *m_frameAllocator; }

    /// シーンアロケーターを取得
    /// シーン変更時にリセットされる
    LinearAllocator& sceneAllocator() { return *m_sceneAllocator; }

    /// ヒープアロケーターを取得（後方互換）
    /// 可変サイズ・個別解放に対応した汎用アロケーター（First-Fit）
    FirstFitAllocator& heapAllocator() { return m_heapResource->getAllocator(); }

    /// ヒープの pmr リソースを取得
    /// std::pmr コンテナへそのまま渡せる
    std::pmr::memory_resource* heapMemoryResource() { return m_heapResource.get(); }

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

        // シーンアロケーター
        size_t sceneBytes;
        size_t sceneCapacity;
        float  sceneUsageRatio;

        // ヒープアロケーター（FreeList）
        size_t heapBytes;
        size_t heapCapacity;
        float  heapUsageRatio;
        size_t heapAllocationCount;
        size_t heapFreeBlockCount;

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
    };

    // マスターバッファ（起動時に 1 度だけ malloc する）
    void*  m_masterBuffer = nullptr;
    size_t m_masterSize   = 0;

    // 各アロケーター（init() 後に有効）
    std::unique_ptr<LinearAllocator>        m_frameAllocator;
    std::unique_ptr<LinearAllocator>        m_sceneAllocator;
    std::unique_ptr<FirstFitMemoryResource> m_heapResource;

    // プールアロケーターマップ（型IDで管理）
    std::unordered_map<std::type_index, std::unique_ptr<IPoolBase>> m_pools;

    // 統計情報
    size_t m_totalFrameAllocations = 0;
    size_t m_totalSceneAllocations = 0;
};

// ========================================
// テンプレート実装
// ========================================

template<typename T, size_t PoolSize>
PoolAllocator<T, PoolSize>& MemoryManager::getPool() {
    std::type_index typeIdx = typeid(T);

    auto it = m_pools.find(typeIdx);
    if (it == m_pools.end()) {
        // 初回アクセス時にプールを生成
        auto holder = std::make_unique<PoolHolder<T, PoolSize>>();
        auto* poolPtr = &holder->pool;
        m_pools[typeIdx] = std::move(holder);
        return *poolPtr;
    }

    // 既存プールを返す
    auto* holder = static_cast<PoolHolder<T, PoolSize>*>(it->second.get());
    return holder->pool;
}

} // namespace mk::memory
