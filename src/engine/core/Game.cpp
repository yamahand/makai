#include "Game.hpp"
#include "game/scenes/TitleScene.hpp"
#include "game/objects/Player.hpp"
#ifndef NDEBUG
#include "game/scenes/DebugBootScene.hpp"
#endif
#include <SDL3/SDL.h>
#include <imgui.h>
#include <stdexcept>

namespace mk {

Game::Game() {
    // 設定ファイルを読み込む
    m_config = Config::load("config.json");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    // 設定からウィンドウを生成
    m_window = std::make_unique<Window>(
        m_config.window.title,
        m_config.window.width,
        m_config.window.height
    );

    // ImGui を初期化
    m_imguiManager = std::make_unique<ImGuiManager>(
        m_window->getSDLWindow(),
        m_window->getRenderer()
    );

    // TextureManager を初期化
    m_textureManager = std::make_unique<TextureManager>(m_window->getRenderer());

    // 設定からフォントを読み込む
    m_fontManager.loadFont("default",
                          m_config.defaultFont.path,
                          m_config.defaultFont.size);
    m_fontManager.loadFont("large",
                          m_config.largeFont.path,
                          m_config.largeFont.size);

    // テクスチャを読み込む
    m_textureManager->loadTexture("test", "assets/textures/CustomUVChecker_byValle_1K.png");

    // 最初のシーンをセット（デバッグビルドではシーン選択画面を表示）
#ifndef NDEBUG
    m_sceneManager.push(std::make_unique<makai::DebugBootScene>(*this));
#else
    m_sceneManager.push(std::make_unique<makai::TitleScene>(*this));
#endif
    m_sceneManager.applyPendingChanges();
}

Game::~Game() {
    SDL_Quit();
}

void Game::run() {
    m_running = true;
    Uint64 prev = SDL_GetTicks();

    while (m_running) {
        Uint64 now   = SDL_GetTicks();
        float  delta = (now - prev) / 1000.0f;
        if (delta > 0.05f) delta = 0.05f; // 最大deltaTime制限（デバッグ用）
        prev = now;

        processEvents();
        update(delta);
        render();

        m_sceneManager.applyPendingChanges();

        // シーンが空になったら終了
        if (m_sceneManager.isEmpty()) m_running = false;
    }
}

void Game::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // ImGui にイベント処理を優先させる
        m_imguiManager->processEvent(event);

        if (event.type == SDL_EVENT_QUIT) {
            m_running = false;
        }
        m_sceneManager.handleEvent(event);
    }
}

void Game::update(float deltaTime) {
    m_sceneManager.update(deltaTime);
}

void Game::render() {
    auto* renderer = m_window->getRenderer();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // ImGui フレームを開始（シーンの render より前に呼ぶ必要がある）
    m_imguiManager->newFrame();

    m_sceneManager.render(renderer);

    // ImGui のゲーム共通UIをレンダリング
    renderImGui();

    SDL_RenderPresent(renderer);

    // フレームアロケーターをリセット
    memoryManager().onFrameEnd();
}

void Game::renderImGui() {
    // 設定に応じて ImGui デモウィンドウを表示
    if (m_config.debug.showImGuiDemo) {
        ImGui::ShowDemoWindow(&m_config.debug.showImGuiDemo);
    }

    // 設定に応じて FPS カウンターを表示
    if (m_config.debug.showFPS) {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debug Info", nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoMove);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
    }

    // メモリ統計表示
    ImGui::SetNextWindowPos(ImVec2(10, 50), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Memory Stats", nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
        auto stats = memoryManager().getStats();

        // フレームアロケーター
        ImGui::Text("Frame Allocator:");
        ImGui::Text("  %.2f / %.2f MB (%.1f%%)",
                    stats.frameBytes / (1024.0f * 1024.0f),
                    stats.frameCapacity / (1024.0f * 1024.0f),
                    stats.frameUsageRatio * 100.0f);
        ImGui::ProgressBar(stats.frameUsageRatio, ImVec2(-1, 0));

        ImGui::Separator();

        // シーンアロケーター
        ImGui::Text("Scene Allocator:");
        ImGui::Text("  %.2f / %.2f MB (%.1f%%)",
                    stats.sceneBytes / (1024.0f * 1024.0f),
                    stats.sceneCapacity / (1024.0f * 1024.0f),
                    stats.sceneUsageRatio * 100.0f);
        ImGui::ProgressBar(stats.sceneUsageRatio, ImVec2(-1, 0));

        ImGui::Separator();

        // 統計
        ImGui::Text("Total Allocations:");
        ImGui::Text("  Frame: %zu", stats.totalFrameAllocations);
        ImGui::Text("  Scene: %zu", stats.totalSceneAllocations);

        ImGui::Separator();

        // プールアロケーター
        ImGui::Text("Object Pools:");
        auto& playerPool = memoryManager().getPool<makai::Player>();
        ImGui::Text("  Player: %zu / %zu (%.1f%%)",
                    playerPool.getUsedCount(),
                    playerPool.getCapacity(),
                    playerPool.getUsageRatio() * 100.0f);
        ImGui::ProgressBar(playerPool.getUsageRatio(), ImVec2(-1, 0));
    }
    ImGui::End();

    m_imguiManager->render();
}

} // namespace mk
