#pragma once
#include "Scene.hpp"
#include "../objects/Player.hpp"
#include <string>

namespace makai {

// ゲームメインシーン
// 「俺に働けって言われても」風：1日のスケジュール・行動選択・ステータス管理
class GameScene : public Scene {
public:
    explicit GameScene(Game& game);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    SceneRenderData collectRenderData() const override;
    void renderImGui() override;

    void onEnter()  override;
    void onExit()   override;

private:
    void updateDayCycle(float deltaTime);
    // HUD描画データを収集してSceneRenderDataに追加する
    void collectHUD(SceneRenderData& data) const;
    // 時刻を HH:MM 形式の文字列で返す
    std::string formatTime() const;

    Player* m_player;  // PoolAllocatorで管理

    // ゲーム内時間管理
    float m_gameTime     = 8.0f;  // 08:00 スタート
    int   m_dayCount     = 1;
    bool  m_isDayOver    = false;

    static constexpr float GAME_HOURS_PER_REAL_SECOND = 0.5f; // 現実1秒 = ゲーム30分
    static constexpr float DAY_END_HOUR               = 24.0f;
};

} // namespace makai
