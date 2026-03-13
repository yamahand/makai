#pragma once
// TypeInfo — ランタイム型情報を保持する構造体
// ECS / シリアライゼーション / リフレクション / エディタ用途
//
// 将来拡張: FieldInfo を追加し std::vector<FieldInfo> fields を持たせることで
//           リフレクションシステムに拡張できる
//
// 使用例:
//   const TypeInfo* info = TypeRegistry::instance().findType(typeId<MyComponent>());
//   assert(info && info->size == sizeof(MyComponent));

#include "TypeId.hpp"
#include "Name.hpp"

#include <cstddef>

namespace mk {

// ---------------------------------------------------------------------------
// FieldInfo — 将来の Reflection 拡張用（現時点ではコメントで予約）
// struct FieldInfo {
//     Name   name;    // フィールド名
//     TypeId typeId;  // フィールドの型ID
//     size_t offset;  // 構造体先頭からのオフセット（bytes）
// };
// ---------------------------------------------------------------------------

struct TypeInfo
{
    // 型の一意ID（typeId<T>() で採番。実行ごとに値が変わり得る）
    TypeId id        = 0;

    // 型名（NameTable でインターンされた文字列に紐づく）
    // 永続化が必要な場合は型名ハッシュ等の安定したIDを別途使用すること
    Name   name      {};

    // sizeof(T)
    size_t size      = 0;

    // alignof(T)
    size_t alignment = 0;

    // 有効な TypeInfo かどうか（id == 0 は無効）
    constexpr bool isValid() const { return id != 0; }
};

} // namespace mk
