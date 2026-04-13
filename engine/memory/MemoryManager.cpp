#include "MemoryManager.hpp"
#include "../core/log/Logger.hpp"
#include <bit>
#include <cstdlib>
#include <format>
#include <new>      // std::nothrow

// MemoryManager は 64bit 環境専用（MB→byte 変換で size_t オーバーフローを起こさないための前提）
static_assert(sizeof(size_t) >= 8, "MemoryManager は 64bit 環境でのみ使用可能です");

namespace mk::memory {

MemoryManager& MemoryManager::instance() {
    static MemoryManager inst;
    return inst;
}

bool MemoryManager::init(const mk::MemoryConfig& config) {
    auto& mgr = instance();

    // 多重初期化を防ぐ（Logger ヒープまたはマスターバッファが確保済みなら初期化済みとみなす）
    if (mgr.m_loggerBuffer != nullptr || mgr.m_masterBuffer != nullptr) {
        MK_BOOT_WARN("MemoryManager::init が複数回呼ばれました。2回目以降は無視します。");
        return true;
    }

    // MemoryConfig の値が負または 0 でないことを検証する
    if (config.frameAllocatorMB <= 0 || config.sceneAllocatorMB <= 0 ||
        config.heapAllocatorMB  <= 0 || config.doubleFrameAllocatorMB <= 0 ||
        config.stackAllocatorMB <= 0 || config.buddyAllocatorMB <= 0 ||
        config.pagedAllocatorPageKB <= 0) {
        MK_BOOT_ERROR(std::format(
            "MemoryManager: MemoryConfig に無効な値が含まれています "
            "(frame={}, doubleFrame={}, scene={}, heap={}, stack={}, buddy={}, pagedPageKB={})",
            config.frameAllocatorMB, config.doubleFrameAllocatorMB,
            config.sceneAllocatorMB, config.heapAllocatorMB,
            config.stackAllocatorMB, config.buddyAllocatorMB,
            config.pagedAllocatorPageKB));
        return false;
    }

    // loggerHeapKB の検証
    if (config.loggerHeapKB <= 0) {
        MK_BOOT_ERROR(std::format(
            "MemoryManager: loggerHeapKB に無効な値が指定されています ({})",
            config.loggerHeapKB));
        return false;
    }

    // 各アロケーターサイズの上限チェック（乗算オーバーフロー防止）
    // 1 アロケーターあたり 4096 MB を上限とする（ゲームエンジンの実用範囲）
    static constexpr int MAX_ALLOCATOR_MB  = 4096;
    // BuddyAllocator は MAX_ORDER=30（1GB）までしか管理できないため 1024MB を上限とする
    static constexpr int MAX_BUDDY_MB      = 1024;
    // pagedAllocatorPageKB は 1 KB〜1 GB（= 1024*1024 KB）を上限とする
    static constexpr int MAX_PAGE_KB       = 1024 * 1024;
    // loggerHeapKB は 256 MB (= 256 * 1024 KB) を上限とする（ログ用ヒープとしては十分なサイズ）
    static constexpr int MAX_LOGGER_HEAP_KB = 256 * 1024;
    if (config.frameAllocatorMB       > MAX_ALLOCATOR_MB ||
        config.doubleFrameAllocatorMB > MAX_ALLOCATOR_MB ||
        config.sceneAllocatorMB       > MAX_ALLOCATOR_MB ||
        config.heapAllocatorMB        > MAX_ALLOCATOR_MB ||
        config.stackAllocatorMB       > MAX_ALLOCATOR_MB) {
        MK_BOOT_ERROR(std::format(
            "MemoryManager: MemoryConfig の値が上限({} MB)を超えています",
            MAX_ALLOCATOR_MB));
        return false;
    }
    if (config.loggerHeapKB > MAX_LOGGER_HEAP_KB) {
        MK_BOOT_ERROR(std::format(
            "MemoryManager: loggerHeapKB の値が上限({} KB = {} MB)を超えています",
            MAX_LOGGER_HEAP_KB, MAX_LOGGER_HEAP_KB / 1024));
        return false;
    }
    if (config.buddyAllocatorMB > MAX_BUDDY_MB) {
        MK_BOOT_ERROR(std::format(
            "MemoryManager: buddyAllocatorMB の値が上限({} MB)を超えています "
            "(BuddyAllocator の最大管理サイズ = 2^MAX_ORDER = 1GB)",
            MAX_BUDDY_MB));
        return false;
    }
    if (config.pagedAllocatorPageKB > MAX_PAGE_KB) {
        MK_BOOT_ERROR(std::format(
            "MemoryManager: pagedAllocatorPageKB の値が上限({} KB)を超えています",
            MAX_PAGE_KB));
        return false;
    }

    // Logger 専用ヒープを確保する（マスターバッファとは独立）
    // Logger 側でまだ実際の割り当てに利用していないため、失敗は致命扱いせず警告のみで継続する
    mgr.m_loggerBuffer   = nullptr;
    mgr.m_loggerSize     = 0;
    mgr.m_loggerResource.reset();

    const size_t loggerSize = static_cast<size_t>(config.loggerHeapKB) * 1024;
    if (loggerSize > 0) {
        mgr.m_loggerBuffer = std::malloc(loggerSize);
        if (!mgr.m_loggerBuffer) {
            // メモリ逼迫時に std::format が追加の確保で失敗することを避けるため固定メッセージにする
            MK_BOOT_WARN("MemoryManager: Logger 専用ヒープの確保に失敗しました。専用ヒープなしで継続します");
        } else {
            // ヒープ確保に成功したのでサイズを反映する
            mgr.m_loggerSize = loggerSize;
            // new(std::nothrow) で例外を使わずに構築する
            auto* ptr = new(std::nothrow) FirstFitMemoryResource(
                mgr.m_loggerBuffer, loggerSize);
            if (!ptr) {
                std::free(mgr.m_loggerBuffer);
                mgr.m_loggerBuffer = nullptr;
                mgr.m_loggerSize   = 0;
                MK_BOOT_WARN("MemoryManager: Logger 専用メモリリソースの構築に失敗しました。専用ヒープなしで継続します");
            } else {
                mgr.m_loggerResource.reset(ptr);
            }
        }
    }
    const size_t frameSize       = static_cast<size_t>(config.frameAllocatorMB)       * 1024 * 1024;
    const size_t doubleFrameSize = static_cast<size_t>(config.doubleFrameAllocatorMB) * 1024 * 1024;
    const size_t sceneSize       = static_cast<size_t>(config.sceneAllocatorMB)       * 1024 * 1024;
    const size_t heapSize        = static_cast<size_t>(config.heapAllocatorMB)        * 1024 * 1024;
    const size_t stackSize       = static_cast<size_t>(config.stackAllocatorMB)       * 1024 * 1024;
    // バディアロケーターのバッファサイズを 2 の累乗に切り捨てる
    // （BuddyAllocator 内でも切り捨てが行われるが、マスターバッファからの
    //   確保量を BuddyAllocator が実際に使うサイズに揃えて無駄な予約を防ぐ）
    const size_t buddySizeRaw    = static_cast<size_t>(config.buddyAllocatorMB)       * 1024 * 1024;
    const size_t buddySize       = std::bit_floor(buddySizeRaw);

    // Logger 専用ヒープの巻き戻し（Logger ヒープ確保後の失敗パスで使用）
    auto rollbackLogger = [&]() {
        mgr.m_loggerResource.reset();
        if (mgr.m_loggerBuffer) {
            std::free(mgr.m_loggerBuffer);
            mgr.m_loggerBuffer = nullptr;
        }
        mgr.m_loggerSize = 0;
    };

    // doubleFrameSize*2 の乗算オーバーフロー検出
    if (doubleFrameSize > (SIZE_MAX / 2)) {
        MK_BOOT_ERROR("MemoryManager: doubleFrameAllocatorMB の 2 倍がオーバーフローします");
        rollbackLogger();
        return false;
    }
    const size_t doubleFrameTotal = doubleFrameSize * 2;
    const size_t totalSize        = frameSize + doubleFrameTotal + sceneSize + heapSize
                                    + stackSize + buddySize;

    // 加算オーバーフロー検出（各項より合計が小さくなった場合に折り返しを検知）
    if (totalSize < frameSize || totalSize < heapSize) {
        MK_BOOT_ERROR("MemoryManager: MemoryConfig の合計サイズがオーバーフローしました");
        rollbackLogger();
        return false;
    }

    mgr.m_masterBuffer = std::malloc(totalSize);

    if (!mgr.m_masterBuffer) {
        // OOM 経路では追加の動的確保を避け、Logger 専用ヒープの巻き戻しを確実に行う
        MK_BOOT_ERROR("MemoryManager: マスターバッファの確保に失敗しました");
        rollbackLogger();
        mgr.m_masterSize   = 0;
        return false;
    }

    mgr.m_masterSize = totalSize;

    // 例外無効環境: make_unique の内部 new 失敗時は abort される。
    // ただし構築するオブジェクトは小さなメタデータ（数十バイト）のため OOM リスクは極めて低い。
    // masterBuffer リーク＋次回 init 不可を防ぐための rollback は検証失敗パスで引き続き使用する。
    auto rollback = [&]() {
        mgr.m_pagedAllocator.reset();
        mgr.m_buddyAllocator.reset();
        mgr.m_stackAllocator.reset();
        if (mgr.m_pools.has_value()) {
            mgr.m_pools->clear();  // PoolHolderDeleter を起動（まだ空なので no-op）
            mgr.m_pools.reset();   // optional 自体を破棄
        }
        mgr.m_frameAllocator.reset();
        mgr.m_doubleFrameAllocator.reset();
        mgr.m_sceneAllocator.reset();
        mgr.m_masterResource.reset();
        std::free(mgr.m_masterBuffer);
        mgr.m_masterBuffer              = nullptr;
        mgr.m_masterSize                = 0;
        mgr.m_subAllocatorReservedBytes = 0;
        // Logger 専用ヒープも巻き戻す
        rollbackLogger();
    };

    {
        // マスター FreeList がバッファ全体を管理する
        mgr.m_masterResource = std::make_unique<FirstFitMemoryResource>(mgr.m_masterBuffer, totalSize);
        auto& master = mgr.m_masterResource->getAllocator();

        // m_pools をマスター FreeList の pmr リソースで初期化する（OS ヒープを使わない）
        mgr.m_pools.emplace(mgr.m_masterResource.get());

        // 各サブアロケーターはマスター FreeList からバッファを借りる
        // （払い出したバッファは永続使用のため deallocate しない）
        void* frameBuf = master.allocate(frameSize,        alignof(std::max_align_t));
        void* dframe0  = master.allocate(doubleFrameSize,  alignof(std::max_align_t));
        void* dframe1  = master.allocate(doubleFrameSize,  alignof(std::max_align_t));
        void* sceneBuf = master.allocate(sceneSize,        alignof(std::max_align_t));
        void* stackBuf = master.allocate(stackSize,        alignof(std::max_align_t));
        void* buddyBuf = master.allocate(buddySize,        alignof(std::max_align_t));

        // いずれかのサブアロケーター用バッファ確保に失敗した場合は即座に失敗として処理する
        if (!frameBuf || !dframe0 || !dframe1 || !sceneBuf || !stackBuf || !buddyBuf) {
            MK_BOOT_ERROR("MemoryManager: サブアロケーター用バッファの確保に失敗");
            rollback();
            return false;
        }

        mgr.m_frameAllocator = std::make_unique<LinearAllocator>(frameBuf, frameSize);
        mgr.m_doubleFrameAllocator = std::make_unique<DoubleFrameAllocator>(
            dframe0, doubleFrameSize, dframe1, doubleFrameSize);
        mgr.m_sceneAllocator = std::make_unique<LinearAllocator>(sceneBuf, sceneSize);
        mgr.m_stackAllocator = std::make_unique<StackAllocator>(stackBuf, stackSize);
        mgr.m_buddyAllocator = std::make_unique<BuddyAllocator>(buddyBuf, buddySize);

        // 固定バッファのサブアロケーター予約分をここで記録する
        // （PagedAllocator の初期ページ確保分をヒープ統計から除外しないため、
        //   PagedAllocator 生成より前に記録しなければならない）
        mgr.m_subAllocatorReservedBytes = master.getUsedBytes();

        // ページドアロケーターはバッキングとしてヒープ（マスター FreeList の残余）を使う
        const size_t pageSize = static_cast<size_t>(config.pagedAllocatorPageKB) * 1024;

        // (1) ページサイズがヘッダサイズ以下の場合は PagedAllocator が使用不可状態になるため
        //     コンストラクタを呼ぶ前に事前検証して失敗を明示する
        if (pageSize <= PagedAllocator::kHeaderSize) {
            MK_BOOT_ERROR(std::format(
                "MemoryManager: pagedAllocatorPageKB から計算したページサイズ {} bytes が"
                "ヘッダサイズ {} bytes 以下です。PagedAllocator を生成できません。",
                pageSize, PagedAllocator::kHeaderSize));
            rollback();
            return false;
        }

        const size_t remainingBytes = totalSize - mgr.m_subAllocatorReservedBytes;
        if (pageSize > remainingBytes) {
            MK_BOOT_ERROR(std::format(
                "MemoryManager: PagedAllocator のページサイズ {} bytes が"
                "残余ヒープ {} bytes を超えています",
                pageSize, remainingBytes));
            rollback();
            return false;
        }
        mgr.m_pagedAllocator = std::make_unique<PagedAllocator>(pageSize, master);

        // (2) コンストラクタ内で初期ページ確保に失敗した場合は getPageCount() == 0 になる。
        //     使用不可状態のまま init() が成功扱いにならないよう確認して失敗時は rollback する。
        if (mgr.m_pagedAllocator->getPageCount() == 0) {
            MK_BOOT_ERROR("MemoryManager: PagedAllocator の初期化に失敗しました "
                          "(初期ページの確保に失敗)");
            rollback();
            return false;
        }
    }

    MK_BOOT_INFO(std::format(
        "MemoryManager: 初期化完了 — Logger ヒープ {} KB / マスターバッファ {} MB "
        "(サブアロケーター予約 {} MB / ヒープ残余 {} MB)",
        config.loggerHeapKB,
        totalSize / (1024 * 1024),
        mgr.m_subAllocatorReservedBytes / (1024 * 1024),
        (totalSize - mgr.m_subAllocatorReservedBytes) / (1024 * 1024)));

    return true;
}

void MemoryManager::shutdown() {
    auto& mgr = instance();

    // 未初期化なら何もしない（二重呼び出しも安全）
    if (!mgr.m_masterBuffer && !mgr.m_loggerBuffer) return;

    // PoolHolderDeleter がマスター FreeList に deallocate するため
    // m_masterResource より先にプールを破棄しなければならない
    if (mgr.m_pools.has_value()) {
        mgr.m_pools->clear();
        mgr.m_pools.reset();
    }

    // PagedAllocator のデストラクタがマスター FreeList に deallocate するため先に破棄する
    mgr.m_pagedAllocator.reset();

    // サブアロケーターを破棄する（外部バッファなので free は呼ばない）
    mgr.m_frameAllocator.reset();
    mgr.m_doubleFrameAllocator.reset();
    mgr.m_sceneAllocator.reset();
    mgr.m_stackAllocator.reset();
    mgr.m_buddyAllocator.reset();

    // マスターリソース自体を破棄する
    if (mgr.m_masterResource) {
        mgr.m_masterResource->reset();
        mgr.m_masterResource.reset();
    }
    // マスターバッファを解放する
    if (mgr.m_masterBuffer) {
        std::free(mgr.m_masterBuffer);
        mgr.m_masterBuffer = nullptr;
    }
    mgr.m_masterSize = 0;

    // Logger 専用ヒープを解放する
    // （Logger::shutdown() の後に呼ばれることが RAII ガードの破棄順序で保証されている）
    if (mgr.m_loggerResource) {
        mgr.m_loggerResource->reset();
        mgr.m_loggerResource.reset();
    }
    if (mgr.m_loggerBuffer) {
        std::free(mgr.m_loggerBuffer);
        mgr.m_loggerBuffer = nullptr;
    }
    mgr.m_loggerSize = 0;

    mgr.m_subAllocatorReservedBytes = 0;
    mgr.m_totalFrameAllocations     = 0;
    mgr.m_totalSceneAllocations     = 0;
}

MemoryManager::~MemoryManager() {
    // shutdown() がまだ呼ばれていない場合に備える（二重呼び出しは安全）
    shutdown();
}

void MemoryManager::onFrameEnd() {
    // init() 未実行／失敗時はアロケーターが nullptr のためガードする
    if (!m_frameAllocator || !m_doubleFrameAllocator) {
        // Logger 未初期化状態では CORE_ERROR が出力されないため BootstrapLogger を使用する
        MK_BOOT_ERROR("MemoryManager::onFrameEnd: 未初期化のためスキップ");
        return;
    }

    size_t usedBytes = m_frameAllocator->getUsedBytes();
    if (usedBytes > 0) {
        m_totalFrameAllocations++;
    }
    m_frameAllocator->reset();

    // ダブルフレームアロケーターをスワップ（新フロントをリセット）
    m_doubleFrameAllocator->swap();

    #ifdef DEBUG_MEMORY_VERBOSE
    if (usedBytes > 0) {
        CORE_DEBUG("MemoryManager: Frame allocator reset (used: {} bytes)", usedBytes);
    }
    #endif
}

void MemoryManager::onSceneChange() {
    // init() 未実行／失敗時はアロケーターが nullptr のためガードする
    if (!m_sceneAllocator) {
        // Logger 未初期化状態では CORE_ERROR が出力されないため BootstrapLogger を使用する
        MK_BOOT_ERROR("MemoryManager::onSceneChange: 未初期化のためスキップ");
        return;
    }

    size_t usedBytes = m_sceneAllocator->getUsedBytes();
    if (usedBytes > 0) {
        m_totalSceneAllocations++;
    }

    m_sceneAllocator->reset();

    CORE_INFO("MemoryManager: Scene allocator reset (used: {} bytes, {:.2f} MB)",
              usedBytes, usedBytes / (1024.0 * 1024.0));
}

MemoryManager::Stats MemoryManager::getStats() const {
    // init() 未呼び出しまたは失敗時はゼロ値を返す
    if (!m_masterResource || !m_frameAllocator ||
        !m_doubleFrameAllocator || !m_sceneAllocator) {
        return Stats{};
    }

    Stats stats{};

    // フレームアロケーター
    stats.frameBytes      = m_frameAllocator->getUsedBytes();
    stats.frameCapacity   = m_frameAllocator->getCapacity();
    stats.frameUsageRatio = m_frameAllocator->getUsageRatio();

    // ダブルフレームアロケーター（現フロント）
    const auto& dfa = m_doubleFrameAllocator->current();
    stats.doubleFrameCurrentBytes        = dfa.getUsedBytes();
    stats.doubleFrameCapacity            = dfa.getCapacity();
    stats.doubleFrameCurrentUsageRatio   = dfa.getUsageRatio();

    // シーンアロケーター
    stats.sceneBytes      = m_sceneAllocator->getUsedBytes();
    stats.sceneCapacity   = m_sceneAllocator->getCapacity();
    stats.sceneUsageRatio = m_sceneAllocator->getUsageRatio();

    // ヒープアロケーター（マスター FreeList のサブアロケーター予約分を除いた実効値）
    const auto& master             = m_masterResource->getAllocator();
    const size_t masterUsed        = master.getUsedBytes();
    stats.heapBytes                = masterUsed > m_subAllocatorReservedBytes
                                         ? masterUsed - m_subAllocatorReservedBytes : 0;
    stats.heapCapacity             = master.getCapacity() > m_subAllocatorReservedBytes
                                         ? master.getCapacity() - m_subAllocatorReservedBytes : 0;
    stats.heapUsageRatio           = stats.heapCapacity > 0
                                         ? static_cast<float>(stats.heapBytes) / stats.heapCapacity : 0.0f;
    stats.heapAllocationCount      = master.getAllocationCount();
    stats.heapFreeBlockCount       = master.getFreeBlockCount();
    stats.masterTotalBytes         = masterUsed;
    stats.masterTotalCapacity      = master.getCapacity();

    // スタックアロケーター
    if (m_stackAllocator) {
        stats.stackBytes      = m_stackAllocator->getUsedBytes();
        stats.stackCapacity   = m_stackAllocator->getCapacity();
        stats.stackUsageRatio = m_stackAllocator->getUsageRatio();
    }

    // バディアロケーター
    if (m_buddyAllocator) {
        stats.buddyBytes      = m_buddyAllocator->getUsedBytes();
        stats.buddyCapacity   = m_buddyAllocator->getCapacity();
        stats.buddyUsageRatio = m_buddyAllocator->getUsageRatio();
    }

    // ページドアロケーター
    if (m_pagedAllocator) {
        stats.pagedBytes         = m_pagedAllocator->getUsedBytes();
        stats.pagedTotalCapacity = m_pagedAllocator->getTotalCapacity();
        stats.pagedPageCount     = m_pagedAllocator->getPageCount();
        stats.pagedUsageRatio    = m_pagedAllocator->getUsageRatio();
    }

    // 総割り当て回数
    stats.totalFrameAllocations = m_totalFrameAllocations;
    stats.totalSceneAllocations = m_totalSceneAllocations;

    return stats;
}

} // namespace mk::memory
