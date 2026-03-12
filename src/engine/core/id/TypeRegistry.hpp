#pragma once
// TypeRegistry — C++ の型に一意の整数IDを割り当てる
// ECS / reflection-lite などランタイム内での型識別に使用する
// RTTI に依存しない
//
// 注意: IDは型の初出順に採番されるため、実行ごとに値が変わり得る
//       永続化（serialization）には型名ハッシュ等の安定したID方式を別途使用すること
//
// 使用例:
//   uint32_t id1 = TypeID<PlayerComponent>::get();
//   uint32_t id2 = TypeID<TransformComponent>::get();
//   assert(id1 != id2);
//   assert(id1 == TypeID<PlayerComponent>::get()); // 同じ型は同じID

#include <cstdint>
#include <atomic>
#include <cassert>
#include <cstdlib>

namespace mk {

namespace detail {

// 型IDカウンター（スレッドセーフ）
inline uint32_t nextTypeId()
{
    static std::atomic<uint32_t> s_counter{1};
    uint32_t id = s_counter.fetch_add(1, std::memory_order_relaxed);
    // オーバーフローによるID重複を検出する
    assert(id != 0 && "TypeID counter overflow");
    // Release ビルドでも安全性を担保する
    if (id == 0)
    {
        std::abort();
    }
    return id;
}

} // namespace detail

// 型ごとに一意のIDを返す
// 同じ型なら常に同じIDを返す（実行中は安定）
template<typename T>
struct TypeID
{
    static uint32_t get()
    {
        static const uint32_t s_id = detail::nextTypeId();
        return s_id;
    }
};

} // namespace mk
