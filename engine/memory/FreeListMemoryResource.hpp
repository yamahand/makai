#pragma once
#include "FreeListAllocator.hpp"
#include "../core/log/Logger.hpp"
#include <cstdlib>          // std::abort
#include <memory_resource>

namespace mk::memory {

/// FreeListMemoryResource — std::pmr::memory_resource ラッパー
///
/// FreeListAllocator を所有し、std::pmr::memory_resource インターフェイスを実装する。
/// std::pmr::vector / std::pmr::string などの pmr コンテナにそのまま渡せる。
///
/// 使用例:
/// @code
/// auto* res = memoryManager().heapMemoryResource();
/// std::pmr::vector<int>   v(res);
/// std::pmr::string        s("hello", res);
/// @endcode
///
/// @tparam SearchPolicy  FirstFitPolicy（デフォルト）または BestFitPolicy
template<typename SearchPolicy = FirstFitPolicy>
class FreeListMemoryResource : public std::pmr::memory_resource {
public:
    /// コンストラクタ（内部 malloc 版）
    /// @param capacity バッキングバッファのバイト数
    explicit FreeListMemoryResource(size_t capacity)
        : m_allocator(capacity)
    {}

    /// コンストラクタ（外部バッファ版）
    /// バッファの所有権は呼び出し側が持つ
    FreeListMemoryResource(void* buf, size_t capacity)
        : m_allocator(buf, capacity)
    {}

    /// 内部アロケーターへの参照を返す（低レベルアクセス・統計取得用）
    FreeListAllocator<SearchPolicy>&       getAllocator()       { return m_allocator; }
    const FreeListAllocator<SearchPolicy>& getAllocator() const { return m_allocator; }

    // コピー禁止
    FreeListMemoryResource(const FreeListMemoryResource&)            = delete;
    FreeListMemoryResource& operator=(const FreeListMemoryResource&) = delete;

    // リセット
    void reset() {
        m_allocator.reset();
    }

protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        void* ptr = m_allocator.allocate(bytes, alignment);
        // pmr の契約では失敗時に例外を投げるが、例外無効環境では abort する
        if (!ptr) {
            MK_BOOT_ERROR("FreeListMemoryResource::do_allocate: メモリ確保に失敗しました");
            std::abort();
        }
        return ptr;
    }

    void do_deallocate(void* ptr, size_t /*bytes*/, size_t /*alignment*/) override {
        m_allocator.deallocate(ptr);
    }

    /// 同一インスタンスのときのみ等しいとみなす
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    FreeListAllocator<SearchPolicy> m_allocator;
};

// ─────────────────────────────────────────────
// Convenience aliases
// ─────────────────────────────────────────────

using FirstFitMemoryResource = FreeListMemoryResource<FirstFitPolicy>;
using BestFitMemoryResource  = FreeListMemoryResource<BestFitPolicy>;

} // namespace mk::memory
