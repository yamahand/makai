#pragma once
#include "LinearAllocator.hpp"
#include <array>
#include <memory>

namespace mk::memory {

/// DoubleFrameAllocator（ダブルバッファ版フレームアロケーター）
///
/// 2 つの LinearAllocator をフレーム毎に交互に切り替える。
/// swap() を呼ぶと新フロントがリセットされるため、
/// 前フレームに割り当てたデータに 1 フレーム間だけアクセスできる。
///
///   current()  — 現フレームの割り当て先
///   previous() — 前フレームのデータへのアクセス（読み取り専用）
///   swap()     — フレーム末に呼ぶ（フロント切り替え + 新フロントをリセット）
class DoubleFrameAllocator {
public:
    /// コンストラクタ（外部バッファ版）
    /// buf0/buf1 はそれぞれ size0/size1 バイトの外部バッファを指す。
    /// バッファの生存期間はこのオブジェクトより長くなければならない。
    DoubleFrameAllocator(void* buf0, size_t size0, void* buf1, size_t size1)
        : m_frontIndex(0)
    {
        m_allocators[0] = std::make_unique<LinearAllocator>(buf0, size0);
        m_allocators[1] = std::make_unique<LinearAllocator>(buf1, size1);
    }

    /// 現フレームのアロケーターを取得（割り当てに使う）
    LinearAllocator& current()  { return *m_allocators[m_frontIndex]; }

    /// 前フレームのアロケーターを取得（読み取り専用）
    LinearAllocator& previous() { return *m_allocators[1 - m_frontIndex]; }

    /// フレーム末に呼ぶ：フロントを切り替えて新フロントをリセット
    /// 旧フロント（前フレームデータ）は次の swap() まで previous() で参照可能
    void swap() {
        m_frontIndex = 1 - m_frontIndex;
        m_allocators[m_frontIndex]->reset();
    }

    // コピー禁止
    DoubleFrameAllocator(const DoubleFrameAllocator&) = delete;
    DoubleFrameAllocator& operator=(const DoubleFrameAllocator&) = delete;

private:
    std::array<std::unique_ptr<LinearAllocator>, 2> m_allocators;
    int m_frontIndex; ///< 現フロントのインデックス（0 または 1）
};

} // namespace mk::memory
