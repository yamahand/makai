#pragma once
#include "Window.hpp"
#include "SceneManager.hpp"
#include "Config.hpp"
#include "ImGuiManager.hpp"
#include "../utils/FontManager.hpp"
#include "../utils/TextureManager.hpp"
#include "../memory/MemoryManager.hpp"
#include <memory>

namespace mk {

class Game {
public:
    static constexpr int TARGET_FPS = 60;

    Game();
    virtual ~Game();  // サブクラスを安全に破棄できるよう virtual にする

    void init();  // onInit() を呼ぶ + applyPendingChanges()（runApp<T> から構築後に呼ばれる）
    void run();   // init() が呼ばれていない場合は std::logic_error を投げる

    SceneManager&          sceneManager()   { return *m_sceneManager; }
    Window&                window()         { return *m_window; }
    FontManager&           fontManager()    { return *m_fontManager; }
    TextureManager&        textureManager() { return *m_textureManager; }
    memory::MemoryManager& memoryManager()  { return memory::MemoryManager::instance(); }
    const Config&          config()         const { return m_config; }

protected:
    // サブクラスで最初のシーンを push する（コンストラクタ外で呼ばれるため仮想ディスパッチが効く）
    virtual void onInit() {}
    // サブクラスでゲーム固有の ImGui を描画する（プール統計など）
    virtual void onRenderImGui() {}

private:
    void processEvents();
    void update(float deltaTime);
    void render();
    void renderImGui();

    Config                          m_config;
    std::unique_ptr<Window>         m_window;
    std::unique_ptr<ImGuiManager>   m_imguiManager;
    std::unique_ptr<SceneManager>   m_sceneManager;
    std::unique_ptr<FontManager>    m_fontManager;
    std::unique_ptr<TextureManager> m_textureManager;
    bool                            m_running = false;
    bool                            m_initialized = false;  // init() 呼び出し済みフラグ
};

} // namespace mk
