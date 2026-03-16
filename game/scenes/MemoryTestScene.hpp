#pragma once
#ifndef NDEBUG

#include "engine/scene/Scene.hpp"
#include <string>
#include <vector>

namespace makai {

/// メモリアロケーターの動作テストシーン（デバッグビルド専用）
///
/// MemoryManager で管理される全アロケーターを対話的にテストできる。
/// 各アロケーターへの割り当て・解放・リセットを ImGui で操作し、
/// 統計情報をリアルタイムで表示する。
class MemoryTestScene : public mk::Scene {
public:
    explicit MemoryTestScene(mk::Game& game);
    ~MemoryTestScene() override;

    void handleEvent(const SDL_Event& event) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

    void onEnter() override;
    void onExit()  override;

private:
    /// 各アロケーターのテスト状態
    struct AllocRecord {
        void* ptr  = nullptr;
        size_t size = 0;
    };

    /// スタックアロケーターのマーカー記録
    struct StackMarker {
        size_t marker = 0;
        size_t size   = 0;
    };

    // ImGui パネル描画
    void renderLinearPanel();
    void renderDoubleFramePanel();
    void renderScenePanel();
    void renderStackPanel();
    void renderBuddyPanel();
    void renderPagedPanel();
    void renderHeapPanel();
    void renderStatsPanel();

    // ───── テスト状態 ─────

    // LinearAllocator（フレーム）
    int  m_linearAllocSize = 64;

    // DoubleFrameAllocator
    int  m_doubleFrameAllocSize = 64;

    // SceneAllocator
    int  m_sceneAllocSize = 128;

    // StackAllocator
    int  m_stackAllocSize = 128;
    std::vector<StackMarker> m_stackMarkers; ///< プッシュしたマーカー一覧

    // BuddyAllocator
    int  m_buddyAllocSize = 256;
    std::vector<AllocRecord> m_buddyAllocs; ///< 割り当て中ブロック一覧

    // PagedAllocator
    int  m_pagedAllocSize = 512;

    // HeapAllocator（FreeList）
    int  m_heapAllocSize = 256;
    std::vector<AllocRecord> m_heapAllocs; ///< 割り当て中ブロック一覧

    // 結果ログ（最大 32 件）
    std::vector<std::string> m_log;
    static constexpr int LOG_MAX = 32;

    void addLog(const char* fmt, ...);
};

} // namespace makai

#endif // NDEBUG
