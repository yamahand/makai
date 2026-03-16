#pragma once
#include "engine/scene/Scene.hpp"

namespace makai {

class TitleScene : public mk::Scene {
public:
    explicit TitleScene(mk::Game& game);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    float m_timer = 0.0f; // 点滅などの演出用
};

} // namespace makai
