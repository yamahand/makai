#include "GameScene.hpp"
#include "engine/core/Game.hpp"
#include "engine/memory/GameObjectFactory.hpp"
#include <string>
#include <sstream>
#include <iomanip>

namespace makai {

GameScene::GameScene(mk::Game& game)
    : mk::Scene(game)
    , m_player(mk::memory::GameObjectFactory::create<Player>())
{
    // シーンアロケーター使用例:
    // シーン固有の一時データをシーンアロケーターから割り当て可能
    // 例:
    //   auto& sceneAlloc = game.memoryManager().sceneAllocator();
    //   m_uiLayout = (UILayout*)sceneAlloc.allocate(sizeof(UILayout));
    //   new (m_uiLayout) UILayout();
    //
    // シーン遷移時に自動的にリセットされるため、手動解放不要
}

void GameScene::onEnter() {
    m_gameTime  = 8.0f;
    m_dayCount  = 1;
    m_isDayOver = false;
}

void GameScene::onExit() {
    // セーブ処理などをここに追加

    // プールにPlayerを返却
    if (m_player) {
        mk::memory::GameObjectFactory::destroy(m_player);
        m_player = nullptr;
    }
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

    // テスト用スプライトを描画（プレイヤーの位置を使用）
    m_game.textureManager().renderTexture("test",
                                          m_player->getX(),
                                          m_player->getY(),
                                          64, 64);

    renderHUD(renderer);
}

void GameScene::renderStatBar(SDL_Renderer* renderer, float x, float y, float w, float h,
                               int value, int maxValue, SDL_Color barColor) {
    // 背景（暗いグレー）
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_FRect bgRect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &bgRect);

    // ゲージの埋域
    float fillWidth = (w - 4) * (static_cast<float>(value) / maxValue);
    SDL_SetRenderDrawColor(renderer, barColor.r, barColor.g, barColor.b, barColor.a);
    SDL_FRect fillRect = {x + 2, y + 2, fillWidth, h - 4};
    SDL_RenderFillRect(renderer, &fillRect);

    // 枠（白）
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &bgRect);
}

