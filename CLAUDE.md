# makai — 開発引き継ぎプロンプト

## プロジェクト概要

C++20 + SDL3 で開発している 2D シミュレーションゲーム。
PSP の「俺に働けって言われても」に近いゲームを目指している。

## 動作環境

- **OS:** Windows 10/11 64bit
- **アーキテクチャ:** x86-64（64bit 専用。32bit は非サポート）
- **コンパイラ:** MSVC 2022（C++20）
- **CMake:** 3.25 以上

> `MemoryManager` など一部モジュールは `static_assert(sizeof(size_t) >= 8)` で 64bit 環境を要求します。

## ビルド方法

```bash
cmake -S . -B build
cmake --build build
./build/Debug/makai.exe
```

SDL3・SDL3_ttf・SDL3_image はすべて FetchContent でプレビルドバイナリ（VC 版）を自動ダウンロードします（**現在の正式な手順**）。`external/SDL3_image-3.4.0/` への手動配置や「CMake 3.20+」といった古いドキュメント上の記述が残っている場合は、それらは**廃止済みの情報**なので無視し、この節の内容を真としてください。

## ディレクトリ構造

```
makai/
  engine/              ← エンジン（mk:: 名前空間）
    core/              ← Core 層（id, handle, log, math, time）
    memory/            ← Core 層（アロケータ群）
    platform/          ← Platform 層（Window）
    scene/             ← Engine 層（Scene, SceneManager）
    resource/          ← Engine 層（FontManager, TextureManager）
    imgui/             ← Engine 層（ImGuiManager）
    objects/           ← Engine 層（GameObject）
    states/            ← Engine 層（StateMachine）
    Game.hpp/.cpp      ← Engine 層（ゲームループ）
    App.hpp            ← Engine 層（起動テンプレート）
    Config.hpp/.cpp    ← Engine 層（設定）
  game/                ← ゲーム固有コード（makai:: 名前空間）
  main.cpp             ← エントリーポイント
```

レイヤー構造の詳細は `docs/engine_architecture.md` を参照。

## 設計上の注意点

- SceneManager のシーン切り替えは必ず遅延コマンド経由（`push/pop/replace/clear` → `applyPendingChanges()`）。update/render 中に直接スタックを触らないこと。

## コーディング規約

- 名前空間: エンジン層（`engine/`）は `mk::` / ゲーム層（`game/`）は `makai::`
- メンバ変数: `m_` プレフィックス
- スマートポインタを使う（動的確保の `new` / `delete` は原則禁止。placement new は可）
- コメントは日本語で記載する
