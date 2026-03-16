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

```cpp
TextureHandle
MeshHandle
EntityHandle
BufferHandle
```

ハンドルは **オブジェクトのインデックス + 世代番号**を持ちます。

---

# なぜHandleを使うのか

ポインタ管理の問題

```cpp
Texture* texture = getTexture();

destroyTexture(texture);

texture->bind(); // クラッシュ
```

Handleを使う場合

```cpp
TextureHandle texture = loadTexture("player.png");

Texture* tex = getTexture(texture);
```

破棄後

```cpp
getTexture(texture) → nullptr
```

クラッシュを防げます。

---

# Handle構造

推奨は **64bit Handle**

構成

```cpp
| generation | index |
```

例

```cpp
32bit generation
32bit index
```

---

# Handle型

例

```cpp
struct TextureHandle
{
    uint64_t value = 0;
};
```

またはテンプレート

```cpp
template<typename Tag>
struct Handle
{
    uint64_t value = 0;
};
```

例

```cpp
using TextureHandle = Handle<TextureTag>;
using MeshHandle = Handle<MeshTag>;
```

---

# Handle作成

例

```cpp
uint32_t index;
uint32_t generation;

handle.value = ((uint64_t)generation << 32) | index;
```

---

# Handle分解

```cpp
uint32_t index = handle.value & 0xffffffff;
uint32_t generation = handle.value >> 32;
```

---

# HandlePool

オブジェクト管理には **HandlePool** を使用します。

例

```cpp
template<typename T, typename Tag, uint32_t Capacity = 256>
class HandlePool
{
public:

    template<typename... Args>
    Handle<Tag> create(Args&&... args);

    void destroy(Handle<Tag> handle);

    T* get(Handle<Tag> handle);

private:

    struct Slot {
        alignas(T) std::byte storage[sizeof(T)];
        uint32_t generation = 1;
        bool     alive      = false;
    };

    Slot     m_slots[Capacity]     {};
    uint32_t m_freeStack[Capacity] {};
    uint32_t m_freeTop             = 0;
    size_t   m_size                = 0;
};
```

---

# create

```cpp
TextureHandle handle = texturePool.create();
```

---

# destroy

```cpp
texturePool.destroy(handle);
```

destroy時

```cpp
generation++
```

これにより **古いHandleが無効化される**

---

# get

```cpp
Texture* texture = texturePool.get(handle);
```

内部チェック

```cpp
if (generation != generations[index])
    return nullptr;
```

---

# 無効Handle

無効ハンドル

```cpp
constexpr uint64_t InvalidHandle = 0;
```

チェック

```cpp
bool isValid()
```

---

# ResourceManagerとの連携

ResourceManagerはHandleを返す。

例

```cpp
TextureHandle loadTexture(const char* path);
```

取得

```cpp
Texture* getTexture(TextureHandle handle);
```

---

# ECSとの連携

EntityもHandleにできます。

```cpp
using EntityHandle = Handle<EntityTag>;
```

例

```cpp
EntityHandle createEntity();
void destroyEntity(EntityHandle entity);
```

---

# GPU Resource例

```cpp
TextureHandle
BufferHandle
ShaderHandle
PipelineHandle
```

RendererはHandleでリソースを管理します。

---

# Handleディレクトリ構造

```cpp
core
 ├ handle
 │   ├ Handle.hpp
 │   ├ HandlePool.hpp
 │   └ HandleTypes.hpp
```

例

```cpp
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

```cpp
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

```cpp
TextureHandle loadTexture(...);

Texture* getTexture(TextureHandle);
```

---

# まとめ

Handleは

```cpp
index + generation
```

で構成される。

利点

* ダングリングポインタ防止
* 安全なオブジェクト参照
* 高速

Handle Systemは

```cpp
Resource
ECS
Renderer
```

すべての基盤になる。

---
