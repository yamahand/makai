#include "FontManager.hpp"
#include <SDL3/SDL_log.h>

namespace mk {

FontManager::FontManager() {
    if (!TTF_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: %s", SDL_GetError());
    }
}

FontManager::~FontManager() {
    m_fonts.clear();
    TTF_Quit();
}

bool FontManager::loadFont(const std::string& name, const std::string& path, int size) {
    TTF_Font* font = TTF_OpenFont(path.c_str(), size);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load font %s: %s", path.c_str(), SDL_GetError());
        return false;
    }

    m_fonts[name] = std::make_unique<Font>(font);
    SDL_Log("Loaded font: %s (size %d)", name.c_str(), size);
    return true;
}

TTF_Font* FontManager::getFont(const std::string& name) const {
    auto it = m_fonts.find(name);
    if (it != m_fonts.end()) {
        return it->second->get();
    }
    return nullptr;
}

SDL_Surface* FontManager::renderText(const std::string& fontName, const std::string& text, SDL_Color color) {
    TTF_Font* font = getFont(fontName);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Font not found: %s", fontName.c_str());
        return nullptr;
    }

    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.length(), color);
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to render text: %s", SDL_GetError());
    }
    return surface;
}

SDL_Texture* FontManager::renderTextTexture(SDL_Renderer* renderer, const std::string& fontName,
                                             const std::string& text, SDL_Color color) {
    SDL_Surface* surface = renderText(fontName, text, color);
    if (!surface) {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s", SDL_GetError());
    }
    return texture;
}

} // namespace mk
