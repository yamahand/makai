#include "GameScene.hpp"
#include "../core/Game.hpp"

namespace makai {

GameScene::GameScene(Game& game)
    : Scene(game)
    , m_player(std::make_unique<Player>())
{
}

void GameScene::onEnter() {
    m_gameTime  = 8.0f;
    m_dayCount  = 1;
    m_isDayOver = false;
}

void GameScene::onExit() {
    // セーブ処理などをここに追加
}

void GameScene::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        m_player->handleInput(event.key.scancode);
    }
}

void GameScene::update(float deltaTime) {
    updateDayCycle(deltaTime);
    m_player->update(deltaTime);
}

void GameScene::updateDayCycle(float deltaTime) {
    if (m_isDayOver) return;

    m_gameTime += deltaTime * GAME_HOURS_PER_REAL_SECOND;

    if (m_gameTime >= DAY_END_HOUR) {
        m_isDayOver = true;
        m_gameTime  = 0.0f;
        m_dayCount++;
        // TODO: 日をまたいだ処理（睡眠・回復・翌日の準備など）
    }
}

void GameScene::render(SDL_Renderer* renderer) {
    // 背景
    SDL_SetRenderDrawColor(renderer, 30, 100, 30, 255);
    SDL_RenderClear(renderer);

    m_player->render(renderer);
    renderHUD(renderer);
}

void GameScene::renderHUD(SDL_Renderer* renderer) {
    // TODO: SDL3_ttf 追加後にHUDテキストを描画
    // 表示予定: 時刻、曜日、体力/精神力/お金/空腹ゲージ
    (void)renderer;
}

} // namespace makai
