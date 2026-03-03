#include "GameScene.hpp"
#include "../core/Game.hpp"
#include "../memory/GameObjectFactory.hpp"
#include <string>
#include <sstream>
#include <iomanip>

namespace makai {

GameScene::GameScene(Game& game)
    : Scene(game)
    , m_player(memory::GameObjectFactory::create<Player>())
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
        memory::GameObjectFactory::destroy(m_player);
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

SceneRenderData GameScene::collectRenderData() const {
    SceneRenderData data;
    data.clearColor = {30, 100, 30, 255};

    // テスト用スプライト（プレイヤーの位置に描画）
    data.sprites.push_back({
        .textureName = "test",
        .x           = m_player->getX(),
        .y           = m_player->getY(),
        .w           = 64.0f,
        .h           = 64.0f
    });

    // プレイヤーの見た目情報を取得
    SpriteInfo info = m_player->getSpriteInfo();
    if (!info.textureName.empty()) {
        // スプライトが設定済みの場合
        data.sprites.push_back({
            .textureName = info.textureName,
            .x           = m_player->getX(),
            .y           = m_player->getY(),
            .w           = info.w,
            .h           = info.h
        });
    } else {
        // テクスチャ未設定はデバッグ矩形として描画
        data.debugRects.push_back({
            .x     = m_player->getX(),
            .y     = m_player->getY(),
            .w     = info.w,
            .h     = info.h,
            .color = info.debugColor
        });
    }

    collectHUD(data);
    return data;
}

void GameScene::renderImGui() {
    // 将来の行動選択UIなどをここに追加する
}

std::string GameScene::formatTime() const {
    int hours   = static_cast<int>(m_gameTime);
    int minutes = static_cast<int>((m_gameTime - hours) * 60);
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << minutes;
    return ss.str();
}

void GameScene::collectHUD(SceneRenderData& data) const {
    const PlayerStats& stats = m_player->stats();
    SDL_Color white = {255, 255, 255, 255};

    // 左上: 時刻
    data.texts.push_back({"default", formatTime(), 10.0f, 10.0f, white});

    // 左上: 日数
    data.texts.push_back({"default", "Day " + std::to_string(m_dayCount), 10.0f, 35.0f, white});

    // 右側: ステータスバーとラベルテキスト
    float barX       = static_cast<float>(m_game.window().getWidth()) - 210.0f;
    float barY       = 10.0f;
    constexpr float barW       = 200.0f;
    constexpr float barH       = 20.0f;
    constexpr float barSpacing = 28.0f;
    float labelX     = barX + barW + 10.0f;

    auto addBar = [&](int value, SDL_Color barColor, const std::string& label) {
        data.bars.push_back({barX, barY, barW, barH,
                             static_cast<float>(value) / 100.0f, barColor});
        // ラベルは barH を考慮して縦中央に配置（テキスト高さ約16pxとして概算）
        data.texts.push_back({"default", label, labelX, barY + (barH - 16.0f) / 2.0f, white});
        barY += barSpacing;
    };

    addBar(stats.hp,     {200,  50,  50, 255}, "HP: "     + std::to_string(stats.hp));
    addBar(stats.mental, { 50, 100, 200, 255}, "Mental: " + std::to_string(stats.mental));
    addBar(stats.hunger, {100, 200,  50, 255}, "Hunger: " + std::to_string(stats.hunger));
    addBar(stats.stress, {150,  50, 200, 255}, "Stress: " + std::to_string(stats.stress));

    // 所持金（バーなしでテキストのみ）
    data.texts.push_back({"default", "Money: " + std::to_string(stats.money), barX, barY, white});

    // 下部: 現在のステート
    std::string stateText = std::string("State: ") + m_player->getCurrentStateName();
    float stateY = static_cast<float>(m_game.window().getHeight()) - 26.0f;
    data.texts.push_back({"default", stateText, 10.0f, stateY, white});
}

} // namespace makai
