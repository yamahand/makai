#ifndef NDEBUG

#include "LogTestScene.hpp"
#include "DebugBootScene.hpp"
#include "engine/core/Game.hpp"
#include "engine/utils/Logger.hpp"
#include <imgui.h>
#include <format>

namespace makai {

LogTestScene::LogTestScene(mk::Game& game)
    : mk::Scene(game)
{
    mk::Logger::info("LogTestScene に入りました");
}

void LogTestScene::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_ESCAPE) {
        m_game.sceneManager().replace(
            std::make_unique<DebugBootScene>(m_game));
    }
}

void LogTestScene::update(float /*deltaTime*/) {}

void LogTestScene::render(SDL_Renderer* renderer) {
    // 背景
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_RenderClear(renderer);

    // ImGui ウィンドウ
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(760, 540), ImGuiCond_Always);
    ImGui::Begin("ログ出力テスト", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextDisabled("ESC で戻る");
    ImGui::Separator();

    // メッセージ入力
    ImGui::Text("メッセージ:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("##msg", m_inputBuf, sizeof(m_inputBuf));

    ImGui::Spacing();
    ImGui::Text("ログレベルを選択して出力:");

    // 各レベルのボタン
    auto logButton = [&](const char* label, ImVec4 color, auto logFn) {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(label, ImVec2(100, 0))) {
            // 先にインクリメントして評価順序の不定動作を回避
            ++m_counter;
            std::string msg = m_inputBuf[0] != '\0'
                ? std::format("[{}] {}", m_counter, m_inputBuf)
                : std::format("[{}] (テストメッセージ {})", m_counter, m_counter);
            logFn(msg);
            m_entries.push_back({ label, msg });
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    };

    logButton("Trace",    ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        [](const std::string& m) { mk::Logger::trace("{}", m); });
    logButton("Debug",    ImVec4(0.2f, 0.6f, 0.8f, 1.0f),
        [](const std::string& m) { mk::Logger::debug("{}", m); });
    logButton("Info",     ImVec4(0.2f, 0.7f, 0.2f, 1.0f),
        [](const std::string& m) { mk::Logger::info("{}", m); });
    logButton("Warn",     ImVec4(0.8f, 0.6f, 0.1f, 1.0f),
        [](const std::string& m) { mk::Logger::warn("{}", m); });
    logButton("Error",    ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
        [](const std::string& m) { mk::Logger::error("{}", m); });
    logButton("Critical", ImVec4(0.9f, 0.1f, 0.4f, 1.0f),
        [](const std::string& m) { mk::Logger::critical("{}", m); });

    ImGui::NewLine();
    ImGui::Separator();

    // 出力履歴
    ImGui::Text("出力履歴 (%zu 件):", m_entries.size());
    if (ImGui::Button("クリア")) m_entries.clear();

    ImGui::BeginChild("##log_scroll", ImVec2(0, 0), true);
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        ImVec4 col = ImVec4(1, 1, 1, 1);
        if (it->level == "Trace")    col = ImVec4(0.6f, 0.6f, 0.6f, 1);
        else if (it->level == "Debug")    col = ImVec4(0.4f, 0.8f, 1.0f, 1);
        else if (it->level == "Warn")     col = ImVec4(1.0f, 0.8f, 0.2f, 1);
        else if (it->level == "Error")    col = ImVec4(1.0f, 0.4f, 0.4f, 1);
        else if (it->level == "Critical") col = ImVec4(1.0f, 0.2f, 0.6f, 1);
        ImGui::TextColored(col, "[%s] %s", it->level.c_str(), it->message.c_str());
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace makai

#endif // NDEBUG
