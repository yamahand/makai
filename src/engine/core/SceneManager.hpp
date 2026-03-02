#pragma once
#include <memory>
#include <stack>
#include <vector>
#include <SDL3/SDL.h>

namespace mk {

class Scene;

class SceneManager {
public:
    // シーンをスタックに積む（前のシーンはポーズ状態で保持）
    void push(std::unique_ptr<Scene> scene);

    // 現在のシーンを破棄して前のシーンに戻る
    void pop();

    // 現在のシーンを別のシーンに置き換える
    void replace(std::unique_ptr<Scene> scene);

    // 全シーンをクリアして新シーンを積む
    void clear(std::unique_ptr<Scene> scene);

    Scene* current() const;
    bool   isEmpty() const { return m_stack.empty(); }

    void handleEvent(const SDL_Event& event);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);

    // 遅延コマンドを実行（update後に呼ぶ）
    void applyPendingChanges();

private:
    std::stack<std::unique_ptr<Scene>> m_stack;

    // 遷移コマンド（update中にスタックを変更しないための仕組み）
    enum class Command { Push, Pop, Replace, Clear };
    struct PendingChange {
        Command                    command;
        std::unique_ptr<Scene>     scene;
    };
    std::vector<PendingChange> m_pending;
};

} // namespace mk
