#include "Game.hpp"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <stdexcept>
#include <string>

namespace mk {

Game::Game() {
    // 設定ファイルを読み込む
    m_config = Config::load("config.json");

    // メモリマネージャーを最初に初期化（マスターバッファを確保）
    if (!memory::MemoryManager::init(m_config.memory)) {
        throw std::runtime_error("MemoryManager initialization failed");
    }

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
}

void Game::init() {
    // 二重呼び出しを防ぐ（init() は一度だけ呼べる）
    if (m_initialized) {
        return;
    }

    // 再入を防ぐため、onInit() を呼ぶ前に初期化済みフラグを立てる
    m_initialized = true;

    try {
        // 最初のシーンをセット（サブクラスの onInit() で実行される）
        onInit();
        m_sceneManager.applyPendingChanges();
    } catch (...) {
        // 初期化中に例外が発生した場合は、再初期化できるようフラグを戻す
        m_initialized = false;
        throw;
    }
}

Game::~Game() {
    SDL_Quit();
}

void Game::run() {
    if (!m_initialized) {
        throw std::logic_error("Game::run() called before Game::init()");
    }
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

        // ヒープアロケーター（FreeList）
        ImGui::Text("Heap Allocator (FreeList):");
        ImGui::Text("  %.2f / %.2f MB (%.1f%%)",
                    stats.heapBytes / (1024.0f * 1024.0f),
                    stats.heapCapacity / (1024.0f * 1024.0f),
                    stats.heapUsageRatio * 100.0f);
        ImGui::ProgressBar(stats.heapUsageRatio, ImVec2(-1, 0));
        ImGui::Text("  Allocs: %zu  Free blocks: %zu",
                    stats.heapAllocationCount, stats.heapFreeBlockCount);

        ImGui::Separator();

        // 統計
        ImGui::Text("Total Allocations:");
        ImGui::Text("  Frame: %zu", stats.totalFrameAllocations);
        ImGui::Text("  Scene: %zu", stats.totalSceneAllocations);

    }
    ImGui::End();

    // ゲーム側の追加 ImGui 表示（プール統計など）
    onRenderImGui();

    m_imguiManager->render();
}

} // namespace mk
