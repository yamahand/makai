#include "ImGuiManager.hpp"
#include "../utils/Logger.hpp"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>
#include "Config.hpp"

namespace mk {

ImGuiManager::ImGuiManager(SDL_Window* window, SDL_Renderer* renderer, const FontConfig& fontConfig)
    : m_renderer(renderer)
{
    // Dear ImGui コンテキストをセットアップ
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // キーボード操作を有効化
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // ゲームパッド操作を有効化

    // 日本語フォントを登録（パスが空・ファイル未存在・サイズ不正の場合はデフォルトフォントにフォールバック）
    if (fontConfig.path.empty()) {
        CORE_WARN("ImGuiManager: フォントパスが空のため、デフォルトフォントを使用します（日本語非対応）");
    } else if (fontConfig.size <= 0) {
        CORE_WARN("ImGuiManager: フォントサイズが不正です（{}）、デフォルトフォントを使用します（日本語非対応）",
                     fontConfig.size);
    } else {
        // Windows 環境で日本語パスを正しく扱うため、UTF-8 文字列として filesystem::path を構築する
        // reinterpret_cast を避け、std::u8string 経由で安全に変換する
        const std::u8string u8str(fontConfig.path.cbegin(), fontConfig.path.cend());
        const std::filesystem::path fontPath(u8str);
        std::error_code ec;
        const bool fileExists = std::filesystem::exists(fontPath, ec);
        if (ec) {
            CORE_WARN("ImGuiManager: フォントファイルの確認中にエラーが発生しました: {} ({}) デフォルトフォントを使用します（日本語非対応）",
                         fontConfig.path, ec.message());
        } else if (!fileExists) {
            CORE_WARN("ImGuiManager: フォントファイルが見つかりません: {} デフォルトフォントを使用します（日本語非対応）",
                         fontConfig.path);
        } else {
            // Windows で日本語パスを含むフォントファイルを開くため、
            // fopen ベースの AddFontFromFileTTF を使わず、
            // filesystem::path 経由でファイルを読み込んで AddFontFromMemoryTTF に渡す
            std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
            if (!file) {
                CORE_WARN("ImGuiManager: フォントファイルを開けませんでした: {} デフォルトフォントを使用します（日本語非対応）",
                             fontConfig.path);
            } else {
                const auto fileSize = file.tellg();
                // AddFontFromMemoryTTF の data_size は int のため INT_MAX、
                // IM_ALLOC は size_t のため SIZE_MAX を上限としてチェックする
                constexpr auto kMaxFontSize = static_cast<std::streamoff>(
                    std::numeric_limits<int>::max()
                );
                if (fileSize <= 0) {
                    CORE_WARN("ImGuiManager: フォントファイルのサイズが不正です: {} デフォルトフォントを使用します（日本語非対応）",
                                 fontConfig.path);
                } else if (fileSize > kMaxFontSize) {
                    CORE_WARN("ImGuiManager: フォントファイルのサイズが大きすぎます（{} bytes）: {} デフォルトフォントを使用します（日本語非対応）",
                                 static_cast<std::intmax_t>(fileSize), fontConfig.path);
                } else {
                    file.seekg(0, std::ios::beg);
                    // ImGui がアトラスのビルド後に解放するバッファを IM_ALLOC で確保する
                    void* fontData = IM_ALLOC(static_cast<size_t>(fileSize));
                    if (fontData == nullptr) {
                        CORE_WARN("ImGuiManager: フォントデータ用メモリの確保に失敗しました: {} デフォルトフォントを使用します（日本語非対応）",
                                     fontConfig.path);
                    } else if (!file.read(static_cast<char*>(fontData), fileSize)) {
                        IM_FREE(fontData);
                        CORE_WARN("ImGuiManager: フォントファイルの読み込みに失敗しました: {} デフォルトフォントを使用します（日本語非対応）",
                                     fontConfig.path);
                    } else {
                        ImFont* font = io.Fonts->AddFontFromMemoryTTF(
                            fontData,
                            static_cast<int>(fileSize),
                            static_cast<float>(fontConfig.size),
                            nullptr,
                            io.Fonts->GetGlyphRangesJapanese()
                        );
                        if (font == nullptr) {
                            IM_FREE(fontData);
                            CORE_WARN("ImGuiManager: フォントの登録に失敗しました: {} デフォルトフォントを使用します（日本語非対応）",
                                         fontConfig.path);
                        } else {
                            CORE_INFO("ImGuiManager: 日本語フォントを読み込みました: {}", fontConfig.path);
                        }
                    }
                }
            }
        }
    }

    // Dear ImGui スタイルをセットアップ
    ImGui::StyleColorsDark();

    // プラットフォーム / レンダラーバックエンドをセットアップ
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
}

ImGuiManager::~ImGuiManager() {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiManager::newFrame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::render() {
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
}

bool ImGuiManager::processEvent(const SDL_Event& event) {
    return ImGui_ImplSDL3_ProcessEvent(&event);
}

} // namespace mk
