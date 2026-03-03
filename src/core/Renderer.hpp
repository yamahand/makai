#pragma once
#include "../scenes/Scene.hpp"
#include "../utils/FontManager.hpp"
#include "../utils/TextureManager.hpp"
#include <SDL3/SDL.h>

namespace makai {

// Engine側の描画実行クラス
// SceneRenderData を受け取り、実際の SDL 描画命令を発行する。
// ゲームロジック（Scene/GameObject）は SDL_Renderer* を直接知らない。
class Renderer {
public:
    Renderer(SDL_Renderer* sdlRenderer,
             FontManager&    fontManager,
             TextureManager& textureManager);

    // シーンから受け取った描画データをすべて描画する
    void execute(const SceneRenderData& data);

private:
    // 背景をクリアする
    void clearBackground(SDL_Color color);

    // スプライトを描画する
    void drawSprite(const SpriteDrawCall& call);

    // テキストを描画する（alignCenter が true のとき水平中央寄せ）
    void drawText(const TextDrawCall& call);

    // デバッグ矩形を描画する（テクスチャ未設定オブジェクト用）
    void drawDebugRect(const DebugRectDrawCall& call);

    // ステータスバーを描画する（背景・ゲージ・枠）
    void drawBar(const BarDrawCall& call);

    SDL_Renderer*   m_sdlRenderer;
    FontManager&    m_fontManager;
    TextureManager& m_textureManager;
};

} // namespace makai
