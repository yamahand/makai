#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <memory>

namespace mk {

// TTF_Font の RAII ラッパー
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

// FontManager: フォントの読み込みとテキストレンダリングを管理
class FontManager {
public:
    FontManager();
    ~FontManager();

    // 指定サイズでフォントファイルを読み込む
    bool loadFont(const std::string& name, const std::string& path, int size);

    // 名前から読み込み済みのフォントを取得
    TTF_Font* getFont(const std::string& name) const;

    // テキストを Surface にレンダリング（呼び出し元が解放責任）
    SDL_Surface* renderText(const std::string& fontName, const std::string& text, SDL_Color color);

    // テキストを Texture にレンダリング（呼び出し元が破棄責任）
    SDL_Texture* renderTextTexture(SDL_Renderer* renderer, const std::string& fontName,
                                    const std::string& text, SDL_Color color);

private:
    std::unordered_map<std::string, std::unique_ptr<Font>> m_fonts;
};

} // namespace mk
