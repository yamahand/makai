# makai — 開発引き継ぎプロンプト

## プロジェクト概要

**makai** は C++20 + SDL3 で開発している 2D ライフシミュレーションゲームです。
PSP の「俺に働けって言われても」に近いジャンル（仕事・休憩・食事などの行動選択、ステータス管理、ゲーム内時刻進行）を目指しています。

---

## アーキテクチャ

採用パターン: **ゲームループ + シーン管理（スタック式） + GameObject継承 + ステートマシン**

```
makai/
├── CMakeLists.txt              # SDL3 を FetchContent で自動取得、C++20
├── src/
│   ├── main.cpp                # エントリポイント
│   ├── core/
│   │   ├── Game.hpp/cpp        # ゲームループ（processEvents → update → render）
│   │   ├── Window.hpp/cpp      # SDL_Window + SDL_Renderer のラッパー
│   │   └── SceneManager.hpp/cpp# シーンスタック。遅延コマンドで安全に切り替え
│   ├── scenes/
│   │   ├── Scene.hpp           # 純粋仮想基底クラス（handleEvent/update/render）
│   │   ├── TitleScene.hpp/cpp  # タイトル画面。Enter/Space でゲームへ遷移
│   │   └── GameScene.hpp/cpp   # メインシーン。ゲーム内時刻（1秒=30分）管理
│   ├── objects/
│   │   ├── GameObject.hpp      # 基底クラス（x, y, alive フラグ）
│   │   └── Player.hpp/cpp      # PlayerStats + StateMachine を持つプレイヤー
│   ├── states/
│   │   ├── StateMachine.hpp/cpp# 汎用ステートマシン（State 基底クラス含む）
│   │   └── PlayerStates.hpp/cpp# Idle / Working / Resting / Eating
│   └── utils/
│       └── Timer.hpp           # クールダウン・イベント用タイマー（ヘッダのみ）
├── assets/
│   ├── textures/
│   ├── audio/
│   ├── fonts/
│   └── maps/
└── data/
    └── configs/
```

---

## 重要な設計メモ

### SceneManager（スタック式）
- `push` / `pop` / `replace` / `clear` はすべて **遅延コマンド** としてキューに積まれ、
  `applyPendingChanges()` でゲームループの末尾に一括適用される。
  これにより update/render 中にスタックを壊さない。

### PlayerStats
```cpp
struct PlayerStats {
    int hp, mental, hunger, money, stress, day;
};
```
- `add*()` メソッドで 0〜100 にクランプ済み（money は 0 以上）。

### PlayerStates（行動ステート）
| ステート | 遷移トリガー | 効果 |
|----------|-------------|------|
| `PlayerIdleState` | 初期状態 / 各行動完了後 | W→Work / R→Rest / E→Eat |
| `PlayerWorkingState` | Idle で W キー | 4秒後に終了。終了時 +500円。体力・精神・空腹を消費 |
| `PlayerRestingState` | Idle で R キー | 2秒後に終了。体力・精神・ストレスを回復 |
| `PlayerEatingState` | Idle で E キー | 1秒後に終了。満腹+40、体力+10、-300円 |

### ゲーム内時刻（GameScene）
- `GAME_HOURS_PER_REAL_SECOND = 0.5f`（現実1秒 = ゲーム30分）
- 24時になったら日をまたぐ処理を呼ぶ（現在は TODO）

---

## ビルド方法

```bash
cmake -S . -B build
cmake --build build
./build/makai
```

SDL3 は FetchContent で自動ダウンロード（初回のみ時間がかかる）。

---

## 現在の実装状況

- [x] ゲームループ（固定 deltaTime キャップ付き）
- [x] ウィンドウ / レンダラー生成
- [x] シーンマネージャー（スタック式・遅延切り替え）
- [x] TitleScene（背景色のみ、Enter でゲーム遷移）
- [x] GameScene（ゲーム内時刻進行・日またぎ検出）
- [x] GameObject 基底クラス
- [x] Player（デバッグ用矩形描画）
- [x] PlayerStats（体力・精神力・満腹度・お金・ストレス）
- [x] StateMachine（汎用）
- [x] PlayerStates（Idle / Working / Resting / Eating）
- [x] Timer ユーティリティ

---

## 次に実装すべきこと（優先度順）

1. **SDL3_ttf の導入** — テキスト描画（HUD、時刻表示、ダイアログ）に必須
2. **SDL3_image の導入** — PNG スプライト読み込み
3. **HUD 描画**（GameScene::renderHUD）— 時刻・ステータスゲージ表示
4. **行動選択メニュー** — Idle 時に選択肢を画面に表示する UI
5. **日またぎ処理** — 睡眠・翌日の初期化・体力回復
6. **マップ描画** — タイル or 背景スプライト
7. **イベントシステム** — 時刻・日数によるランダムイベント発生
8. **セーブ / ロード** — JSON（nlohmann/json など）でステータスを永続化

---

## コーディング規約

- 名前空間: `makai::`
- メンバ変数: `m_` プレフィックス
- スマートポインタを使う（`new` / `delete` は原則禁止）
- SDL のリソース管理は RAII（Window クラスのデストラクタ参照）
- `TODO:` コメントで未実装箇所を明示済み
- コメントは日本語で記載する
