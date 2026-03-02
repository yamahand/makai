#pragma once
#include "LinearAllocator.hpp"
#include "PoolAllocator.hpp"
#include <unordered_map>
#include <typeindex>
#include <memory>

namespace makai::memory {

/// MemoryManager - メモリアロケーターの中央管理
///
/// シングルトンパターンで、ゲーム全体のメモリ割り当てを管理する。
/// - FrameAllocator: 毎フレームリセットされる一時データ用
/// - SceneAllocator: シーン変更時にリセットされるシーンデータ用
///
/// 使い方:
///   auto& mem = MemoryManager::instance();
///   void* ptr = mem.frameAllocator().allocate(1024);
class MemoryManager {
public:
    /// シングルトンインスタンスを取得
    static MemoryManager& instance();

    /// フレームアロケーターを取得
    /// 毎フレーム終了時にリセットされる
    LinearAllocator& frameAllocator() { return m_frameAllocator; }

    /// シーンアロケーターを取得
    /// シーン変更時にリセットされる
    LinearAllocator& sceneAllocator() { return m_sceneAllocator; }

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
    MemoryManager();
    ~MemoryManager() = default;

    // 型消去されたプールの基底クラス
    struct IPoolBase {
        virtual ~IPoolBase() = default;
    };

    // 型付きプールラッパー
    template<typename T, size_t PoolSize>
    struct PoolHolder : IPoolBase {
        PoolAllocator<T, PoolSize> pool;
    };

    // アロケーター
    LinearAllocator m_frameAllocator; // 4MB
    LinearAllocator m_sceneAllocator; // 16MB

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

} // namespace makai::memory
