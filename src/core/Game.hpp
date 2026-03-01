#pragma once
#include "Window.hpp"
#include "SceneManager.hpp"
#include "../utils/FontManager.hpp"
#include <memory>

namespace makai {

class Game {
public:
    static constexpr int SCREEN_WIDTH  = 1280;
    static constexpr int SCREEN_HEIGHT = 720;
    static constexpr int TARGET_FPS    = 60;

    Game();
    ~Game();

    void run();

    SceneManager& sceneManager() { return m_sceneManager; }
    Window&       window()       { return *m_window; }
    FontManager&  fontManager()  { return m_fontManager; }

private:
    void processEvents();
    void update(float deltaTime);
    void render();

    std::unique_ptr<Window> m_window;
    SceneManager            m_sceneManager;
    FontManager             m_fontManager;
    bool                    m_running = false;
};

} // namespace makai
