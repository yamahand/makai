#include "PlayerStates.hpp"
#include "../objects/Player.hpp"

namespace makai {

// ---- PlayerIdleState ----

void PlayerIdleState::onEnter() {
    m_elapsed = 0.0f;
}

void PlayerIdleState::handleInput(SDL_Scancode key) {
    switch (key) {
        case SDL_SCANCODE_W: // 仕事する
            m_player.stateMachine().changeState(
                std::make_unique<PlayerWorkingState>(m_player));
            break;
        case SDL_SCANCODE_R: // 休む
            m_player.stateMachine().changeState(
                std::make_unique<PlayerRestingState>(m_player));
            break;
        case SDL_SCANCODE_E: // 食事する
            m_player.stateMachine().changeState(
                std::make_unique<PlayerEatingState>(m_player));
            break;
        default:
            break;
    }
}

void PlayerIdleState::update(float deltaTime) {
    m_elapsed += deltaTime;
    // アイドル中も空腹・精神力は緩やかに減少
    // TODO: ゲーム内時刻と連動させる
}

// ---- PlayerWorkingState ----

void PlayerWorkingState::onEnter() {
    m_workTimer = 0.0f;
}

void PlayerWorkingState::update(float deltaTime) {
    m_workTimer += deltaTime;
    // 仕事中: 体力・精神力が減り、お金が増える
    m_player.stats().addHp(-static_cast<int>(deltaTime * 2));
    m_player.stats().addMental(-static_cast<int>(deltaTime * 1));
    m_player.stats().addHunger(-static_cast<int>(deltaTime * 3));

    if (m_workTimer >= m_workDuration) {
        m_player.stateMachine().changeState(
            std::make_unique<PlayerIdleState>(m_player));
    }
}

void PlayerWorkingState::onExit() {
    // 仕事終了時に給料を加算
    m_player.stats().addMoney(500);
}

// ---- PlayerRestingState ----

void PlayerRestingState::onEnter() {
    m_restTimer = 0.0f;
}

void PlayerRestingState::update(float deltaTime) {
    m_restTimer += deltaTime;
    // 休憩中: 体力・精神力が回復
    m_player.stats().addHp(static_cast<int>(deltaTime * 5));
    m_player.stats().addMental(static_cast<int>(deltaTime * 3));
    m_player.stats().addStress(-static_cast<int>(deltaTime * 2));

    if (m_restTimer >= m_restDuration) {
        m_player.stateMachine().changeState(
            std::make_unique<PlayerIdleState>(m_player));
    }
}

void PlayerRestingState::onExit() {}

// ---- PlayerEatingState ----

void PlayerEatingState::onEnter() {
    m_eatTimer = 0.0f;
}

void PlayerEatingState::update(float deltaTime) {
    m_eatTimer += deltaTime;
    if (m_eatTimer >= 1.0f) { // 1秒で食事完了
        m_player.stateMachine().changeState(
            std::make_unique<PlayerIdleState>(m_player));
    }
}

void PlayerEatingState::onExit() {
    // 食事完了で満腹度・体力を回復
    m_player.stats().addHunger(40);
    m_player.stats().addHp(10);
    m_player.stats().addMoney(-300); // 食費
}

} // namespace makai
