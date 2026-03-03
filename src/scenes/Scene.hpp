#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace makai {

class Game;

// ---- 描画データ構造体（Engineへの描画依頼） ----

// スプライト描画依頼
struct SpriteDrawCall {
    std::string  textureName;          // TextureManager のキー
    float        x, y, w, h;
    const SDL_FRect* srcRect = nullptr; // nullptr でテクスチャ全体を使用
};

// テキスト描画依頼
struct TextDrawCall {
    std::string fontName;               // FontManager のキー
    std::string text;
    float       x, y;
    SDL_Color   color;
    bool        alignCenter = false;    // true のとき x を中心として水平中央寄せ
};

// デバッグ矩形描画依頼（スプライト未設定のオブジェクト用）
struct DebugRectDrawCall {
    float     x, y, w, h;
    SDL_Color color;
};

// ステータスバー描画依頼（背景・ゲージ・枠を一括指定）
struct BarDrawCall {
    float     x, y, w, h;
    float     value;    // 現在値（0.0〜1.0 の比率）
    SDL_Color barColor; // ゲージの色
};

// シーンが1フレームに返す描画データの集合
struct SceneRenderData {
    SDL_Color                    clearColor = {0, 0, 0, 255};
    std::vector<SpriteDrawCall>    sprites;
    std::vector<TextDrawCall>      texts;
    std::vector<DebugRectDrawCall> debugRects;
    std::vector<BarDrawCall>       bars;
};

// ---- Scene 基底クラス ----

// 全シーンの基底クラス（純粋仮想）
// ゲームロジックと描画データの提供のみを担当する。
// SDL_Renderer* を直接受け取らない。
class Scene {
public:
    explicit Scene(Game& game) : m_game(game) {}
    virtual ~Scene() = default;

    virtual void handleEvent(const SDL_Event& event) = 0;
    virtual void update(float deltaTime)              = 0;

    // Engine がフレームごとに呼ぶ。SDL呼び出しなしで描画データを返す。
    virtual SceneRenderData collectRenderData() const = 0;

    // Engine の ImGui フレーム内で呼ばれる。
    // ImGui ウィジェット呼び出しのみ書くこと（newFrame/render は呼ばない）。
    virtual void renderImGui() {}

    // シーンがスタックに積まれたとき
    virtual void onEnter() {}
    // シーンがスタックから外れたとき
    virtual void onExit()  {}
    // 上のシーンが消えて再び最前面に来たとき
    virtual void onResume() {}

protected:
    Game& m_game;
};

} // namespace makai
