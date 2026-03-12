# Core System Design

このドキュメントはゲームエンジンの **Core システム設計** を定義します。

Coreはエンジンの最下層であり、すべてのモジュールから使用されます。

目的

* 一意なIDシステム
* 高速な文字列識別
* 型情報の管理
* 将来的なリフレクション対応

本システムは以下で構成されます。

```
Core ID System
StringID
Name
TypeRegistry
```

---

# Core ID System

エンジン内の多くのオブジェクトは **整数IDで管理**します。

理由

* ポインタより安全
* キャッシュ効率が高い
* ECSやResource管理と相性が良い

例

```id="h1c8dn"
using EntityID = uint32_t;
using ComponentID = uint32_t;
using TypeID = uint32_t;
```

ルール

* 0 は **Invalid ID**
* IDは連番またはハッシュ

例

```id="6b8bwe"
constexpr EntityID InvalidEntity = 0;
```

---

# StringID

StringIDは **文字列を高速に識別するID**です。

用途

* リソース名
* ECSコンポーネント名
* イベント名
* シェーダー名
* UI要素名

文字列比較の代わりに **整数比較**を行います。

---

## 例

```id="t3byu1"
StringID id = StringID("player");
```

比較

```id="stpnb3"
if (id == StringID("player"))
{
}
```

---

# StringID 実装方針

内部は **ハッシュ値**を使用します。

例

```id="k66a7x"
class StringID
{
public:

    constexpr StringID() = default;

    constexpr StringID(const char* str);

    uint32_t value() const;

private:

    uint32_t m_hash = 0;

};
```

推奨ハッシュ

* FNV1a
* xxHash
* RapidHash

---

# Name

Nameは **StringID のみを保持する軽量型**です。

元文字列との相互変換は **NameTable** が担います。

用途

* ECS コンポーネント識別
* リソース名
* イベント名

例

```id="8f83ol"
class Name
{
public:

    constexpr Name() = default;
    constexpr explicit Name(const char* str);  // sid() でハッシュ化
    constexpr explicit Name(StringID id);

    constexpr StringID getId() const;
    constexpr bool isValid() const;

private:

    StringID m_id = InvalidStringID;

};
```

利点

* IDのみのため軽量（4バイト）
* ホットパスでのコピーコストが最小
* 元文字列が必要な場合は NameTable から取得する

---

# NameTable

NameTableは **文字列インターンテーブル**です。

`Name`（StringID）と元の文字列の相互変換を提供します。

用途

* デバッグ
* ログ
* エディタ

特徴

* 同じ文字列は一度だけ保存（インターン）
* `std::deque` によるポインタ安定性（`toString()` で返したポインタは追加後も有効）
* mutex によるスレッドセーフ
* ハッシュ衝突を検出（Debug: assert、Release: ログ出力）

例

```id="name_table_ex"
auto& table = NameTable::instance();

Name name = table.make("player");       // 登録 & Name 取得
const char* str = table.toString(name); // "player"（常に有効）
bool found = table.exists(name);        // true
```

利点

* `Name` を軽量に保ちつつ文字列を引ける
* ライフタイム管理が不要（テーブルがオーナー）

---

# TypeRegistry

TypeRegistryは **型情報を管理するシステム**です。

用途

* ECS
* Serialization
* Reflection
* Editor

---

## TypeID

型はすべて **TypeID**を持ちます。

```id="zvovfd"
using TypeID = uint32_t;
```

---

# 型登録

型を登録する。

例

```id="2kgmp7"
TypeID registerType(const char* name);
```

テンプレート版

```id="t8mnpd"
template<typename T>
TypeID registerType();
```

---

# 型取得

```id="vfwg0b"
TypeInfo* getType(TypeID id);
```

名前から取得

```id="swbxt4"
TypeInfo* findType(StringID name);
```

---

# TypeInfo

型情報

例

```id="67bbzj"
struct TypeInfo
{
    TypeID id;

    StringID name;

    size_t size;

    size_t alignment;
};
```

将来的に追加可能

```
constructor
destructor
serializer
component flag
```

---

# TypeRegistry例

```id="y82xhb"
class TypeRegistry
{
public:

    template<typename T>
    TypeID registerType();

    TypeInfo* getType(TypeID id);

    TypeInfo* findType(StringID name);

private:

    HashMap<TypeID, TypeInfo> m_types;

};
```

---

# Reflection拡張

TypeRegistryは将来的に **Reflectionシステム**に拡張できます。

例

```
FieldInfo
MethodInfo
Attribute
```

例

```id="5uf6ja"
struct FieldInfo
{
    StringID name;

    TypeID type;

    size_t offset;
};
```

---

# ECSとの連携

TypeRegistryは **ECS Component管理**にも使用できます。

例

```id="m9yqbg"
ComponentID registerComponent<T>();
```

取得

```id="35rm5p"
ComponentInfo* getComponent(ComponentID id);
```

---

# Resource管理

ResourceにもTypeIDを使えます。

例

```id="rrc8m1"
Texture
Mesh
Shader
Material
```

例

```id="pbjtiu"
ResourceHandle loadResource(TypeID type, const char* path);
```

---

# Core System構造

推奨ディレクトリ

```
core
 ├ id
 ├ string
 ├ name
 └ type
```

例

```
core
 ├ StringID.h
 ├ Name.h
 ├ TypeRegistry.h
 ├ TypeInfo.h
```

---

# Core設計ルール

Coreは以下を守る。

* OS APIを使用しない
* Renderer APIを使用しない
* Engine APIを使用しない
* 他モジュールに依存しない

Coreは **完全に独立したライブラリ**とする。

---

# まとめ

Core Systemは以下で構成される。

```
ID System
StringID
Name
TypeRegistry
```

これらは

* ECS
* Resource
* Serialization
* Editor

の基盤になる。

Coreは **ゲームエンジンの最も重要なモジュール**である。

---
