#include "Window.hpp"
#include <stdexcept>

namespace makai {

Window::Window(const std::string& title, int width, int height)
    : m_width(width), m_height(height)
{
    m_window = SDL_CreateWindow(title.c_str(), width, height, 0);
    if (!m_window) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        SDL_DestroyWindow(m_window);
        throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    }
}

Window::~Window()
{
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window)   SDL_DestroyWindow(m_window);
}

} // namespace makai
