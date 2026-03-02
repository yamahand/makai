#pragma once
#include <SDL3/SDL.h>
#include <string>

namespace mk {

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    SDL_Window*   getSDLWindow()   const { return m_window; }
    SDL_Renderer* getRenderer()    const { return m_renderer; }
    int           getWidth()       const { return m_width; }
    int           getHeight()      const { return m_height; }

    bool isValid() const { return m_window && m_renderer; }

private:
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    int           m_width;
    int           m_height;
};

} // namespace mk
