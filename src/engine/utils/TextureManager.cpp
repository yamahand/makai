#include "TextureManager.hpp"
#include <SDL3/SDL_log.h>

namespace mk {

TextureManager::TextureManager(SDL_Renderer* renderer)
    : m_renderer(renderer)
{
    // SDL3_image は IMG_Init/IMG_Quit を必要としない
    // 画像フォーマットは IMG_Load 経由で自動的にサポートされる
}

TextureManager::~TextureManager() {
    m_textures.clear();
    // SDL3_image は IMG_Quit を必要としない
}

bool TextureManager::loadTexture(const std::string& name, const std::string& path) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load image %s: %s",
                     path.c_str(), SDL_GetError());
        return false;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_DestroySurface(surface);

    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture from %s: %s",
                     path.c_str(), SDL_GetError());
        return false;
    }

    m_textures[name] = std::make_unique<Texture>(texture);
    SDL_Log("Loaded texture: %s", name.c_str());
    return true;
}

SDL_Texture* TextureManager::getTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second->get();
    }
    return nullptr;
}

void TextureManager::renderTexture(const std::string& name, float x, float y,
                                    float w, float h) {
    SDL_Texture* texture = getTexture(name);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Texture not found: %s", name.c_str());
        return;
    }

    SDL_FRect dstRect;
    dstRect.x = x;
    dstRect.y = y;

    if (w == 0 || h == 0) {
        // テクスチャの元のサイズを使用
        float texW, texH;
        SDL_GetTextureSize(texture, &texW, &texH);
        dstRect.w = (w == 0) ? texW : w;
        dstRect.h = (h == 0) ? texH : h;
    } else {
        dstRect.w = w;
        dstRect.h = h;
    }

    SDL_RenderTexture(m_renderer, texture, nullptr, &dstRect);
}

void TextureManager::renderTexture(const std::string& name, const SDL_FRect* srcRect,
                                    const SDL_FRect* dstRect) {
    SDL_Texture* texture = getTexture(name);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Texture not found: %s", name.c_str());
        return;
    }

    SDL_RenderTexture(m_renderer, texture, srcRect, dstRect);
}

} // namespace mk
