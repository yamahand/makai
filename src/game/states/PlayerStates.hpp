#pragma once
#include "engine/states/StateMachine.hpp"

namespace makai {

class Player;

// ---- Idle（待機中・行動選択待ち） ----
class PlayerIdleState : public mk::State {
public:
    explicit PlayerIdleState(Player& player) : m_player(player) {}
    void onEnter() override;
    void handleInput(SDL_Scancode key) override;
    void update(float deltaTime) override;
    const char* getName() const override { return "Idle"; }
private:
    Player& m_player;
    float   m_elapsed = 0.0f;
};

// ---- Working（アルバイト中） ----
class PlayerWorkingState : public mk::State {
public:
    explicit PlayerWorkingState(Player& player) : m_player(player) {}
    void onEnter() override;
    void update(float deltaTime) override;
    void onExit() override;
    const char* getName() const override { return "Working"; }
private:
    Player& m_player;
    float   m_workTimer  = 0.0f;
    float   m_workDuration = 4.0f; // ゲーム内4時間
};

// ---- Resting（休憩・睡眠） ----
class PlayerRestingState : public mk::State {
public:
    explicit PlayerRestingState(Player& player) : m_player(player) {}
    void onEnter() override;
    void update(float deltaTime) override;
    void onExit() override;
    const char* getName() const override { return "Resting"; }
private:
    Player& m_player;
    float   m_restTimer    = 0.0f;
    float   m_restDuration = 2.0f;
};

// ---- Eating（食事中） ----
class PlayerEatingState : public mk::State {
public:
    explicit PlayerEatingState(Player& player) : m_player(player) {}
    void onEnter() override;
    void update(float deltaTime) override;
    void onExit() override;
    const char* getName() const override { return "Eating"; }
private:
    Player& m_player;
    float   m_eatTimer = 0.0f;
};

} // namespace makai
