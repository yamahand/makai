#pragma once
// HandlePool — Handle ベースのオブジェクト管理プール
//
// 固定容量の配列でオブジェクトを管理し、世代番号（generation）で
// 無効ハンドルを安全に検出する。
//
// 特徴:
//   - create() / destroy() / get() の3操作のみ
//   - destroy() 後に generation をインクリメントするため古いハンドルは自動無効化
//   - フリーリストを配列スタックで管理し O(1) の create / destroy を実現
//   - ヘッダオンリー（テンプレートのため）
//
// 設計上の注意:
//   - get() が返すポインタは「対応するスロットのオブジェクトが alive の間」に限り有効
//   - 他スロットに対する create() / destroy() ではオブジェクトは移動せず、ポインタの指すアドレスも変化しない
//   - 同一スロットに対して destroy() を行った時点でそのオブジェクトのライフタイムは終了し、
//       それ以前に取得したポインタはダングリングとなるため以後使用してはならない
//   - destroy() 後に同じスロットへ新しいオブジェクトが構築されても、ポインタは get() により再取得すること
//   - スレッドセーフではない（外部でロックすること）
//
// 使用例:
//   HandlePool<Texture, TextureTag, 512> pool;
//   TextureHandle h = pool.create("player.png");
//   Texture* tex = pool.get(h);   // 有効なら非null
//   pool.destroy(h);
//   tex = pool.get(h);            // nullptr（古いハンドル）

#include "Handle.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <cstdlib>

namespace mk {

template<typename T, typename Tag, uint32_t Capacity = 256>
class HandlePool
{
public:
    using HandleType = Handle<Tag>;

    // Capacity は 0 を許可しない（ゼロ長配列を防ぐため）
    static_assert(Capacity > 0, "HandlePool Capacity must be greater than 0.");

    HandlePool();
    ~HandlePool();

    // コピー・ムーブ禁止（固定配列を持つため）
    HandlePool(const HandlePool&) = delete;
    HandlePool& operator=(const HandlePool&) = delete;
    HandlePool(HandlePool&&) = delete;
    HandlePool& operator=(HandlePool&&) = delete;

    // ------------------------------------------------------------------
    // create — オブジェクトを生成してハンドルを返す
    // プールが満杯の場合は無効ハンドル（value == 0）を返す
    // ------------------------------------------------------------------
    template<typename... Args>
    HandleType create(Args&&... args);

    // ------------------------------------------------------------------
    // destroy — ハンドルを無効化してオブジェクトを破棄する
    // 無効なハンドルや古いハンドルは無視する
    // ------------------------------------------------------------------
    void destroy(HandleType handle);

    // ------------------------------------------------------------------
    // get — ハンドルからポインタを取得する
    // 無効なハンドル・古いハンドルの場合は nullptr を返す
    // ------------------------------------------------------------------
    T*       get(HandleType handle);
    const T* get(HandleType handle) const;

    // ------------------------------------------------------------------
    // 状態照会
    // ------------------------------------------------------------------
    size_t   size()     const { return m_size; }
    bool     isEmpty()  const { return m_size == 0; }
    bool     isFull()   const { return m_size >= Capacity; }
    uint32_t capacity() const { return Capacity; }

private:
    // スロット: オブジェクト本体のストレージ + 世代番号 + 生存フラグ
    struct Slot
    {
        alignas(T) std::byte storage[sizeof(T)];
        uint32_t generation; // 1始まり（0はvalue==0の無効ハンドルと衝突するため）
        bool     alive;

        // ストレージは未初期化のままとし、メタデータのみ初期化する
        Slot() noexcept
            : generation(1)
            , alive(false)
        {
        }
    };

    // ハンドルの検証（共通処理）
    bool isValidHandle(HandleType handle) const;

