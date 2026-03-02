#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <string>
#include <unordered_map>
#include <memory>

namespace makai {

// SDL_Texture の RAII ラッパー
class Texture {
public:
    Texture(SDL_Texture* texture) : m_texture(texture) {}
    ~Texture() { if (m_texture) SDL_DestroyTexture(m_texture); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept : m_texture(other.m_texture) { other.m_texture = nullptr; }
    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            if (m_texture) SDL_DestroyTexture(m_texture);
            m_texture = other.m_texture;
            other.m_texture = nullptr;
        }
        return *this;
    }

    SDL_Texture* get() const { return m_texture; }
    operator SDL_Texture*() const { return m_texture; }

private:
    SDL_Texture* m_texture;
};

// TextureManager: テクスチャの読み込みと描画を管理
class TextureManager {
public:
    TextureManager(SDL_Renderer* renderer);
    ~TextureManager();

    // テクスチャファイルを読み込む
    bool loadTexture(const std::string& name, const std::string& path);

    // 名前から読み込み済みのテクスチャを取得
    SDL_Texture* getTexture(const std::string& name) const;

    // テクスチャを指定位置にオプションサイズで描画
    void renderTexture(const std::string& name, float x, float y,
                       float w = 0, float h = 0);

    // テクスチャをソースと宛先矩形で描画
    void renderTexture(const std::string& name, const SDL_FRect* srcRect,
                       const SDL_FRect* dstRect);

private:
    SDL_Renderer* m_renderer;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
};

} // namespace makai
