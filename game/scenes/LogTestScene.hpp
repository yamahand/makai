#pragma once
#ifndef NDEBUG

#include "engine/scene/Scene.hpp"
#include <string>
#include <vector>

namespace makai {

// デバッグビルド専用：ログ出力テストシーン
class LogTestScene : public mk::Scene {
public:
    explicit LogTestScene(mk::Game& game);

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    struct LogEntry {
        std::string level;
        std::string message;
    };

    char                    m_inputBuf[256] = {};  // ImGui 入力バッファ
    std::vector<LogEntry>   m_entries;              // 今回のセッションで出力したログ
    int                     m_counter = 0;          // ログカウンター
};

} // namespace makai

#endif // NDEBUG
