#include "TitleScene.hpp"
#include "GameScene.hpp"
#include "../core/Game.hpp"

namespace makai {

TitleScene::TitleScene(Game& game)
    : Scene(game)
{
}

void TitleScene::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        // 何かキーを押したらゲームシーンへ
        if (event.key.scancode == SDL_SCANCODE_RETURN ||
            event.key.scancode == SDL_SCANCODE_SPACE) {
            m_game.sceneManager().replace(std::make_unique<GameScene>(m_game));
        }
    }
}

void TitleScene::update(float deltaTime) {
    m_timer += deltaTime;
}

void TitleScene::render(SDL_Renderer* renderer) {
    // TODO: タイトルロゴ・テキスト描画
    // 現在はデバッグ用の背景色のみ
    SDL_SetRenderDrawColor(renderer, 20, 20, 60, 255);
    SDL_RenderClear(renderer);

    // "Press ENTER" の点滅（0.5秒ごとに切り替え）
    if (static_cast<int>(m_timer * 2) % 2 == 0) {
        // TODO: テキスト描画ライブラリ（SDL3_ttf）追加後に実装
    }
}

} // namespace makai
