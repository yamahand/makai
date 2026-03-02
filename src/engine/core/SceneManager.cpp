#include "SceneManager.hpp"
#include "../scenes/Scene.hpp"
#include "../memory/MemoryManager.hpp"

namespace mk {

void SceneManager::push(std::unique_ptr<Scene> scene) {
    m_pending.push_back({ Command::Push, std::move(scene) });
}

void SceneManager::pop() {
    m_pending.push_back({ Command::Pop, nullptr });
}

void SceneManager::replace(std::unique_ptr<Scene> scene) {
    m_pending.push_back({ Command::Replace, std::move(scene) });
}

void SceneManager::clear(std::unique_ptr<Scene> scene) {
    m_pending.push_back({ Command::Clear, std::move(scene) });
}

Scene* SceneManager::current() const {
    return m_stack.empty() ? nullptr : m_stack.top().get();
}

void SceneManager::handleEvent(const SDL_Event& event) {
    if (auto* s = current()) s->handleEvent(event);
}

void SceneManager::update(float deltaTime) {
    if (auto* s = current()) s->update(deltaTime);
}

void SceneManager::render(SDL_Renderer* renderer) {
    if (auto* s = current()) s->render(renderer);
}

void SceneManager::applyPendingChanges() {
    bool sceneChanged = !m_pending.empty();

    for (auto& change : m_pending) {
        switch (change.command) {
            case Command::Push:
                m_stack.push(std::move(change.scene));
                break;
            case Command::Pop:
                if (!m_stack.empty()) m_stack.pop();
                break;
            case Command::Replace:
                if (!m_stack.empty()) m_stack.pop();
                m_stack.push(std::move(change.scene));
                break;
            case Command::Clear:
                while (!m_stack.empty()) m_stack.pop();
                if (change.scene) m_stack.push(std::move(change.scene));
                break;
        }
    }
    m_pending.clear();

    // シーン変更時にシーンアロケーターをリセット
    if (sceneChanged) {
        memory::MemoryManager::instance().onSceneChange();
    }
}

} // namespace mk
