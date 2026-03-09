#include "ImGuiManager.hpp"
#include "../utils/Logger.hpp"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <filesystem>

namespace mk {

ImGuiManager::ImGuiManager(SDL_Window* window, SDL_Renderer* renderer, const FontConfig& fontConfig)
    : m_renderer(renderer)
{
    // Dear ImGui コンテキストをセットアップ
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // キーボード操作を有効化
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // ゲームパッド操作を有効化

    // 日本語フォントを登録（パスが空またはファイル未存在の場合はデフォルトフォントにフォールバック）
    if (fontConfig.path.empty()) {
        Logger::warn("ImGuiManager: フォントパスが空のため、デフォルトフォントを使用します（日本語非対応）");
    } else if (!std::filesystem::exists(fontConfig.path)) {
        Logger::warn("ImGuiManager: フォントファイルが見つかりません: {} デフォルトフォントを使用します（日本語非対応）",
                     fontConfig.path);
    } else {
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            fontConfig.path.c_str(),
            static_cast<float>(fontConfig.size),
            nullptr,
            io.Fonts->GetGlyphRangesJapanese()
        );
        if (font == nullptr) {
            Logger::error("ImGuiManager: フォントの読み込みに失敗しました: {} デフォルトフォントを使用します（日本語非対応）",
                          fontConfig.path);
        } else {
            Logger::info("ImGuiManager: 日本語フォントを読み込みました: {}", fontConfig.path);
        }
    }

    // Dear ImGui スタイルをセットアップ
    ImGui::StyleColorsDark();

    // プラットフォーム / レンダラーバックエンドをセットアップ
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
}

ImGuiManager::~ImGuiManager() {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiManager::newFrame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::render() {
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
}

bool ImGuiManager::processEvent(const SDL_Event& event) {
    return ImGui_ImplSDL3_ProcessEvent(&event);
}

} // namespace mk
