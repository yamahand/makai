#ifndef NDEBUG

#include "MemoryTestScene.hpp"
#include "DebugBootScene.hpp"
#include "engine/core/Game.hpp"
#include "engine/memory/MemoryManager.hpp"
#include "engine/memory/StackAllocator.hpp"
#include <imgui.h>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace makai {

// ────────────────────────────────────────────
// コンストラクタ / デストラクタ
// ────────────────────────────────────────────

MemoryTestScene::MemoryTestScene(mk::Game& game)
    : mk::Scene(game)
{}

MemoryTestScene::~MemoryTestScene() {
    // 割り当てたままのブロックを解放する
    auto& mem = mk::memory::MemoryManager::instance();
    for (auto& rec : m_buddyAllocs) {
        if (rec.ptr) mem.buddyAllocator().deallocate(rec.ptr);
    }
    for (auto& rec : m_heapAllocs) {
        if (rec.ptr) mem.heapAllocator().deallocate(rec.ptr);
    }
}

void MemoryTestScene::onEnter() {
    addLog("MemoryTestScene に入りました");
}

void MemoryTestScene::onExit() {
    // シーン離脱時に保持中のポインタをすべて解放する
    auto& mem = mk::memory::MemoryManager::instance();
    for (auto& rec : m_buddyAllocs) {
        if (rec.ptr) { mem.buddyAllocator().deallocate(rec.ptr); rec.ptr = nullptr; }
    }
    m_buddyAllocs.clear();
    for (auto& rec : m_heapAllocs) {
        if (rec.ptr) { mem.heapAllocator().deallocate(rec.ptr); rec.ptr = nullptr; }
    }
    m_heapAllocs.clear();
    m_stackMarkers.clear();

    // このシーンで使用したスタック / ページアロケーターをクリーンな状態に戻す
    mem.stackAllocator().reset();
    mem.pagedAllocator().reset();
}

// ────────────────────────────────────────────
// イベント / 更新
// ────────────────────────────────────────────

void MemoryTestScene::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_ESCAPE) {
        // ESC でデバッグブートシーンへ戻る
        m_game.sceneManager().replace(std::make_unique<DebugBootScene>(m_game));
    }
}

void MemoryTestScene::update(float /*deltaTime*/) {}

// ────────────────────────────────────────────
// 描画
// ────────────────────────────────────────────

void MemoryTestScene::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_RenderClear(renderer);

    const ImGuiIO& io = ImGui::GetIO();

    // ─── 左側：操作パネル ───
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(420, io.DisplaySize.y - 16), ImGuiCond_Always);
    ImGui::Begin("## MemTestOp", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[DEBUG] メモリアロケーターテスト");
    ImGui::TextDisabled("ESC でデバッグメニューへ戻る");
    ImGui::Separator();

    renderLinearPanel();
    ImGui::Separator();
    renderDoubleFramePanel();
    ImGui::Separator();
    renderScenePanel();
    ImGui::Separator();
    renderStackPanel();
    ImGui::Separator();
    renderBuddyPanel();
    ImGui::Separator();
    renderPagedPanel();
    ImGui::Separator();
    renderHeapPanel();

    ImGui::End();

    // ─── 右側：統計 + ログ ───
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 440, 8), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(432, io.DisplaySize.y - 16), ImGuiCond_Always);
    ImGui::Begin("## MemTestStats", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    renderStatsPanel();

    ImGui::Separator();
    ImGui::TextDisabled("操作ログ");
    for (const auto& line : m_log) {
        ImGui::TextUnformatted(line.c_str());
    }

    ImGui::End();
}

// ────────────────────────────────────────────
// LinearAllocator（フレーム）パネル
// ────────────────────────────────────────────

