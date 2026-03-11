#pragma once
// TypeRegistry — C++ の型に一意の整数IDを割り当てる
// ECS / serialization / reflection-lite などで使用する
// RTTI に依存しない
//
// 使用例:
//   uint32_t id1 = TypeID<PlayerComponent>::Get();
//   uint32_t id2 = TypeID<TransformComponent>::Get();
//   assert(id1 != id2);
//   assert(id1 == TypeID<PlayerComponent>::Get()); // 同じ型は同じID

#include <cstdint>
#include <atomic>

namespace mk {

namespace detail {

// 型IDカウンター（スレッドセーフ）
inline uint32_t NextTypeID()
{
    static std::atomic<uint32_t> s_counter{1};
    return s_counter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace detail

// 型ごとに一意のIDを返す
// 同じ型なら常に同じIDを返す（実行中は安定）
template<typename T>
struct TypeID
{
    static uint32_t Get()
    {
        static const uint32_t s_id = detail::NextTypeID();
        return s_id;
    }
};

} // namespace mk
