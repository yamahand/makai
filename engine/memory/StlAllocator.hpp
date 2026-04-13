#pragma once
#include <cstddef>
#include <cstdlib>      // std::abort
#include <limits>
#include <memory>       // std::allocator_traits
#include <type_traits>
#include "../core/log/Logger.hpp"

namespace mk::memory {

/// StlAllocator - STL コンテナ用アロケーターアダプター
///
/// LinearAllocator・FreeListAllocator などの既存アロケーターを
/// STL コンテナから利用するためのアダプタ（C++ named requirement
/// Allocator の全要件を満たすわけではない）。
/// 使用例:
/// @code
/// auto& heap = memoryManager().heapAllocator();
/// std::vector<int, mk::memory::StlAllocator<int, FirstFitAllocator>> v(heap);
///
/// auto& frame = memoryManager().frameAllocator();
/// std::vector<int, mk::memory::StlAllocator<int, LinearAllocator>> tmp(frame);
/// @endcode
///
/// 注意:
/// - バッキングアロケーターへの参照が必要なためデフォルトコンストラクタを持たない。
///   デフォルト構築が必要なコンテナ（一部の std::map など）には使用できない。
/// - LinearAllocator の deallocate は no-op なので、フレームバッファ等の
///   一時コンテナにのみ使うこと
///
/// @tparam T           要素の型
/// @tparam Backing     バッキングアロケーター型（LinearAllocator / FreeListAllocator など）
template<typename T, typename Backing>
class StlAllocator {
public:
    using value_type = T;

    // rebind: std::list などの内部ノード型に対して使われる
    template<typename U>
    struct rebind {
        using other = StlAllocator<U, Backing>;
    };

    /// バッキングアロケーターへの参照を受け取るコンストラクタ
    explicit StlAllocator(Backing& backing) noexcept
        : m_backing(&backing)
    {}

    /// rebind コンストラクタ（異なる T から変換）
    template<typename U>
    StlAllocator(const StlAllocator<U, Backing>& other) noexcept
        : m_backing(other.m_backing)
    {}

    /// n 個の T を割り当てる
    /// 例外無効環境では失敗時に abort する（MSVC STL の /EHs-c- 動作と一貫）
    T* allocate(std::size_t n) {
        // n * sizeof(T) の乗算オーバーフローを検出する
        if (sizeof(T) > 0 && n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            MK_BOOT_ERROR("StlAllocator::allocate: サイズオーバーフロー");
            std::abort();
        }
        void* ptr = m_backing->allocate(n * sizeof(T), alignof(T));
        if (!ptr) {
            MK_BOOT_ERROR("StlAllocator::allocate: メモリ確保に失敗しました");
            std::abort();
        }
        return static_cast<T*>(ptr);
    }

    /// n 個の T を解放する
    void deallocate(T* ptr, std::size_t /*n*/) noexcept {
        m_backing->deallocate(ptr);
    }

    bool operator==(const StlAllocator& other) const noexcept {
        return m_backing == other.m_backing;
    }

    bool operator!=(const StlAllocator& other) const noexcept {
        return !(*this == other);
    }

    template<typename, typename>
    friend class StlAllocator;

private:
    Backing* m_backing;
};

} // namespace mk::memory
