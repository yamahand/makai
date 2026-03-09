#pragma once
#include <SDL3/SDL.h>

namespace mk {

class Game;

// 全シーンの基底クラス（純粋仮想）
class Scene {
public:
    explicit Scene(Game& game) : m_game(game) {}
    virtual ~Scene() = default;

    virtual void handleEvent(const SDL_Event& event) = 0;
    virtual void update(float deltaTime)              = 0;
    virtual void render(SDL_Renderer* renderer)       = 0;

    // シーンがスタックに積まれたとき
    virtual void onEnter() {}
    // シーンがスタックから外れたとき
    virtual void onExit()  {}
    // 上のシーンが消えて再び最前面に来たとき
    virtual void onResume() {}

protected:
    Game& m_game;
};

} // namespace mk
