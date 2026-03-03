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

    // 破棄順は宣言の逆順になるため、参照先を先に宣言する
    // m_renderer は m_fontManager / m_textureManager への参照を保持するため、
    // それらより後に宣言し、先に破棄されるようにする
    Config                          m_config;
    std::unique_ptr<Window>         m_window;
    std::unique_ptr<ImGuiManager>   m_imguiManager;
    FontManager                     m_fontManager;        // m_renderer より先に宣言（後に破棄）
    std::unique_ptr<TextureManager> m_textureManager;     // m_renderer より先に宣言（後に破棄）
    std::unique_ptr<Renderer>       m_renderer;           // 上2つの参照を保持するため最後に破棄
    SceneManager                    m_sceneManager;
    bool                            m_running = false;
};

} // namespace makai
