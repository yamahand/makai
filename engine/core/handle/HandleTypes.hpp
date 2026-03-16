#pragma once
// HandleTypes — ゲームエンジン固有のハンドル型定義
//
// 新しいリソース型を追加する場合はここにタグとエイリアスを追加する
//
// 使用例:
//   TextureHandle h = texturePool.create();
//   EntityHandle  e = entityPool.create();

#include "Handle.hpp"

namespace mk {

// ---------------------------------------------------------------------------
// タグ型（前方宣言のみ。インスタンス化は不要）
// ---------------------------------------------------------------------------
struct TextureTag  {};
struct MeshTag     {};
struct ShaderTag   {};
struct MaterialTag {};
struct BufferTag   {};
struct EntityTag   {};

// ---------------------------------------------------------------------------
// ハンドル型エイリアス
// ---------------------------------------------------------------------------
using TextureHandle  = Handle<TextureTag>;
using MeshHandle     = Handle<MeshTag>;
using ShaderHandle   = Handle<ShaderTag>;
using MaterialHandle = Handle<MaterialTag>;
using BufferHandle   = Handle<BufferTag>;
using EntityHandle   = Handle<EntityTag>;

} // namespace mk
