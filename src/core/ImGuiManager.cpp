#include "ImGuiManager.hpp"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

namespace makai {

ImGuiManager::ImGuiManager(SDL_Window* window, SDL_Renderer* renderer)
    : m_renderer(renderer)
{
    // Dear ImGui コンテキストをセットアップ
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // キーボード操作を有効化
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // ゲームパッド操作を有効化

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

} // namespace makai
