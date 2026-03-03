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

SceneRenderData TitleScene::collectRenderData() const {
    SceneRenderData data;
    data.clearColor = {20, 20, 60, 255};

    SDL_Color white = {255, 255, 255, 255};
    float centerX = static_cast<float>(m_game.window().getWidth()) / 2.0f;

    // タイトルテキスト（水平中央寄せ）
    data.texts.push_back({
        .fontName    = "large",
        .text        = "MAKAI",
        .x           = centerX,
        .y           = 200.0f,
        .color       = white,
        .alignCenter = true
    });

    // 「ENTER を押してください」点滅テキスト（0.5秒ごとに切り替え）
    if (static_cast<int>(m_timer * 2) % 2 == 0) {
        data.texts.push_back({
            .fontName    = "default",
            .text        = "Press ENTER to Start",
            .x           = centerX,
            .y           = 400.0f,
            .color       = white,
            .alignCenter = true
        });
    }

    return data;
}

} // namespace makai
