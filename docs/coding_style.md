# C++ コーディング規約（ゲームエンジン）

このドキュメントは、本ゲームエンジンで使用する **命名規則・API命名ルール・設計方針** を定義します。

目的

* コードの一貫性を保つ
* 可読性を高める
* AIツール（Copilot / Codex / Claude など）との相性を良くする
* APIの意味を明確にする

本エンジンは **C++20 以降のみを対象**とします。

---

# 命名規則

## クラス / 構造体

**PascalCase**

例

```
class Renderer;
class Texture;
class EntityManager;
class TransformComponent;
```

ルール

* 名詞を使用する
* 不要な略語は避ける

良い例

```
Renderer
TextureLoader
ResourceManager
```

悪い例

```
tex_mgr
rend
```

---

# 関数

**camelCase**

例

```
initialize()
shutdown()
createTexture()
loadFile()
renderFrame()
```

ルール

* 動詞から始める
* 処理内容が分かる名前にする

---

# メンバ変数

**m_ + camelCase**

例

```
m_width
m_height
m_device
m_transform
```

例

```
class Texture
{
private:

    uint32_t m_width;
    uint32_t m_height;

};
```

目的

* ローカル変数との区別
* 可読性の向上

---

# ローカル変数

**camelCase**

```
frameIndex
deltaTime
textureHandle
entityId
```

---

# 関数引数

**camelCase**

```
loadTexture(const char* path)
createBuffer(size_t size)
```

---

# 定数

**PascalCase**

```
constexpr uint32_t MaxEntities = 100000;
constexpr uint32_t MaxLights = 128;
```

可能な限り **constexpr を使用する**

---

# enum

型名・値ともに **PascalCase**

例

```
enum class RenderAPI
{
    Vulkan,
    DirectX12
};
```

```
enum class TextureFormat
{
    RGBA8,
    BGRA8,
    Depth24Stencil8
};
```

---

# 名前空間

**lowercase**

レイヤーごとに使用する名前空間を分ける。

| レイヤー | 名前空間 |
|---------|---------|
| エンジン層（`src/engine/`） | `mk::` |
| ゲーム層（`src/game/`） | `makai::` |

例

```
namespace mk        // エンジン層
{
}

namespace makai     // ゲーム層
{
}
```

ネスト例

```
namespace mk::render
{
}
```

---

# ファイル名

**PascalCase**

例

```
Renderer.hpp
Renderer.cpp

TextureLoader.hpp
TextureLoader.cpp
```

ルール

* 基本は **1ファイル1クラス**
* ファイル名とクラス名を一致させる
* ヘッダファイルの拡張子はプロジェクトの慣習に従う（本リポジトリでは `.hpp`）

---

# マクロ

**SCREAMING_SNAKE_CASE**

```
ENGINE_ASSERT(x)
ENGINE_LOG(...)
ENGINE_PROFILE_SCOPE(name)
```

マクロには必ず **エンジン名プレフィックス**を付ける。

例

```
MYENGINE_ASSERT
```

---

# ポインタ変数

**ポインタであることを名前に含めない**

悪い例

```
texturePtr
pTexture
```

良い例

```
Texture* texture;
```

理由

* 型でポインタであることが分かる
* 現代C++ではIDEが型情報を表示する

---

# bool変数

自然な文章になる名前にする

```
isRunning
isVisible
hasFocus
isValid
```

悪い例

```
runningFlag
visibleFlag
```

---

# ゲームエンジン特有の命名

## Manager

リソース管理クラス

```
ResourceManager
TextureManager
EntityManager
```

---

## System

ゲームループ処理

```
RenderSystem
PhysicsSystem
AnimationSystem
```

---

## Component

ECSコンポーネント

```
TransformComponent
CameraComponent
LightComponent
```

---

## Handle / ID

ハンドル型

```
TextureHandle
BufferHandle
EntityID
ComponentID
```

---

## Descriptor / CreateInfo

生成パラメータ

```
TextureDesc
BufferDesc
PipelineDesc

ShaderCreateInfo
BufferCreateInfo
```

---

# API命名ルール（ゲームエンジン）

ゲームエンジンでは **動詞ごとに意味を固定する**。

これにより

* APIの意味が明確になる
* AIが正しい関数を生成しやすくなる

---

# create

**新しいオブジェクトを生成する**

特徴

* メモリ確保
* 新規インスタンス

例

```
createTexture()
createBuffer()
createEntity()
```

---

# destroy

**create の逆**

```
destroyTexture()
destroyEntity()
destroyBuffer()
```

---

# load

**外部リソースを読み込む**

特徴

* ファイル読み込み
* キャッシュを利用する場合がある

例

```
loadTexture("player.png")
loadShader("shader.hlsl")
loadMesh("enemy.fbx")
```

---

# get

**必ず存在するオブジェクトを取得**

特徴

* 失敗しない
* 存在しない場合は assert する

例

```
Texture* getTexture(TextureHandle handle);
Entity& getEntity(EntityID id);
```

> **注意:** 既存の一部 API（`TextureManager::getTexture` 等）は過渡的に
> `nullptr` を返す実装になっている。新規 API では上記ルールを厳守すること。

> **Handle System の例外:** `HandlePool::get()` はハンドルの有効性チェックを含むため
> 無効・古いハンドルで `nullptr` を返す。これは Handle System の安全設計上の意図的な例外。

---

# find

**存在しない可能性がある**

例

```
Texture* findTexture(StringID name);
Entity* findEntity(EntityID id);
```

戻り値

```
nullptr
```

---

# tryGet

**安全に取得する**

例

```
bool tryGetTexture(TextureHandle handle, Texture*& outTexture);
```

---

# has

**存在確認**

```
bool hasComponent(EntityID entity);
bool hasTexture(TextureHandle texture);
```

---

# remove

**コンテナから削除**

```
removeComponent(entity);
removeEntity(entity);
```

---

# register

**システム登録**

```
registerSystem(system);
registerComponent(type);
registerService(service);
```

---

# update

**毎フレーム処理**

```
update(deltaTime);
updateAnimation(deltaTime);
```

---

# initialize / shutdown

**エンジンライフサイクル**

```
initialize()
shutdown()
```

---

# API使用例

```
class TextureManager
{
public:

    TextureHandle loadTexture(const char* path);

    Texture* getTexture(TextureHandle handle);

    Texture* findTexture(StringID name);

    void destroyTexture(TextureHandle handle);

};
```

---

# ECS例

```
EntityID createEntity();

void destroyEntity(EntityID entity);

Transform& getTransform(EntityID entity);

bool hasTransform(EntityID entity);
```

---

# ResourceManager例

```
MeshHandle loadMesh(const char* path);

TextureHandle loadTexture(const char* path);

Mesh* getMesh(MeshHandle handle);

Texture* getTexture(TextureHandle handle);
```

---

# まとめ

| 要素        | 命名規則            |
| --------- | --------------- |
| クラス       | PascalCase      |
| 関数        | camelCase       |
| メンバ変数     | m_camelCase     |
| ローカル変数    | camelCase       |
| enum      | PascalCase      |
| namespace | lowercase       |
| マクロ       | SCREAMING_SNAKE |
| ファイル名     | PascalCase      |

---

# 基本方針

1. 短さより **分かりやすさを優先**
2. 不必要な略語は避ける
3. 命名パターンを統一する
4. 人間とAIの両方が理解しやすいコードを書く

---
