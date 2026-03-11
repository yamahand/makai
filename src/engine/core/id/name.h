#pragma once
// Name — StringID の軽量ラッパー
// asset / component / event などの識別に使用する
//
// 使用例:
//   Name name("player");
//   if (name == Name("enemy")) { /* ... */ }
//   StringID id = name.GetID();

#include "string_id.h"

namespace mk {

class Name
{
public:
    // デフォルトコンストラクタ（無効な名前）
    constexpr Name() : m_id(InvalidStringID) {}

    // 文字列からの構築
    constexpr Name(const char* str) : m_id(SID(str)) {}

    // StringID からの構築
    constexpr explicit Name(StringID id) : m_id(id) {}

    // ハッシュIDを取得する
    constexpr StringID GetID() const { return m_id; }

    // 比較演算子
    constexpr bool operator==(const Name& other) const { return m_id == other.m_id; }
    constexpr bool operator!=(const Name& other) const { return m_id != other.m_id; }

    // ソート用
    constexpr bool operator<(const Name& other) const { return m_id < other.m_id; }

    // 有効な名前かどうか
    constexpr bool IsValid() const { return m_id != InvalidStringID; }
    constexpr explicit operator bool() const { return IsValid(); }

private:
    StringID m_id;
};

} // namespace mk
