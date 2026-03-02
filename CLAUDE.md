# makai — 開発引き継ぎプロンプト

## プロジェクト概要

C++20 + SDL3 で開発している 2D シミュレーションゲーム。
PSP の「俺に働けって言われても」に近いゲームを目指している。

## ビルド方法

```bash
cmake -S . -B build
cmake --build build
./build/makai
```

SDL3 は FetchContent で自動ダウンロード。

## 外部ライブラリのセットアップ

### SDL3_image（手動配置が必要）

FetchContent でのビルドが通らないためプリビルド版を使用。`external/` は .gitignore 対象なので各自でダウンロードして配置すること。

1. https://github.com/libsdl-org/SDL_image/releases から `SDL3_image-devel-3.4.0-VC.zip` をダウンロード
2. 解凍して `external/SDL3_image-3.4.0/` に配置

## 設計上の注意点

- SceneManager のシーン切り替えは必ず遅延コマンド経由（`push/pop/replace/clear` → `applyPendingChanges()`）。update/render 中に直接スタックを触らないこと。

## コーディング規約

- 名前空間: `makai::`
- メンバ変数: `m_` プレフィックス
- スマートポインタを使う（`new` / `delete` は原則禁止）
- コメントは日本語で記載する
