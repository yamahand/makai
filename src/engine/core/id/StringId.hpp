#pragma once
// 文字列ハッシュID — 文字列比較を高速な整数比較に変換する
// 衝突が起こり得るため、同じ文字列は同じIDになりますが、異なる文字列が同じIDになる可能性もあります
//
// 使用例:
//   constexpr StringID playerID = SID("player");
//   StringID enemyID = SID("enemy");
//   if (playerID == enemyID) { /* ... */ }

#include <cstdint>

namespace mk {

// 文字列ハッシュの型（32bit）
using StringID = uint32_t;

// 無効なStringIDを表す定数
inline constexpr StringID InvalidStringID = 0;

// FNV-1a 32bit ハッシュで文字列をハッシュ化する
// nullptr の場合は InvalidStringID を返す
// 結果が 0 の場合は 1 に変換し、0 を無効値として予約する
constexpr StringID hashString(const char* str)
{
    if (str == nullptr)
    {
        return InvalidStringID;
    }

    // FNV-1a パラメータ（32bit）
    constexpr uint32_t fnvOffsetBasis = 2166136261u;
    constexpr uint32_t fnvPrime = 16777619u;

    uint32_t hash = fnvOffsetBasis;
    while (*str)
    {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(*str));
        hash *= fnvPrime;
        ++str;
    }

    // 0 は InvalidStringID として予約されているため回避する
    return (hash == InvalidStringID) ? 1u : hash;
}

// hashString の短縮エイリアス（constexpr 関数）
// 意図的に全大文字を維持している（マクロ風の短縮名として可読性を優先）
constexpr StringID SID(const char* str)
{
    return hashString(str);
}

} // namespace mk