    Slot     m_slots[Capacity];
    uint32_t m_freeStack[Capacity]; // 空きインデックスのスタック
    uint32_t m_freeTop             = 0;
    size_t   m_size                = 0;
};

// ===========================================================================
// テンプレート実装
// ===========================================================================

template<typename T, typename Tag, uint32_t Capacity>
HandlePool<T, Tag, Capacity>::HandlePool()
{
    // フリースタックを逆順で初期化（index 0 が最初に使われるように）
    for (uint32_t i = 0; i < Capacity; ++i)
    {
        m_freeStack[i] = Capacity - 1 - i;
    }
    m_freeTop = Capacity;
}

template<typename T, typename Tag, uint32_t Capacity>
HandlePool<T, Tag, Capacity>::~HandlePool()
{
    // 生存中のオブジェクトをすべて破棄する
    for (uint32_t i = 0; i < Capacity; ++i)
    {
        if (m_slots[i].alive)
        {
            std::destroy_at(std::launder(reinterpret_cast<T*>(m_slots[i].storage)));
            m_slots[i].alive = false;
        }
    }
}

template<typename T, typename Tag, uint32_t Capacity>
template<typename... Args>
Handle<Tag> HandlePool<T, Tag, Capacity>::create(Args&&... args)
{
    if (m_freeTop == 0)
    {
        // プール枯渇: 無効ハンドルを返す
        // assert(false && "HandlePool: プールが満杯です");
        return HandleType{};
    }

    // フリースタックから空きインデックスを取得する（例外安全のためデクリメントは構築成功後に行う）
    const uint32_t index = m_freeStack[m_freeTop - 1];
    Slot& slot = m_slots[index];

    assert(!slot.alive && "HandlePool: 内部不整合（alive なスロットが freeStack に含まれている）");

    // placement new でオブジェクトを構築する
    ::new (slot.storage) T(std::forward<Args>(args)...);
    slot.alive = true;
    ++m_size;
    --m_freeTop;

    return detail::makeHandle<Tag>(index, slot.generation);
}

template<typename T, typename Tag, uint32_t Capacity>
void HandlePool<T, Tag, Capacity>::destroy(HandleType handle)
{
    if (!isValidHandle(handle))
    {
        return;
    }

    const uint32_t index = handle.index();
    Slot& slot = m_slots[index];

    // デストラクタを明示的に呼ぶ
    // std::launder: placement new 後のポインタ取得に必要（UB回避）
    std::destroy_at(std::launder(reinterpret_cast<T*>(slot.storage)));
    slot.alive = false;

    // generation をインクリメントして古いハンドルを無効化する
    ++slot.generation;

    // generation のオーバーフロー検出（0 になると無効ハンドルと衝突する）
    // 発生した場合はハンドル安全性が保証できないため、Release ビルドでも fail-fast する
    assert(slot.generation != 0 && "HandlePool: generation がオーバーフローしました");
    if (slot.generation == 0)
    {
        // TypeId と同様に、理論上到達不能だが到達した場合は致命的エラーとして即時終了する
        std::abort();
    }

    // フリースタックに返却する
    m_freeStack[m_freeTop++] = index;
    --m_size;
}

template<typename T, typename Tag, uint32_t Capacity>
T* HandlePool<T, Tag, Capacity>::get(HandleType handle)
{
    if (!isValidHandle(handle))
    {
        return nullptr;
    }
    // std::launder: placement new 後のポインタ取得に必要（UB回避）
    return std::launder(reinterpret_cast<T*>(m_slots[handle.index()].storage));
}

template<typename T, typename Tag, uint32_t Capacity>
const T* HandlePool<T, Tag, Capacity>::get(HandleType handle) const
{
    if (!isValidHandle(handle))
    {
        return nullptr;
    }
    // std::launder: placement new 後のポインタ取得に必要（UB回避）
    return std::launder(reinterpret_cast<const T*>(m_slots[handle.index()].storage));
}

template<typename T, typename Tag, uint32_t Capacity>
bool HandlePool<T, Tag, Capacity>::isValidHandle(HandleType handle) const
{
    if (!handle.isValid())
    {
        return false;
    }

    const uint32_t index = handle.index();
    if (index >= Capacity)
    {
        return false;
    }

    const Slot& slot = m_slots[index];
    return slot.alive && (slot.generation == handle.generation());
}

} // namespace mk
