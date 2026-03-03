#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace makai {

// オブジェクトの見た目情報（Engineが描画に使用する）
struct SpriteInfo {
    std::string textureName;                          // 空文字列 → debugRect として描画
    float       w          = 32.0f;
    float       h          = 48.0f;
    SDL_Color   debugColor = {255, 200, 100, 255};    // textureName未設定時のデバッグ色
};

// 全ゲームオブジェクトの基底クラス
// ゲームロジックと見た目情報の提供のみを担当する。
// SDL_Renderer* を直接受け取らない。
class GameObject {
public:
    GameObject(float x = 0.0f, float y = 0.0f)
        : m_x(x), m_y(y) {}
    virtual ~GameObject() = default;

    virtual void handleInput(SDL_Scancode key) {}
    virtual void update(float deltaTime) = 0;

    // Engine に「どう見えるか」を伝える
    virtual SpriteInfo getSpriteInfo() const = 0;

    bool isAlive() const { return m_alive; }
    void destroy()       { m_alive = false; }

    float getX() const { return m_x; }
    float getY() const { return m_y; }

protected:
    float m_x, m_y;
    bool  m_alive = true;
};

} // namespace makai
