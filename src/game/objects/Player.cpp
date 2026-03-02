#include "Player.hpp"
#include "../states/PlayerStates.hpp"
#include <algorithm>

namespace makai {

// ---- PlayerStats ----

static int clamp100(int v) { return std::clamp(v, 0, 100); }

void PlayerStats::addHp(int v)     { hp     = clamp100(hp     + v); }
void PlayerStats::addMental(int v) { mental = clamp100(mental + v); }
void PlayerStats::addHunger(int v) { hunger = clamp100(hunger + v); }
void PlayerStats::addMoney(int v)  { money  = std::max(0, money + v); }
void PlayerStats::addStress(int v) { stress = clamp100(stress + v); }

// ---- Player ----

Player::Player()
    : GameObject(100.0f, 300.0f)
{
    // 初期ステート: Idle
    m_stateMachine.changeState(std::make_unique<PlayerIdleState>(*this));
}

void Player::handleInput(SDL_Scancode key) {
    m_stateMachine.handleInput(key);
}

void Player::update(float deltaTime) {
    m_stateMachine.update(deltaTime);

    // 時間経過で空腹・疲労を減少（仮）
    // TODO: ゲーム内時刻と連動させる
}

void Player::render(SDL_Renderer* renderer) {
    // TODO: スプライト描画。現在はデバッグ用矩形
    SDL_FRect rect = { m_x, m_y, 32.0f, 48.0f };
    SDL_SetRenderDrawColor(renderer, 255, 200, 100, 255);
    SDL_RenderFillRect(renderer, &rect);
}

const char* Player::getCurrentStateName() const {
    if (m_stateMachine.currentState()) {
        return m_stateMachine.currentState()->getName();
    }
    return "Unknown";
}

} // namespace makai
