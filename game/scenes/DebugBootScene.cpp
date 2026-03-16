#ifndef NDEBUG

#include "DebugBootScene.hpp"
#include "TitleScene.hpp"
#include "GameScene.hpp"
#include "MemoryTestScene.hpp"
#include "LogTestScene.hpp"
#include "engine/Game.hpp"
#include <imgui.h>

namespace makai {

DebugBootScene::DebugBootScene(mk::Game& game)
    : mk::Scene(game)
{
    // 起動可能なシーン一覧を登録
    m_entries.push_back({
        "TitleScene",
        [this]() {
            m_game.sceneManager().replace(std::make_unique<TitleScene>(m_game));
        }
    });
    m_entries.push_back({
        "GameScene",
        [this]() {
            m_game.sceneManager().replace(std::make_unique<GameScene>(m_game));
        }
    });
    m_entries.push_back({
        "MemoryTestScene",
        [this]() {
            m_game.sceneManager().replace(std::make_unique<MemoryTestScene>(m_game));
        }
    });
    m_entries.push_back({
        "LogTestScene",
        [this]() {
            m_game.sceneManager().replace(std::make_unique<LogTestScene>(m_game));
        }
    });
}

void DebugBootScene::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        switch (event.key.scancode) {
            case SDL_SCANCODE_UP:
                if (m_selectedIndex > 0) --m_selectedIndex;
                break;
            case SDL_SCANCODE_DOWN:
                if (m_selectedIndex < static_cast<int>(m_entries.size()) - 1)
                    ++m_selectedIndex;
                break;
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_SPACE:
                m_entries[m_selectedIndex].launch();
                break;
            default:
                break;
        }
    }
}

void DebugBootScene::update(float /*deltaTime*/) {}

void DebugBootScene::render(SDL_Renderer* renderer) {
    // 背景
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    // ImGui でシーン選択UIを表示
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
    ImGui::Begin("## DebugBoot", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize   |
                 ImGuiWindowFlags_NoMove     |
                 ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[DEBUG] シーン選択");
    ImGui::TextDisabled("↑↓ で選択、Enter/Space で起動");
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        bool selected = (i == m_selectedIndex);
        if (ImGui::Selectable(m_entries[i].name.c_str(), selected)) {
            m_selectedIndex = i;
            m_entries[i].launch();
        }
        if (selected) {
            ImGui::SetItemDefaultFocus();
        }
    }

    ImGui::End();
}

} // namespace makai

#endif // NDEBUG
