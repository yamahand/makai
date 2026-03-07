#pragma once
#include "engine/core/App.hpp"
#include "game/scenes/TitleScene.hpp"
#include "game/objects/Player.hpp"
#ifndef NDEBUG
#include "game/scenes/DebugBootScene.hpp"
#endif
#include <imgui.h>

namespace makai {

// エンジンの Game を継承したゲーム固有のアプリケーションクラス
class App : public mk::Game {
protected:
    // 最初のシーンを push する
    void onInit() override {
#ifndef NDEBUG
        sceneManager().push(std::make_unique<DebugBootScene>(*this));
#else
        sceneManager().push(std::make_unique<TitleScene>(*this));
#endif
    }

    // ゲーム固有の ImGui 表示（Player プール統計）
    void onRenderImGui() override {
        if (ImGui::Begin("Memory Stats")) {
            ImGui::Separator();

            // プールアロケーター
            ImGui::Text("Object Pools:");
            auto& playerPool = memoryManager().getPool<Player>();
            ImGui::Text("  Player: %zu / %zu (%.1f%%)",
                        playerPool.getUsedCount(),
                        playerPool.getCapacity(),
                        playerPool.getUsageRatio() * 100.0f);
            ImGui::ProgressBar(playerPool.getUsageRatio(), ImVec2(-1, 0));
        }
        ImGui::End();
    }
};

} // namespace makai
