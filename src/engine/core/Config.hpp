#pragma once
#include <string>

namespace mk {

// ウィンドウ設定
struct WindowConfig {
    std::string title = "makai";      // ウィンドウタイトル
    int width = 1280;                   // ウィンドウ幅（ピクセル）
    int height = 720;                   // ウィンドウ高さ（ピクセル）
    bool fullscreen = false;            // フルスクリーンモード
    bool vsync = true;                  // 垂直同期を有効にするか
};

// フォント設定
struct FontConfig {
    std::string path;       // フォントファイルのパス
    int size = 16;          // フォントサイズ
};

// メモリ設定
struct MemoryConfig {
    int frameAllocatorMB       = 4;   // フレームアロケーターのサイズ（MB）
    int sceneAllocatorMB       = 16;  // シーンアロケーターのサイズ（MB）
    int heapAllocatorMB        = 32;  // ヒープアロケーターのサイズ（MB）
    int doubleFrameAllocatorMB = 4;   // ダブルフレームアロケーター 1 バッファ分のサイズ（MB）
};

// ゲーム設定
struct GameConfig {
    float timeScale = 0.5f;         // ゲーム内時刻のスケール（実秒 × this = ゲーム内時間）
    int startingMoney = 10000;      // 初期所持金
    int startingDay = 1;            // 開始日数
};

// デバッグ設定
struct DebugConfig {
    bool showFPS = true;            // FPS表示を有効にするか
    bool showImGuiDemo = false;     // ImGuiデモウィンドウを表示するか
};

class Config {
public:
    WindowConfig window;
    FontConfig defaultFont;
    FontConfig largeFont;
    MemoryConfig memory;
    GameConfig game;
    DebugConfig debug;

    // JSONファイルから設定を読み込む
    static Config load(const std::string& path);

    // JSONファイルに設定を保存する
    void save(const std::string& path) const;

    Config() = default;
};

} // namespace mk
