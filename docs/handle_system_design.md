# Handle System Design

このドキュメントはゲームエンジンで使用する **Handle System（ハンドル管理システム）**を定義します。

目的

* ポインタの直接公開を避ける
* 無効ポインタのクラッシュを防ぐ
* オブジェクトの安全な参照を提供する
* Resource / ECS / GPU オブジェクト管理を統一する

---

# Handleとは

Handleとは **オブジェクトを参照するID型**です。

ポインタの代わりに使用します。

例

```id="c57zk4"
TextureHandle
MeshHandle
EntityHandle
BufferHandle
```

ハンドルは **オブジェクトのインデックス + 世代番号**を持ちます。

---

# なぜHandleを使うのか

ポインタ管理の問題

```id="4gmlup"
Texture* texture = getTexture();

destroyTexture(texture);

texture->bind(); // クラッシュ
```

Handleを使う場合

```id="6j20rr"
TextureHandle texture = loadTexture("player.png");

Texture* tex = getTexture(texture);
```

破棄後

```id="3uqv9a"
getTexture(texture) → nullptr
```

クラッシュを防げます。

---

# Handle構造

推奨は **64bit Handle**

構成

```id="azv7t1"
| generation | index |
```

例

```id="k1t7g1"
32bit generation
32bit index
```

---

# Handle型

例

```id="pf63op"
struct TextureHandle
{
    uint64_t value = 0;
};
```

またはテンプレート

```id="62xup7"
template<typename Tag>
struct Handle
{
    uint64_t value = 0;
};
```

例

```id="q9ktw3"
using TextureHandle = Handle<TextureTag>;
using MeshHandle = Handle<MeshTag>;
```

---

# Handle作成

例

```id="w80k7f"
uint32_t index;
uint32_t generation;

handle.value = ((uint64_t)generation << 32) | index;
```

---

# Handle分解

```id="aq1yys"
uint32_t index = handle.value & 0xffffffff;
uint32_t generation = handle.value >> 32;
```

---

# HandlePool

オブジェクト管理には **HandlePool** を使用します。

例

```id="9y0bg3"
template<typename T>
class HandlePool
{
public:

    Handle<T> create();

    void destroy(Handle<T> handle);

    T* get(Handle<T> handle);

private:

    Array<T> objects;

    Array<uint32_t> generations;

};
```

---

# create

```id="72f70p"
TextureHandle handle = texturePool.create();
```

---

# destroy

```id="ayv4yz"
texturePool.destroy(handle);
```

destroy時

```id="0du2r2"
generation++
```

これにより **古いHandleが無効化される**

---

# get

```id="ap9k6p"
Texture* texture = texturePool.get(handle);
```

内部チェック

```id="egk78e"
if (generation != generations[index])
    return nullptr;
```

---

# 無効Handle

無効ハンドル

```id="o8bwjk"
constexpr uint64_t InvalidHandle = 0;
```

チェック

```id="x9m3sb"
bool isValid()
```

---

# ResourceManagerとの連携

ResourceManagerはHandleを返す。

例

```id="s9xy5g"
TextureHandle loadTexture(const char* path);
```

取得

```id="rsl33p"
Texture* getTexture(TextureHandle handle);
```

---

# ECSとの連携

EntityもHandleにできます。

```id="7ct21k"
using EntityHandle = Handle<EntityTag>;
```

例

```id="fmyy4o"
EntityHandle createEntity();
void destroyEntity(EntityHandle entity);
```

---

# GPU Resource例

```id="l3d96d"
TextureHandle
BufferHandle
ShaderHandle
PipelineHandle
```

RendererはHandleでリソースを管理します。

---

# Handleディレクトリ構造

```id="bqemx2"
core
 ├ handle
 │   ├ Handle.h
 │   ├ HandlePool.h
 │   └ HandleTypes.h
```

例

```id="3z7h2q"
TextureHandle
MeshHandle
MaterialHandle
EntityHandle
```

---

# Handle設計ルール

Handleは

* 軽量
* コピー可能
* POD型

にする。

禁止

```id="8ui11u"
virtual
動的メモリ
```

---

# Handle使用ルール

Handleを

* Resource
* Entity
* GPU Object

すべてで統一する。

直接ポインタを公開しない。

例

```id="6yqfzb"
TextureHandle loadTexture(...);

Texture* getTexture(TextureHandle);
```

---

# まとめ

Handleは

```id="gjh6fu"
index + generation
```

で構成される。

利点

* ダングリングポインタ防止
* 安全なオブジェクト参照
* 高速

Handle Systemは

```id="w3o2w9"
Resource
ECS
Renderer
```

すべての基盤になる。

---
