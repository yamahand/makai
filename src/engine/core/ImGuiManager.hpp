#pragma once
#include <SDL3/SDL.h>
#include "Config.hpp"

struct SDL_Window;
struct SDL_Renderer;

namespace mk {

class ImGuiManager {
public:
    ImGuiManager(SDL_Window* window, SDL_Renderer* renderer, const FontConfig& fontConfig);
    ~ImGuiManager();

    // 新しい ImGui フレームを開始する
    void newFrame();

    // ImGui の描画データをレンダリング
    void render();

    // SDL イベントを処理
    bool processEvent(const SDL_Event& event);

    // コピーを禁止
    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;

private:
    SDL_Renderer* m_renderer;
};

} // namespace mk
