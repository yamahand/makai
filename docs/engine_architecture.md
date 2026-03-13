# Game Engine Architecture

このドキュメントはゲームエンジンの **モジュール構造と依存ルール** を定義します。

目的

* エンジン構造を明確にする
* 依存関係の崩壊を防ぐ
* AIツール（Copilot / Codex / Claude）が正しい構造でコードを生成できるようにする

本エンジンは **C++20** を前提とします。

---

# レイヤー構造

エンジンは次のレイヤーで構成します。

```
Game
 ↑
Engine
 ↑
Renderer
 ↑
Platform
 ↑
Core
```

依存関係は **下方向のみ許可**されます。

```
Game → Engine → Renderer → Platform → Core
```

逆方向の依存は禁止します。

---

# Core

最下層のライブラリです。

目的

* 基本データ構造
* メモリ管理
* ログ
* IDシステム
* 型システム

Coreは **他のすべてのモジュールから使用されます**。

例

```
core/
    memory/
    container/
    string/
    log/
    id/
    type/
```

※補足  
現在のリポジトリでは `src/engine/core/` 配下に `Game/` や `Window/`、`SceneManager/`、`ImGuiManager/` など、ここでいうレイヤー構造の「Game / Engine」側に属するモジュールも含まれています。  
このドキュメントで示している `core/ memory/ ... log/` の構成は「最下層の低レベルCore」を指す概念的な例であり、将来的なリファクタで `src/engine/core/id` や `src/engine/core/memory` などの低レベルユーティリティをこのレイヤーとして整理することを意図しています。  
現時点では、「Core（最下層）」という用語は **低レベルな共通ユーティリティ群** を指すものとし、`src/engine/core/` 直下の高レベルモジュール（Game/Window/SceneManager/ImGuiManager 等）は「Engine / Game レイヤー側の実装」として扱ってください。

主な機能

```
Allocator
Array
HashMap
String
StringID
TypeRegistry
Logger
```

ルール

* OS APIを直接使用しない
* Renderer APIを使用しない
* Engine APIを使用しない

---

# Platform

OS依存処理をまとめるレイヤー。

例

```
platform/
    window/
    input/
    filesystem/
    timer/
```

例

```
Window
Input
FileSystem
Timer
```

使用する可能性のあるライブラリ

* SDL3
* OS API
* file IO

ルール

* Coreに依存可能
* Rendererには依存しない

---

# Renderer

描画システム。

例

```
renderer/
    rhi/
    vulkan/
    dx12/
    resource/
```

主な役割

```
Device
Buffer
Texture
Shader
Pipeline
CommandBuffer
```

例

```
Texture
Buffer
Shader
Pipeline
RenderPass
```

ルール

* Coreに依存可能
* Platformに依存可能
* Engineには依存しない

---

# Engine

ゲームエンジン機能。

例

```
engine/
    ecs/
    scene/
    asset/
    system/
```

主な機能

```
Entity
Component
Scene
ResourceManager
System
```

例

```
TransformComponent
CameraComponent
LightComponent
```

ルール

* Rendererを使用可能
* Platformを使用可能
* Coreを使用可能

---

# Game

ゲーム固有コード。

例

```
game/
    gameplay/
    ui/
    logic/
```

例

```
PlayerController
EnemyAI
GameScene
```

ルール

* Engineを使用
* Renderer直接使用は可能だが基本は避ける

---

# 依存関係ルール

許可される依存

```
Game → Engine
Engine → Renderer
Renderer → Platform
Platform → Core
```

禁止される依存

```
Core → Platform
Core → Renderer
Renderer → Engine
Engine → Game
```

---

# モジュール構造

推奨ディレクトリ構造

```
engine/
    core/
    platform/
    renderer/
    engine/
    game/
```

例

```
engine
 ├ core
 ├ platform
 ├ renderer
 ├ engine
 └ game
```

---

# Renderer API設計

Rendererは **抽象化レイヤー(RHI)** を持ちます。

例

```
Renderer
 ├ RHI
 ├ Vulkan
 └ DX12
```

RHI例

```
class Device
class Buffer
class Texture
class Shader
class Pipeline
```

バックエンド

```
VulkanDevice
DX12Device
```

---

# Resource管理

すべてのGPUリソースは **Handleベース**で管理します。

例

```
TextureHandle
BufferHandle
MeshHandle
```

例

```
TextureHandle loadTexture(const char* path);

Texture* getTexture(TextureHandle handle);
```

ポインタを直接公開することは避けます。

---

# ECS設計

エンジンは **Entity Component System** を使用します。

基本構造

```
Entity
Component
System
```

例

```
EntityID
TransformComponent
RenderSystem
PhysicsSystem
```

---

# エンジンライフサイクル

エンジンの初期化順序

```
Core
Platform
Renderer
Engine
Game
```

シャットダウン順序

```
Game
Engine
Renderer
Platform
Core
```

---

# AI開発ルール

AIツールがコードを書く場合は以下を守る。

* レイヤー依存を破らない
* Coreを最下層にする
* RendererをEngineから分離する
* GameコードをEngineに入れない

---

# まとめ

レイヤー構造

```
Game
Engine
Renderer
Platform
Core
```

依存方向

```
Game → Engine → Renderer → Platform → Core
```

このルールは **必ず守ること**。

---