void MemoryTestScene::renderLinearPanel() {
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "LinearAllocator（フレーム）");
    ImGui::InputInt("サイズ (B)##linear", &m_linearAllocSize);
    if (m_linearAllocSize < 1) m_linearAllocSize = 1;

    if (ImGui::Button("Allocate##linear")) {
        auto& alloc = mk::memory::MemoryManager::instance().frameAllocator();
        void* p = alloc.allocate(static_cast<size_t>(m_linearAllocSize));
        if (p) addLog("[Linear] allocate %d B → %p", m_linearAllocSize, p);
        else   addLog("[Linear] allocate %d B → 失敗", m_linearAllocSize);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##linear")) {
        mk::memory::MemoryManager::instance().frameAllocator().reset();
        addLog("[Linear] reset");
    }
}

// ────────────────────────────────────────────
// DoubleFrameAllocator パネル
// ────────────────────────────────────────────

void MemoryTestScene::renderDoubleFramePanel() {
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.7f, 1.0f), "DoubleFrameAllocator");
    ImGui::InputInt("サイズ (B)##dframe", &m_doubleFrameAllocSize);
    if (m_doubleFrameAllocSize < 1) m_doubleFrameAllocSize = 1;

    auto& mem = mk::memory::MemoryManager::instance();

    // current() へ割り当て
    if (ImGui::Button("Alloc current##dframe")) {
        void* p = mem.doubleFrameAllocator().allocate(
            static_cast<size_t>(m_doubleFrameAllocSize));
        if (p) addLog("[DFrame] alloc current %d B → %p", m_doubleFrameAllocSize, p);
        else   addLog("[DFrame] alloc current %d B → 失敗", m_doubleFrameAllocSize);
    }
    ImGui::SameLine();
    // current() の内容をリセット（previous との swap は行わない）
    // 注意: 実際の swap はゲームループの onFrameEnd() で毎フレーム自動的に行われる
    if (ImGui::Button("Reset current##dframe")) {
        mem.doubleFrameAllocator().reset();
        addLog("[DFrame] current reset");
    }

    const auto& cur  = mem.doubleFrameAllocator();
    const auto& prev = mem.previousFrameAllocator();
    ImGui::TextDisabled("  current: %zu B / %zu B  |  previous: %zu B / %zu B",
                        cur.getUsedBytes(),  cur.getCapacity(),
                        prev.getUsedBytes(), prev.getCapacity());
}

// ────────────────────────────────────────────
// SceneAllocator パネル
// ────────────────────────────────────────────

void MemoryTestScene::renderScenePanel() {
    ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.4f, 1.0f), "SceneAllocator（Linear）");
    ImGui::InputInt("サイズ (B)##scene", &m_sceneAllocSize);
    if (m_sceneAllocSize < 1) m_sceneAllocSize = 1;

    auto& scene = mk::memory::MemoryManager::instance().sceneAllocator();

    if (ImGui::Button("Allocate##scene")) {
        void* p = scene.allocate(static_cast<size_t>(m_sceneAllocSize));
        if (p) addLog("[Scene] allocate %d B → %p", m_sceneAllocSize, p);
        else   addLog("[Scene] allocate %d B → 失敗", m_sceneAllocSize);
    }
    ImGui::SameLine();
    // シーン切り替え相当のリセット（実際は SceneManager::applyPendingChanges() 経由）
    if (ImGui::Button("Reset (シーン切り替え相当)##scene")) {
        scene.reset();
        addLog("[Scene] reset");
    }

    ImGui::TextDisabled("  used: %zu B / %zu B  (%.1f%%)",
                        scene.getUsedBytes(), scene.getCapacity(),
                        scene.getUsageRatio() * 100.0f);
}

// ────────────────────────────────────────────
// StackAllocator パネル
// ────────────────────────────────────────────

