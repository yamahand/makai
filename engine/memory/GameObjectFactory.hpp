#pragma once
#include "MemoryManager.hpp"
#include <utility>
#include <SDL3/SDL_log.h>

namespace mk::memory {

/// GameObjectFactory - GameObject生成/破棄ファクトリー
///
/// PoolAllocatorを使用してGameObjectを効率的に管理する。
/// placement newとexplicit destructorを使用。
///
/// 使い方:
///   Player* player = GameObjectFactory::create<Player>(args...);
///   GameObjectFactory::destroy(player);
class GameObjectFactory {
public:
    /// オブジェクトを生成する
    /// @tparam T オブジェクトの型（PoolAllocatorに登録済みである必要がある）
    /// @param args コンストラクタ引数
    /// @return 生成されたオブジェクトへのポインタ（プール枯渇時はnullptr）
    template<typename T, typename... Args>
    static T* create(Args&&... args) {
        auto& pool = MemoryManager::instance().getPool<T>();
        T* ptr = pool.allocate();

        if (!ptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "GameObjectFactory: Failed to allocate %s (pool exhausted)",
                        typeid(T).name());
            return nullptr;
        }

        // placement new でコンストラクタを呼ぶ
        // 例外無効環境ではコンストラクタ内のエラーは abort される
        new (ptr) T(std::forward<Args>(args)...);

        #ifdef DEBUG_MEMORY_VERBOSE
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                    "GameObjectFactory: Created %s (pool usage: %zu/%zu)",
                    typeid(T).name(), pool.getUsedCount(), pool.getCapacity());
        #endif

        return ptr;
    }

    /// オブジェクトを破棄する
    /// @tparam T オブジェクトの型
    /// @param ptr 破棄するオブジェクトへのポインタ
    template<typename T>
    static void destroy(T* ptr) {
        if (!ptr) {
            return;
        }

        // explicit destructorを呼ぶ
        ptr->~T();

        // プールに返却
        auto& pool = MemoryManager::instance().getPool<T>();
        pool.deallocate(ptr);

        #ifdef DEBUG_MEMORY_VERBOSE
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                    "GameObjectFactory: Destroyed %s (pool usage: %zu/%zu)",
                    typeid(T).name(), pool.getUsedCount(), pool.getCapacity());
        #endif
    }

    /// 配列を生成する
    /// @tparam T オブジェクトの型
    /// @param count 配列のサイズ
    /// @return 生成された配列の先頭ポインタ（失敗時はnullptr）
    template<typename T>
    static T* createArray(size_t count) {
        auto& pool = MemoryManager::instance().getPool<T>();

        // 連続したブロックを確保できるか確認
        if (pool.getFreeCount() < count) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "GameObjectFactory: Failed to allocate array of %zu %s (only %zu free)",
                        count, typeid(T).name(), pool.getFreeCount());
            return nullptr;
        }

        T* array = pool.allocate();
        if (!array) {
            return nullptr;
        }

        // 残りの要素を割り当て
        for (size_t i = 1; i < count; ++i) {
            T* elem = pool.allocate();
            if (!elem) {
                // 失敗したら既に割り当てた分を解放
                for (size_t j = 0; j < i; ++j) {
                    pool.deallocate(&array[j]);
                }
                return nullptr;
            }
        }

        // placement newで全要素を初期化
        for (size_t i = 0; i < count; ++i) {
            new (&array[i]) T();
        }

        return array;
    }

    /// 配列を破棄する
    /// @tparam T オブジェクトの型
    /// @param array 破棄する配列の先頭ポインタ
    /// @param count 配列のサイズ
    template<typename T>
    static void destroyArray(T* array, size_t count) {
        if (!array) {
            return;
        }

        auto& pool = MemoryManager::instance().getPool<T>();

        // 全要素のデストラクタを呼ぶ
        for (size_t i = 0; i < count; ++i) {
            array[i].~T();
            pool.deallocate(&array[i]);
        }
    }

    // インスタンス化禁止（静的メソッドのみ）
    GameObjectFactory() = delete;
    ~GameObjectFactory() = delete;
    GameObjectFactory(const GameObjectFactory&) = delete;
    GameObjectFactory& operator=(const GameObjectFactory&) = delete;
};

} // namespace mk::memory
