#pragma once
// TypeId — C++ の型に一意の整数IDを割り当てる
// ECS / reflection-lite などランタイム内での型識別に使用する
// RTTI に依存しない
//
// 注意: IDは型の初出順に採番されるため、実行ごとに値が変わり得る
//       永続化（serialization）には型名ハッシュ等の安定したID方式を別途使用すること
//
// 使用例:
//   uint32_t id1 = TypeId<PlayerComponent>();
//   uint32_t id2 = TypeId<TransformComponent>();
//   assert(id1 != id2);
//   assert(id1 == TypeId<PlayerComponent>()); // 同じ型は同じID

#include <cstdint>
#include <atomic>
#include <cassert>
#include <cstdlib>

namespace mk {

// 型ごとに一意のIDを返す
// 同じ型なら常に同じIDを返す（実行中は安定）
using TypeId = uint32_t;

namespace detail {

// 型IDカウンター（スレッドセーフ）
inline TypeId nextTypeId()
{
    static std::atomic<TypeId> s_counter{1};
    TypeId id = s_counter.fetch_add(1, std::memory_order_relaxed);
    // オーバーフローによるID重複を検出する
    assert(id != 0 && "TypeId counter overflow");
    // Release ビルドでも安全性を担保する
    if (id == 0)
    {
        std::abort();
    }
    return id;
}

} // namespace detail

template<typename T>
TypeId typeId()
{
    static const TypeId id = detail::nextTypeId();
    return id;
}

} // namespace mk
