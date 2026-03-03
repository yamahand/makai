#include "Renderer.hpp"

namespace makai {

Renderer::Renderer(SDL_Renderer* sdlRenderer,
                   FontManager&    fontManager,
                   TextureManager& textureManager)
    : m_sdlRenderer(sdlRenderer)
    , m_fontManager(fontManager)
    , m_textureManager(textureManager)
{
}

void Renderer::execute(const SceneRenderData& data) {
    // 背景クリア
    clearBackground(data.clearColor);

    // スプライト描画
    for (const auto& call : data.sprites) {
        drawSprite(call);
    }

    // デバッグ矩形描画
    for (const auto& call : data.debugRects) {
        drawDebugRect(call);
    }

    // ステータスバー描画
    for (const auto& call : data.bars) {
        drawBar(call);
    }

    // テキスト描画
    for (const auto& call : data.texts) {
        drawText(call);
    }
}

void Renderer::clearBackground(SDL_Color color) {
    SDL_SetRenderDrawColor(m_sdlRenderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(m_sdlRenderer);
}

void Renderer::drawSprite(const SpriteDrawCall& call) {
    SDL_FRect dst = {call.x, call.y, call.w, call.h};
    // srcRect は optional のため、値がある場合はそのポインタを、ない場合は nullptr を渡す
    const SDL_FRect* src = call.srcRect.has_value() ? &call.srcRect.value() : nullptr;
    m_textureManager.renderTexture(call.textureName, src, &dst);
}

void Renderer::drawText(const TextDrawCall& call) {
    SDL_Texture* tex = m_fontManager.renderTextTexture(
        m_sdlRenderer, call.fontName, call.text, call.color
    );
    if (!tex) return;

    float w, h;
    SDL_GetTextureSize(tex, &w, &h);

    // 水平中央寄せ: x を中心として左端を算出する
    float drawX = call.alignCenter ? call.x - w / 2.0f : call.x;
    // 縦中央寄せ: y をテキスト中心として上端を算出する
    float drawY = call.alignMiddle ? call.y - h / 2.0f : call.y;
    SDL_FRect dst = {drawX, drawY, w, h};
    SDL_RenderTexture(m_sdlRenderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void Renderer::drawDebugRect(const DebugRectDrawCall& call) {
    SDL_SetRenderDrawColor(m_sdlRenderer,
        call.color.r, call.color.g, call.color.b, call.color.a);
    SDL_FRect rect = {call.x, call.y, call.w, call.h};
    SDL_RenderFillRect(m_sdlRenderer, &rect);
}

void Renderer::drawBar(const BarDrawCall& call) {
    // 背景（暗いグレー）
    SDL_SetRenderDrawColor(m_sdlRenderer, 40, 40, 40, 255);
    SDL_FRect bgRect = {call.x, call.y, call.w, call.h};
    SDL_RenderFillRect(m_sdlRenderer, &bgRect);

    // ゲージ部分（値に応じた幅）
    float fillWidth = (call.w - 4.0f) * call.value;
    if (fillWidth > 0.0f) {
        SDL_SetRenderDrawColor(m_sdlRenderer,
            call.barColor.r, call.barColor.g, call.barColor.b, call.barColor.a);
        SDL_FRect fillRect = {call.x + 2.0f, call.y + 2.0f, fillWidth, call.h - 4.0f};
        SDL_RenderFillRect(m_sdlRenderer, &fillRect);
    }

    // 枠（白）
    SDL_SetRenderDrawColor(m_sdlRenderer, 255, 255, 255, 255);
    SDL_RenderRect(m_sdlRenderer, &bgRect);
}

} // namespace makai
