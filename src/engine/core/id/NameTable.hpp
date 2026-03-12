#pragma once
// NameTable — 文字列インターンテーブル
// 同じ文字列は一度だけ保存し、Name（StringID）と元の文字列の相互変換を提供する
// ゲームエンジン用途を想定し、mutex によるスレッドセーフを保証する
//
// ポインタ安定性:
//   内部コンテナに std::deque を使用しているため、make() による要素追加後も
//   過去に toString() で返したポインタは無効化されない
//
// ハッシュ衝突:
//   StringID は FNV-1a 32bit ハッシュのため衝突の可能性がある
//   make() で衝突を検出した場合、Debug ビルドでは assert で停止し、
//   Release ビルドでは CORE_ERROR ログを出力して先に登録された文字列の Name を返す
//
// 使用例:
//   auto& table = NameTable::instance();
//   Name name = table.make("player");
//   const char* str = table.toString(name);   // "player" — make() 後も有効
//   bool found = table.exists(name);           // true

#include "Name.hpp"

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

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
    // 同一文字列が登録済みの場合は既存の Name を返す
    // ハッシュ衝突（異なる文字列が同一 StringID）を検出した場合:
    //   Debug: assert で停止
    //   Release: CORE_ERROR ログを出力し、先に登録された文字列の Name を返す
    Name make(const char* str);

    // Name から元の文字列を取得する
    // 未登録の場合は nullptr を返す
    // 返されたポインタは、後続の make() 呼び出し後も有効（deque のポインタ安定性による）
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

    // StringID → deque 内インデックスのマップ（高速検索用）
    std::unordered_map<StringID, uint32_t> m_indexMap;

    // 登録済みエントリの配列
    // std::deque を使用することで push_back 時に既存要素のポインタが無効化されない
    std::deque<NameEntry> m_entries;

    // スレッドセーフ用ミューテックス
    mutable std::mutex m_mutex;
};

} // namespace mk
