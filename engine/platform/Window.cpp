#include "Window.hpp"
#include "../core/log/Assert.hpp"

namespace mk {

Window::Window(const std::string& title, int width, int height)
    : m_width(width), m_height(height)
{
    m_window = SDL_CreateWindow(title.c_str(), width, height, 0);
    MK_VERIFY_MSG(m_window, "SDL_CreateWindow failed: {}", SDL_GetError());

    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    MK_VERIFY_MSG(m_renderer, "SDL_CreateRenderer failed: {}", SDL_GetError());
}

Window::~Window()
{
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window)   SDL_DestroyWindow(m_window);
}

} // namespace mk