void MemoryTestScene::renderStackPanel() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "StackAllocator");
    ImGui::InputInt("サイズ (B)##stack", &m_stackAllocSize);
    if (m_stackAllocSize < 1) m_stackAllocSize = 1;

    auto& stack = mk::memory::MemoryManager::instance().stackAllocator();

    if (ImGui::Button("Push (mark + alloc)##stack")) {
        // 現在地をマークしてから割り当て
        StackMarker sm;
        sm.marker = stack.mark();
        sm.size   = static_cast<size_t>(m_stackAllocSize);
        void* p   = stack.allocate(sm.size);
        if (p) {
            m_stackMarkers.push_back(sm);
            addLog("[Stack] push mark=%zu, alloc %zu B → %p", sm.marker, sm.size, p);
        } else {
            addLog("[Stack] alloc %zu B → 失敗", sm.size);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Pop (rewind)##stack")) {
        if (!m_stackMarkers.empty()) {
            size_t m = m_stackMarkers.back().marker;
            stack.rewindTo(m);
            m_stackMarkers.pop_back();
            addLog("[Stack] rewindTo %zu", m);
        } else {
            addLog("[Stack] pop: マーカーなし");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##stack")) {
        stack.reset();
        m_stackMarkers.clear();
        addLog("[Stack] reset");
    }

    ImGui::TextDisabled("  マーカー数: %zu / used: %zu B",
                        m_stackMarkers.size(), stack.getUsedBytes());
}

// ────────────────────────────────────────────
// BuddyAllocator パネル
// ────────────────────────────────────────────

void MemoryTestScene::renderBuddyPanel() {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "BuddyAllocator");
    ImGui::InputInt("サイズ (B)##buddy", &m_buddyAllocSize);
    if (m_buddyAllocSize < 1) m_buddyAllocSize = 1;

    auto& buddy = mk::memory::MemoryManager::instance().buddyAllocator();

    if (ImGui::Button("Allocate##buddy")) {
        void* p = buddy.allocate(static_cast<size_t>(m_buddyAllocSize));
        if (p) {
            m_buddyAllocs.push_back({p, static_cast<size_t>(m_buddyAllocSize)});
            addLog("[Buddy] allocate %d B → %p", m_buddyAllocSize, p);
        } else {
            addLog("[Buddy] allocate %d B → 失敗", m_buddyAllocSize);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Free Last##buddy")) {
        if (!m_buddyAllocs.empty()) {
            auto& rec = m_buddyAllocs.back();
            buddy.deallocate(rec.ptr);
            addLog("[Buddy] deallocate %p (%zu B)", rec.ptr, rec.size);
            m_buddyAllocs.pop_back();
        } else {
            addLog("[Buddy] deallocate: 割り当てなし");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##buddy")) {
        buddy.reset();
        m_buddyAllocs.clear();
        addLog("[Buddy] reset");
    }

    ImGui::TextDisabled("  割り当て中: %zu ブロック / used: %zu B",
                        m_buddyAllocs.size(), buddy.getUsedBytes());
}

// ────────────────────────────────────────────
// PagedAllocator パネル
// ────────────────────────────────────────────

void MemoryTestScene::renderPagedPanel() {
    ImGui::TextColored(ImVec4(0.9f, 0.5f, 1.0f, 1.0f), "PagedAllocator");
    ImGui::InputInt("サイズ (B)##paged", &m_pagedAllocSize);
    if (m_pagedAllocSize < 1) m_pagedAllocSize = 1;

    auto& paged = mk::memory::MemoryManager::instance().pagedAllocator();

    if (ImGui::Button("Allocate##paged")) {
        void* p = paged.allocate(static_cast<size_t>(m_pagedAllocSize));
        if (p) addLog("[Paged] allocate %d B → %p  pages=%zu",
                      m_pagedAllocSize, p, paged.getPageCount());
        else   addLog("[Paged] allocate %d B → 失敗", m_pagedAllocSize);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##paged")) {
        paged.reset();
        addLog("[Paged] reset  pages=%zu", paged.getPageCount());
    }

    ImGui::TextDisabled("  ページ数: %zu / used: %zu B",
                        paged.getPageCount(), paged.getUsedBytes());
}

// ────────────────────────────────────────────
// HeapAllocator（FreeList）パネル
// ────────────────────────────────────────────

void MemoryTestScene::renderHeapPanel() {
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "HeapAllocator（FreeList）");
    ImGui::InputInt("サイズ (B)##heap", &m_heapAllocSize);
    if (m_heapAllocSize < 1) m_heapAllocSize = 1;

    auto& heap = mk::memory::MemoryManager::instance().heapAllocator();

    if (ImGui::Button("Allocate##heap")) {
        void* p = heap.allocate(static_cast<size_t>(m_heapAllocSize));
        if (p) {
            m_heapAllocs.push_back({p, static_cast<size_t>(m_heapAllocSize)});
            addLog("[Heap] allocate %d B → %p", m_heapAllocSize, p);
        } else {
            addLog("[Heap] allocate %d B → 失敗", m_heapAllocSize);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Free Last##heap")) {
        if (!m_heapAllocs.empty()) {
            auto& rec = m_heapAllocs.back();
            heap.deallocate(rec.ptr);
            addLog("[Heap] deallocate %p (%zu B)", rec.ptr, rec.size);
            m_heapAllocs.pop_back();
        } else {
            addLog("[Heap] deallocate: 割り当てなし");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Free All##heap")) {
        for (auto& rec : m_heapAllocs) {
            if (rec.ptr) heap.deallocate(rec.ptr);
        }
        addLog("[Heap] free all (%zu ブロック)", m_heapAllocs.size());
        m_heapAllocs.clear();
    }

    ImGui::TextDisabled("  割り当て中: %zu ブロック / used: %zu B",
                        m_heapAllocs.size(), heap.getUsedBytes());
}

// ────────────────────────────────────────────
// 統計パネル
// ────────────────────────────────────────────

void MemoryTestScene::renderStatsPanel() {
    auto stats = mk::memory::MemoryManager::instance().getStats();

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "メモリ統計");

    auto drawBar = [](const char* label, size_t used, size_t cap, float ratio) {
        ImGui::Text("%-22s %zu / %zu B  (%.1f%%)",
                    label, used, cap, ratio * 100.0f);
        ImGui::ProgressBar(ratio, ImVec2(-1.0f, 6.0f), "");
    };

    drawBar("Linear(frame):",
            stats.frameBytes, stats.frameCapacity, stats.frameUsageRatio);
    drawBar("DoubleFrame(cur):",
            stats.doubleFrameCurrentBytes, stats.doubleFrameCapacity,
            stats.doubleFrameCurrentUsageRatio);
    drawBar("Scene:",
            stats.sceneBytes, stats.sceneCapacity, stats.sceneUsageRatio);
    drawBar("Stack:",
            stats.stackBytes, stats.stackCapacity, stats.stackUsageRatio);
    drawBar("Buddy:",
            stats.buddyBytes, stats.buddyCapacity, stats.buddyUsageRatio);
    drawBar("Paged:",
            stats.pagedBytes,
            stats.pagedTotalCapacity > 0 ? stats.pagedTotalCapacity : 1,
            stats.pagedUsageRatio);
    drawBar("Heap(FreeList):",
            stats.heapBytes, stats.heapCapacity > 0 ? stats.heapCapacity : 1,
            stats.heapUsageRatio);

    ImGui::Text("Heap allocs: %zu / free blocks: %zu",
                stats.heapAllocationCount, stats.heapFreeBlockCount);
    ImGui::Text("Paged pages: %zu", stats.pagedPageCount);
    ImGui::Text("Master: %zu / %zu B",
                stats.masterTotalBytes, stats.masterTotalCapacity);
}

// ────────────────────────────────────────────
// ログ追加
// ────────────────────────────────────────────

void MemoryTestScene::addLog(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    m_log.insert(m_log.begin(), std::string(buf));
    if (static_cast<int>(m_log.size()) > LOG_MAX) {
        m_log.resize(LOG_MAX);
    }
}

} // namespace makai

#endif // NDEBUG
