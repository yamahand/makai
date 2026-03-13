#pragma once
// Handle — オブジェクトを安全に参照するID型
// ポインタの代わりに使用し、ダングリング参照を防ぐ
//
// 構造:
//   64bit = 上位32bit(generation) + 下位32bit(index)
//   value == 0 は無効ハンドルとして予約する
//
// 使用例:
//   TextureHandle h = texturePool.create();
//   Texture* tex = texturePool.get(h);   // 有効なら非null
//   texturePool.destroy(h);
//   tex = texturePool.get(h);            // nullptr（古いハンドル）

#include <cstdint>

namespace mk {

// ---------------------------------------------------------------------------
// Handle<Tag> — タグ型で区別される軽量ハンドル（POD）
// ---------------------------------------------------------------------------
template<typename Tag>
struct Handle
{
    uint64_t value = 0;

    // 下位32bit: スロットインデックス
    uint32_t index()      const { return static_cast<uint32_t>(value & 0xFFFFFFFF); }

    // 上位32bit: 世代番号
    uint32_t generation() const { return static_cast<uint32_t>(value >> 32); }

    // 有効なハンドルかどうか（value == 0 は無効）
    bool isValid() const { return value != 0; }
    explicit operator bool() const { return isValid(); }

    bool operator==(const Handle&) const = default;
    bool operator!=(const Handle&) const = default;
};

// ---------------------------------------------------------------------------
// makeHandle — HandlePool 内部から呼ぶファクトリ関数
// generation は最低 1 からスタートするため value == 0 にはならない
// ---------------------------------------------------------------------------
template<typename Tag>
Handle<Tag> makeHandle(uint32_t index, uint32_t generation)
{
    Handle<Tag> h;
    h.value = (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index);
    return h;
}

} // namespace mk
