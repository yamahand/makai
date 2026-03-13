#pragma once
// TypeRegistry — ランタイム型情報管理システム（シングルトン）
//
// 責務:
//   - TypeInfo の登録と検索
//   - テンプレート版 registerType<T>() による自動登録
//   - 手動登録 registerType(TypeId, Name, size, alignment) の提供
//
// スレッドセーフ:
//   - 読み取り（findType）は shared_lock（並列読み取り可能）
//   - 書き込み（registerType）は unique_lock
//   - double-checked locking でテンプレート版の登録コストを最小化
//
// 型名の取得:
//   - テンプレート版は MSVC の __FUNCSIG__ から型名を抽出して NameTable に登録する
//   - 型名は "struct"/"class" プレフィックスを除去した完全修飾名
//
// 使用例:
//   auto& reg = TypeRegistry::instance();
//   TypeId id = reg.registerType<PlayerComponent>();
//   const TypeInfo* info = reg.findType(id);
//   const TypeInfo* info2 = reg.findType(Name("mk::PlayerComponent"));

#include "TypeInfo.hpp"
#include "StringId.hpp"

#include <cassert>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <cstdlib>

namespace mk {

namespace detail {
// __FUNCSIG__ から型名を抽出して NameTable に登録する（TypeRegistry.cpp に実装）
Name typeNameFromFuncsig(const char* funcsig);
} // namespace detail

class TypeRegistry
{
public:
    // シングルトンアクセス
    static TypeRegistry& instance();

    // ------------------------------------------------------------------
    // テンプレート登録（推奨）
    // 既に登録済みの場合は冪等（既存の TypeId をそのまま返す）
    // ------------------------------------------------------------------
    template<typename T>
    TypeId registerType();

    // ------------------------------------------------------------------
    // 手動登録
    // 既に登録済みの TypeId を渡した場合:
    //   Debug: assert で停止
    //   Release: CORE_WARN ログを出力し、既存の TypeId を返す
    // ------------------------------------------------------------------
    TypeId registerType(TypeId id, Name name, size_t size, size_t alignment);

    // ------------------------------------------------------------------
    // 検索（未登録なら nullptr）
    // ------------------------------------------------------------------
    const TypeInfo* findType(TypeId id) const;
    const TypeInfo* findType(Name name) const;

    // 登録済み型の数
    size_t typeCount() const;

    // コピー・ムーブ禁止
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;
    TypeRegistry(TypeRegistry&&) = delete;
    TypeRegistry& operator=(TypeRegistry&&) = delete;

private:
    TypeRegistry() = default;
    ~TypeRegistry() = default;

    // TypeId → TypeInfo（主インデックス）
    std::unordered_map<TypeId, TypeInfo> m_types;

    // StringID → TypeId（名前による逆引きインデックス）
    std::unordered_map<StringID, TypeId> m_nameIndex;

    // 読み取り多数・書き込み少数の用途に shared_mutex を使用
    mutable std::shared_mutex m_mutex;
};

// --------------------------------------------------------------------------
// テンプレート実装（ヘッダ内定義が必要）
// --------------------------------------------------------------------------
template<typename T>
TypeId TypeRegistry::registerType()
{
    // typeId<T>() は C++11 以降の static local 保証でスレッドセーフ
    const TypeId id = typeId<T>();

    // 共有ロックで既登録チェック（読み取りのみ）
    {
        std::shared_lock lock(m_mutex);
        if (m_types.contains(id))
        {
            return id;
        }
    }

    // double-checked locking: 排他ロックで再確認してから登録
    std::unique_lock lock(m_mutex);
    if (m_types.contains(id))
    {
        return id;
    }

    // __FUNCSIG__ から型名を抽出して NameTable に登録する（MSVC専用）
    Name name = detail::typeNameFromFuncsig(__FUNCSIG__);

    TypeInfo info;
    info.id        = id;
    info.name      = name;
    info.size      = sizeof(T);
    info.alignment = alignof(T);

    m_types.emplace(id, info);

    // emplace の戻り値を確認して Name 衝突を検出する
    // ヘッダ内テンプレートのため Logger は使えない。衝突は assert と abort で検出・停止する
    auto [nameIt, nameInserted] = m_nameIndex.emplace(name.getId(), id);
    if (!nameInserted)
    {
        // m_types をロールバックして両インデックスの整合性を保つ
        m_types.erase(id);
        // この時点で致命的な不整合が発生しているため、Release ビルドでも必ず異常終了する
        assert(false && "TypeRegistry: 同一 Name で異なる TypeId の登録を検出");
        std::abort();
    }

    return id;
}

} // namespace mk
