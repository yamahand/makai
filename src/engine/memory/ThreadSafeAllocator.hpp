#pragma once
#include <mutex>
#include <cstddef>

namespace mk::memory {

/// ThreadSafeAllocator（スレッドセーフラッパー）
///
/// 任意のアロケーターを std::mutex でラップし、
/// allocate() / deallocate() をスレッドセーフにする。
///
/// このゲームはシングルスレッドのため通常は不要だが、
/// オーディオスレッドや将来のワーカースレッドからメモリを確保する場合に使う。
///
/// 使い方:
/// @code
///   ThreadSafeAllocator<FirstFitAllocator> safeAlloc(someAllocator);
///   void* ptr = safeAlloc.allocate(256);
///   safeAlloc.deallocate(ptr);
/// @endcode
///
/// @tparam Allocator ラップするアロケーターの型
///         allocate(size_t, size_t) と deallocate(void*) を持つこと
template<typename Allocator>
class ThreadSafeAllocator {
public:
    /// コンストラクタ
    /// @param wrapped ラップするアロケーター（参照を保持する）
    explicit ThreadSafeAllocator(Allocator& wrapped)
        : m_wrapped(wrapped)
    {}

    /// スレッドセーフなメモリ割り当て
    void* allocate(size_t size, size_t alignment = 16) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_wrapped.allocate(size, alignment);
    }

    /// スレッドセーフなメモリ解放
    void deallocate(void* ptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_wrapped.deallocate(ptr);
    }

    /// 使用中バイト数（ロックなし・参考値）
    size_t getUsedBytes() const { return m_wrapped.getUsedBytes(); }

    /// 総容量（ロックなし）
    size_t getCapacity() const { return m_wrapped.getCapacity(); }

    // コピー禁止（mutex はコピー不可）
    ThreadSafeAllocator(const ThreadSafeAllocator&) = delete;
    ThreadSafeAllocator& operator=(const ThreadSafeAllocator&) = delete;

private:
    Allocator&         m_wrapped; ///< ラップ対象アロケーター
    mutable std::mutex m_mutex;   ///< アクセス保護用ミューテックス
};

} // namespace mk::memory
