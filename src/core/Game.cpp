#include "Game.hpp"
#include "../scenes/TitleScene.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>

namespace makai {

Game::Game() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    m_window = std::make_unique<Window>("makai", SCREEN_WIDTH, SCREEN_HEIGHT);

    // 最初のシーンをセット
    m_sceneManager.push(std::make_unique<TitleScene>(*this));
    m_sceneManager.applyPendingChanges();
}

Game::~Game() {
    SDL_Quit();
}

void Game::run() {
    m_running = true;
    Uint64 prev = SDL_GetTicks();

    while (m_running) {
        Uint64 now   = SDL_GetTicks();
        float  delta = (now - prev) / 1000.0f;
        if (delta > 0.05f) delta = 0.05f; // 最大deltaTime制限（デバッグ用）
        prev = now;

        processEvents();
        update(delta);
        render();

        m_sceneManager.applyPendingChanges();

        // シーンが空になったら終了
        if (m_sceneManager.isEmpty()) m_running = false;
    }
}

void Game::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            m_running = false;
        }
        m_sceneManager.handleEvent(event);
    }
}

void Game::update(float deltaTime) {
    m_sceneManager.update(deltaTime);
}

void Game::render() {
    auto* renderer = m_window->getRenderer();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    m_sceneManager.render(renderer);

    SDL_RenderPresent(renderer);
}

} // namespace makai
