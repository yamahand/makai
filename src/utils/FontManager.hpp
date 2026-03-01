#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <memory>

namespace makai {

// RAII wrapper for TTF_Font
class Font {
public:
    Font(TTF_Font* font) : m_font(font) {}
    ~Font() { if (m_font) TTF_CloseFont(m_font); }

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&& other) noexcept : m_font(other.m_font) { other.m_font = nullptr; }
    Font& operator=(Font&& other) noexcept {
        if (this != &other) {
            if (m_font) TTF_CloseFont(m_font);
            m_font = other.m_font;
            other.m_font = nullptr;
        }
        return *this;
    }

    TTF_Font* get() const { return m_font; }
    operator TTF_Font*() const { return m_font; }

private:
    TTF_Font* m_font;
};

// FontManager: Manages font loading and text rendering
class FontManager {
public:
    FontManager();
    ~FontManager();

    // Load a font file with specified size
    bool loadFont(const std::string& name, const std::string& path, int size);

    // Get loaded font by name
    TTF_Font* getFont(const std::string& name) const;

    // Render text to a surface (caller must free the surface)
    SDL_Surface* renderText(const std::string& fontName, const std::string& text, SDL_Color color);

    // Render text to a texture (caller must destroy the texture)
    SDL_Texture* renderTextTexture(SDL_Renderer* renderer, const std::string& fontName,
                                    const std::string& text, SDL_Color color);

private:
    std::unordered_map<std::string, std::unique_ptr<Font>> m_fonts;
};

} // namespace makai
