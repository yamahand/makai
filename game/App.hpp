#pragma once
#include "engine/App.hpp"
#include "scenes/TitleScene.hpp"
#include "objects/Player.hpp"
#ifndef NDEBUG
#include "scenes/DebugBootScene.hpp"
#endif
#include <imgui.h>
#include <memory>

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
        if (ImGui::Begin("Game Memory Stats")) {
            ImGui::Separator();

            // プールアロケーター（生成済みのプールのみ表示）
            ImGui::Text("Object Pools:");
            if (memoryManager().hasPool<Player>()) {
                auto& playerPool = memoryManager().getPool<Player>();
                const float usageRatio = playerPool.getUsageRatio();
                ImGui::Text("  Player: %zu / %zu (%.1f%%)",
                            playerPool.getUsedCount(),
                            playerPool.getCapacity(),
                            usageRatio * 100.0f);
                ImGui::ProgressBar(usageRatio, ImVec2(-1, 0));
            } else {
                ImGui::TextDisabled("  (未生成)");
            }
        }
        ImGui::End();
    }
};

} // namespace makai
