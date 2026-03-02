#include "Game.hpp"
#include "../scenes/TitleScene.hpp"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <stdexcept>

namespace makai {

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

    // 最初のシーンをセット
    m_sceneManager.push(std::make_unique<TitleScene>(*this));
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

    m_sceneManager.render(renderer);

    // ImGui をレンダリング
    renderImGui();

    SDL_RenderPresent(renderer);

    // フレームアロケーターをリセット
    memoryManager().onFrameEnd();
}

void Game::renderImGui() {
    m_imguiManager->newFrame();

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
    }
    ImGui::End();

    m_imguiManager->render();
}

} // namespace makai