void GameScene::renderHUD(SDL_Renderer* renderer) {
    const PlayerStats& stats = m_player->stats();
    SDL_Color white = {255, 255, 255, 255};

    // 時刻を HH:MM 形式でフォーマット
    int hours = static_cast<int>(m_gameTime);
    int minutes = static_cast<int>((m_gameTime - hours) * 60);
    std::ostringstream timeStr;
    timeStr << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setfill('0') << std::setw(2) << minutes;

    // 左上: 時刻と日手
    SDL_Texture* timeTexture = m_game.fontManager().renderTextTexture(
        renderer, "default", timeStr.str(), white
    );
    if (timeTexture) {
        float w, h;
        SDL_GetTextureSize(timeTexture, &w, &h);
        SDL_FRect rect = {10, 10, w, h};
        SDL_RenderTexture(renderer, timeTexture, nullptr, &rect);
        SDL_DestroyTexture(timeTexture);
    }

    std::string dayStr = "Day " + std::to_string(m_dayCount);
    SDL_Texture* dayTexture = m_game.fontManager().renderTextTexture(
        renderer, "default", dayStr, white
    );
    if (dayTexture) {
        float w, h;
        SDL_GetTextureSize(dayTexture, &w, &h);
        SDL_FRect rect = {10, 35, w, h};
        SDL_RenderTexture(renderer, dayTexture, nullptr, &rect);
        SDL_DestroyTexture(dayTexture);
    }

    // 右側: ステータスバー
    float barX = m_game.window().getWidth() - 210;
    float barY = 10;
    float barW = 200;
    float barH = 20;
    float barSpacing = 28;

    // HP バー（赤）
    SDL_Color red = {200, 50, 50, 255};
    renderStatBar(renderer, barX, barY, barW, barH, stats.hp, 100, red);
    std::string hpText = "HP: " + std::to_string(stats.hp);
    SDL_Texture* hpTexture = m_game.fontManager().renderTextTexture(
        renderer, "default", hpText, white
    );
    if (hpTexture) {
        float w, h;
        SDL_GetTextureSize(hpTexture, &w, &h);
        SDL_FRect rect = {barX + barW + 10, barY + (barH - h) / 2, w, h};
        SDL_RenderTexture(renderer, hpTexture, nullptr, &rect);
        SDL_DestroyTexture(hpTexture);
    }
    barY += barSpacing;

    // 精神力バー（青）
    SDL_Color blue = {50, 100, 200, 255};
    renderStatBar(renderer, barX, barY, barW, barH, stats.mental, 100, blue);
    std::string mentalText = "Mental: " + std::to_string(stats.mental);
    SDL_Texture* mentalTexture = m_game.fontManager().renderTextTexture(
        renderer, "default", mentalText, white
    );
    if (mentalTexture) {
        float w, h;
        SDL_GetTextureSize(mentalTexture, &w, &h);
        SDL_FRect rect = {barX + barW + 10, barY + (barH - h) / 2, w, h};
        SDL_RenderTexture(renderer, mentalTexture, nullptr, &rect);
        SDL_DestroyTexture(mentalTexture);
    }
    barY += barSpacing;

    // 満腹度バー（緑／黄）
    SDL_Color green = {100, 200, 50, 255};
    renderStatBar(renderer, barX, barY, barW, barH, stats.hunger, 100, green);
    std::string hungerText = "Hunger: " + std::to_string(stats.hunger);
    SDL_Texture* hungerTexture = m_game.fontManager().renderTextTexture(
        renderer, "default", hungerText, white
    );
    if (hungerTexture) {
        float w, h;
        SDL_GetTextureSize(hungerTexture, &w, &h);
        SDL_FRect rect = {barX + barW + 10, barY + (barH - h) / 2, w, h};
        SDL_RenderTexture(renderer, hungerTexture, nullptr, &rect);
        SDL_DestroyTexture(hungerTexture);
    }
    barY += barSpacing;

    // ストレスバー（紫）
    SDL_Color purple = {150, 50, 200, 255};
    renderStatBar(renderer, barX, barY, barW, barH, stats.stress, 100, purple);
    std::string stressText = "Stress: " + std::to_string(stats.stress);
    SDL_Texture* stressTexture = m_game.fontManager().renderTextTexture(
        renderer, "default", stressText, white
    );
    if (stressTexture) {
        float w, h;
        SDL_GetTextureSize(stressTexture, &w, &h);
        SDL_FRect rect = {barX + barW + 10, barY + (barH - h) / 2, w, h};
        SDL_RenderTexture(renderer, stressTexture, nullptr, &rect);
        SDL_DestroyTexture(stressTexture);
    }
    barY += barSpacing;

    // 所持金（バーなしでテキストのみ）
    std::string moneyText = "Money: ¥" + std::to_string(stats.money);
    SDL_Texture* moneyTexture = m_game.fontManager().renderTextTexture(
        renderer, "default", moneyText, white
    );
    if (moneyTexture) {
        float w, h;
        SDL_GetTextureSize(moneyTexture, &w, &h);
        SDL_FRect rect = {barX, barY, w, h};
        SDL_RenderTexture(renderer, moneyTexture, nullptr, &rect);
        SDL_DestroyTexture(moneyTexture);
    }

    // 下部: 現在のステート
    std::string stateText = "State: ";
    stateText += m_player->getCurrentStateName();
    SDL_Texture* stateTexture = m_game.fontManager().renderTextTexture(
        renderer, "default", stateText, white
    );
    if (stateTexture) {
        float w, h;
        SDL_GetTextureSize(stateTexture, &w, &h);
        SDL_FRect rect = {10, m_game.window().getHeight() - h - 10, w, h};
        SDL_RenderTexture(renderer, stateTexture, nullptr, &rect);
        SDL_DestroyTexture(stateTexture);
    }
}

} // namespace makai
