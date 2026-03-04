#include "PagedAllocator.hpp"
#include <SDL3/SDL_log.h>
#include <new>

namespace mk::memory {

PagedAllocator::PagedAllocator(size_t pageSize, FirstFitAllocator& backing)
    : m_backing(backing)
    , m_pageSize(pageSize)
    , m_head(nullptr)
    , m_current(nullptr)
    , m_pageCount(0)
    , m_usedBytes(0)
{
    // pageSize がヘッダ分にも満たない場合は容量計算がアンダーフローするため使用不可にする
    if (pageSize <= kHeaderSize) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PagedAllocator: ページサイズ(%zu B)がヘッダサイズ(%zu B)以下のため使用不可",
                     pageSize, kHeaderSize);
        m_pageSize = 0; // 使用不可状態を明示
        return; // m_head / m_current が nullptr のまま（使用不可状態）
    }

    // 初期ページを即座に確保して準備しておく
    if (!addPage()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PagedAllocator: 初期ページの確保に失敗しました (ページサイズ: %zu KB)",
                     pageSize / 1024);
        return; // m_head / m_current が nullptr のまま（使用不可状態）
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "PagedAllocator: 初期化完了 (ページサイズ: %zu KB)",
                pageSize / 1024);
}

PagedAllocator::~PagedAllocator() {
    // 全ページをバッキングアロケーターに返却する
    PageHeader* page = m_head;
    while (page) {
        PageHeader* next = page->next;
        m_backing.deallocate(page);
        page = next;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "PagedAllocator: 解放完了 (返却ページ数: %zu)", m_pageCount);

    m_head = m_current = nullptr;
    m_pageCount = 0;
}

void* PagedAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) return nullptr;

    // ページサイズがヘッダサイズ以下の場合は割り当て不可
    if(m_pageSize <= kHeaderSize) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PagedAllocator: ページサイズがヘッダサイズ以下のため割り当て不可");
        return nullptr;
    }

    // alignment は 2 のべき乗かつ 1 以上でなければならない
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PagedAllocator: alignment は2のべき乗でなければなりません (%zu)", alignment);
        return nullptr;
    }

    // FreeListAllocator と同様に、alignment の上限は 255 に制限する
    if (alignment > 255) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PagedAllocator: alignment(%zu) がサポート上限(255)を超えています",
                     alignment);
        return nullptr;
    }
    const size_t capacity = m_pageSize - kHeaderSize;

    // size がページの収容可能サイズを超えている場合は対応不可
    if (size > capacity) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PagedAllocator: 要求サイズ(%zu)がページ収容可能サイズ(%zu)を超えています",
                     size, capacity);
        return nullptr;
    }

    // alignment による最大パディング分（alignment-1）を含めてもページに収まらない場合はエラー
    // size <= capacity を確認済みなので (capacity - size) はアンダーフローしない
    if ((alignment - 1) > capacity - size) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PagedAllocator: 要求サイズ(%zu)とアライメント(%zu)の最大パディングがページ収容可能サイズ(%zu)を超えます",
                     size, alignment, capacity);
        return nullptr;
    }

    if (!m_current) {
        if (!addPage()) return nullptr;
    }

    // アライメント調整して割り当て可能か確認する
    auto tryAlloc = [&](PageHeader* page) -> void* {
        std::byte* base    = pagePayload(page);
        size_t     addr    = reinterpret_cast<size_t>(base) + page->offset;
        size_t     aligned = (addr + alignment - 1) & ~(alignment - 1);
        size_t     padding = aligned - addr;
        size_t     needed  = padding + size;

        if (page->offset + needed > page->capacity) return nullptr;

        page->offset  += needed;
        m_usedBytes   += needed;
        return reinterpret_cast<void*>(aligned);
    };

    // 現ページで割り当てを試みる
    if (void* ptr = tryAlloc(m_current)) return ptr;

    // 現ページが足りなければ新しいページを追加して再試行
    if (!addPage()) return nullptr;
    return tryAlloc(m_current);
}

void PagedAllocator::reset() {
    if (!m_head) return;

    // 先頭ページ以外をバッキングアロケーターに返却する
    PageHeader* page = m_head->next;
    while (page) {
        PageHeader* next = page->next;
        m_backing.deallocate(page);
        page = next;
        m_pageCount--;
    }

    // 先頭ページのオフセットをリセットして再利用する
    m_head->next   = nullptr;
    m_head->offset = 0;
    m_current      = m_head;
    m_usedBytes    = 0;
}

size_t PagedAllocator::getTotalCapacity() const {
    size_t total = 0;
    for (PageHeader* p = m_head; p != nullptr; p = p->next) {
        total += p->capacity;
    }
    return total;
}

float PagedAllocator::getUsageRatio() const {
    size_t cap = getTotalCapacity();
    return cap > 0 ? static_cast<float>(m_usedBytes) / cap : 0.0f;
}

bool PagedAllocator::addPage() {
    void* buf = m_backing.allocate(m_pageSize, alignof(std::max_align_t));
    if (!buf) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PagedAllocator: バッキングアロケーターからのページ確保に失敗 "
                     "(ページサイズ: %zu KB, 現在 %zu ページ)",
                     m_pageSize / 1024, m_pageCount);
        return false;
    }

    // バッファ先頭にページヘッダを埋め込む（placement new）
    auto* header    = new(buf) PageHeader{};
    header->next     = nullptr;
    header->capacity = m_pageSize - kHeaderSize;
    header->offset   = 0;

    // リンクリストの末尾に追加する
    if (!m_head) {
        m_head    = header;
        m_current = header;
    } else {
        m_current->next = header;
        m_current       = header;
    }

    m_pageCount++;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "PagedAllocator: ページ追加 (ページ番号: %zu, 容量: %zu KB)",
                m_pageCount, header->capacity / 1024);

    return true;
}

} // namespace mk::memory
