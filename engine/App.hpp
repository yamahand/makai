#pragma once
#include "Game.hpp"
#include <concepts>

namespace mk {

// T が Game を継承していること、Game 自身でないこと、デフォルト構築可能であることを要求するコンセプト
template<typename T>
concept DerivedGame = std::derived_from<T, Game> && (!std::same_as<T, Game>) && std::default_initializable<T>;

// main 関数の定型コード（初期化・実行）をエンジン側に隠蔽するテンプレート関数
// 例外無効環境: 致命的エラーは MK_VERIFY_MSG / std::abort() で処理される
template<DerivedGame T>
int runApp(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    T app;
    app.init();  // 仮想関数が正しくディスパッチされるよう、構築後に明示的に呼ぶ
    app.run();
    return 0;
}

} // namespace mk

// ゲーム側の main.cpp に記述するマクロ
// 例: MAKAI_APP(makai::App)
#define MAKAI_APP(AppClass) \
    int main(int argc, char* argv[]) { \
        return mk::runApp<AppClass>(argc, argv); \
    }
