#pragma once
// 文字列ハッシュID — 文字列比較を高速な整数比較に変換する
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
constexpr StringID HashString(const char* str)
{
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
    return hash;
}

// HashString のエイリアス（短縮形）
constexpr StringID SID(const char* str)
{
    return HashString(str);
}

} // namespace mk
