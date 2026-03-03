#pragma once
#include "GameObject.hpp"
#include "../states/StateMachine.hpp"

namespace makai {

// プレイヤーステータス（俺働き風）
struct PlayerStats {
    int hp        = 100; // 体力
    int mental    = 100; // 精神力
    int hunger    = 100; // 満腹度（0で空腹）
    int money     = 0;   // 所持金
    int stress    = 0;   // ストレス
    int day       = 1;   // 経過日数

    // ステータス加減算（0〜100にクランプ）
    void addHp(int v);
    void addMental(int v);
    void addHunger(int v);
    void addMoney(int v);
    void addStress(int v);
};

class Player : public GameObject {
public:
    Player();

    void handleInput(SDL_Scancode key) override;
    void update(float deltaTime) override;
    SpriteInfo getSpriteInfo() const override;

    PlayerStats&       stats()       { return m_stats; }
    const PlayerStats& stats() const { return m_stats; }

    StateMachine& stateMachine() { return m_stateMachine; }

    // HUD \u8868\u793a\u7528\u306b\u73fe\u5728\u306e\u30b9\u30c6\u30fc\u30c8\u540d\u3092\u53d6\u5f97
    const char* getCurrentStateName() const;

private:
    PlayerStats  m_stats;
    StateMachine m_stateMachine;
};

} // namespace makai
