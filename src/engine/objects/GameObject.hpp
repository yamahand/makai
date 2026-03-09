#pragma once
#include <SDL3/SDL.h>

namespace mk {

// 全ゲームオブジェクトの基底クラス
class GameObject {
public:
    GameObject(float x = 0.0f, float y = 0.0f)
        : m_x(x), m_y(y) {}
    virtual ~GameObject() = default;

    virtual void handleInput(SDL_Scancode key) {}
    virtual void update(float deltaTime) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;

    bool isAlive() const { return m_alive; }
    void destroy()       { m_alive = false; }

    float getX() const { return m_x; }
    float getY() const { return m_y; }

protected:
    float m_x, m_y;
    bool  m_alive = true;
};

} // namespace mk
