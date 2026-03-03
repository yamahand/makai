#pragma once
#include "Window.hpp"
#include "SceneManager.hpp"
#include "Renderer.hpp"
#include "Config.hpp"
#include "ImGuiManager.hpp"
#include "../utils/FontManager.hpp"
#include "../utils/TextureManager.hpp"
#include "../memory/MemoryManager.hpp"
#include <memory>

namespace makai {

class Game {
public:
    static constexpr int TARGET_FPS = 60;

    Game();
    ~Game();

    void run();

    SceneManager&          sceneManager()   { return m_sceneManager; }
    Window&                window()         { return *m_window; }
    FontManager&           fontManager()    { return m_fontManager; }
    TextureManager&        textureManager() { return *m_textureManager; }
    memory::MemoryManager& memoryManager()  { return memory::MemoryManager::instance(); }
    const Config&          config()         const { return m_config; }

private:
    void processEvents();
    void update(float deltaTime);
    void render();
    void renderImGui();
    // Engine固有のデバッグUI（FPS・メモリ統計）を描画する
    void renderDebugImGui();

    Config                          m_config;
    std::unique_ptr<Window>         m_window;
    std::unique_ptr<ImGuiManager>   m_imguiManager;
    std::unique_ptr<Renderer>       m_renderer;
    SceneManager                    m_sceneManager;
    FontManager                     m_fontManager;
    std::unique_ptr<TextureManager> m_textureManager;
    bool                            m_running = false;
};

} // namespace makai
