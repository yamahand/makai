#pragma once
// NameTable — 文字列インターンテーブル
// 同じ文字列は一度だけ保存し、Name（StringID）と元の文字列の相互変換を提供する
// ゲームエンジン用途を想定し、mutex によるスレッドセーフを保証する
//
// 使用例:
//   auto& table = NameTable::instance();
//   Name name = table.make("player");
//   const char* str = table.toString(name);   // "player"
//   bool found = table.exists(name);           // true

#include "Name.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mk {

// インターンされた文字列エントリ
struct NameEntry
{
    StringID id;
    std::string string;
};

class NameTable
{
public:
    // シングルトンアクセス
    static NameTable& instance();

    // 文字列を登録し、対応する Name を返す
    // 既に登録済みの場合は既存の Name を返す
    Name make(const char* str);

    // Name から元の文字列を取得する
    // 未登録の場合は nullptr を返す
    const char* toString(Name name) const;

    // Name が登録されているかどうかを返す
    bool exists(Name name) const;

    // 登録されているエントリ数を返す
    size_t size() const;

    // コピー・ムーブ禁止
    NameTable(const NameTable&) = delete;
    NameTable& operator=(const NameTable&) = delete;
    NameTable(NameTable&&) = delete;
    NameTable& operator=(NameTable&&) = delete;

private:
    NameTable() = default;
    ~NameTable() = default;

    // StringID → vector 内インデックスのマップ（高速検索用）
    std::unordered_map<StringID, uint32_t> m_indexMap;

    // 登録済みエントリの配列
    std::vector<NameEntry> m_entries;

    // スレッドセーフ用ミューテックス
    mutable std::mutex m_mutex;
};

} // namespace mk
