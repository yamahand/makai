#pragma once
#ifndef NDEBUG

#include "engine/scenes/Scene.hpp"
#include <functional>
#include <string>
#include <vector>

namespace makai {

// デバッグビルド専用：起動シーン選択画面
class DebugBootScene : public mk::Scene {
public:
    explicit DebugBootScene(mk::Game& game);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    struct SceneEntry {
        std::string name;
        std::function<void()> launch; // シーン遷移を実行するコールバック
    };

    std::vector<SceneEntry> m_entries;
    int m_selectedIndex = 0;
};

} // namespace makai

#endif // NDEBUG
