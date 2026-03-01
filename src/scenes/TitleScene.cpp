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
    // Background
    SDL_SetRenderDrawColor(renderer, 20, 20, 60, 255);
    SDL_RenderClear(renderer);

    // Title text
    SDL_Color white = {255, 255, 255, 255};
    SDL_Texture* titleTexture = m_game.fontManager().renderTextTexture(
        renderer, "large", "MAKAI", white
    );

    if (titleTexture) {
        float w, h;
        SDL_GetTextureSize(titleTexture, &w, &h);
        SDL_FRect titleRect = {
            (m_game.window().getWidth() - w) / 2,
            200,
            w, h
        };
        SDL_RenderTexture(renderer, titleTexture, nullptr, &titleRect);
        SDL_DestroyTexture(titleTexture);
    }

    // "Press ENTER" blinking text (toggle every 0.5 seconds)
    if (static_cast<int>(m_timer * 2) % 2 == 0) {
        SDL_Texture* promptTexture = m_game.fontManager().renderTextTexture(
            renderer, "default", "Press ENTER to Start", white
        );

        if (promptTexture) {
            float w, h;
            SDL_GetTextureSize(promptTexture, &w, &h);
            SDL_FRect promptRect = {
                (m_game.window().getWidth() - w) / 2,
                400,
                w, h
            };
            SDL_RenderTexture(renderer, promptTexture, nullptr, &promptRect);
            SDL_DestroyTexture(promptTexture);
        }
    }
}

} // namespace makai
