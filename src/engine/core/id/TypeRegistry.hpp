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
#include "TypeId.hpp"
#include "Name.hpp"

#include <cassert>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>

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
    //
    // ※ name には NameTable::make() で生成した「インターン済み Name」を必ず渡すこと。
    //    NameTable を経由せずに生成した Name（未インターンの Name）を渡すと、
    //    実装側ではエラー（assert / CORE_ERROR）として扱われる。
    //
    // 無効な入力値が渡された場合の挙動:
    //   - id == 0
    //   - name が無効（未初期化など、Name::isValid() が false を返す状態）
    //   - name が NameTable にインターンされていない（NameTable::exists(name) が false）
    //   - size == 0
    //   - alignment が不正（0 や想定外の値など、実装側で不正と判定される状態）
    //   いずれかに該当する場合は登録を行わず:
    //     Debug: assert で停止（継続した場合は CORE_ERROR ログを出力し、0 を返す）
    //     Release: CORE_ERROR ログを出力し、0 を返す
    //   ※ 呼び出し側は戻り値 0 を「登録失敗」として扱うこと
    //   ※ name は事前に NameTable にインターン済み（NameTable::exists(name) が true）である必要がある
    //
    // 既に登録済みの TypeId を渡した場合の挙動:
    //   - 登録済みのメタデータ（Name / size / alignment）と完全に一致する場合:
    //       Debug/Release: CORE_WARN ログを出力し、既存の TypeId を返す（再登録は冪等）
    //   - 登録済みのメタデータと異なる場合:
    //       Debug: assert で停止（継続した場合は CORE_ERROR ログを出力し、0 を返す）
    //       Release: CORE_ERROR ログを出力し、0 を返す
    //
    // Name が既に別の TypeId に紐づいている場合（Name 衝突）の挙動:
    //   Debug: assert で停止（継続した場合は CORE_ERROR ログを出力し、既存の TypeId を返す）
    //   Release: CORE_ERROR ログを出力し、既存の TypeId を返す
    //   ※ 戻り値が引数 id と異なる場合、呼び出し側は「登録失敗（Name 衝突）」として扱うこと
    TypeId registerType(TypeId id, Name name, size_t size, size_t alignment);
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // 検索（未登録なら nullptr）
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

    // まず共有ロックで既登録チェック（読み取りのみ）を行う。
    // 既に登録済みであれば、この時点で早期リターンすることで
    // 不要な型名生成（NameTable ロック/検索）を避ける。
    {
        std::shared_lock lock(m_mutex);
        auto it = m_types.find(id);
        if (it != m_types.end())
        {
            const TypeInfo& existing = it->second;
            // size/alignment が一致しない場合、別の型が同じ TypeId を持っている（衝突）
            // typeId<T>() の連番採番と手動登録 registerType(id,...) が衝突した可能性がある
            const bool sizeMatch      = (existing.size == sizeof(T));
            const bool alignmentMatch = (existing.alignment == alignof(T));
            assert(sizeMatch && alignmentMatch
                   && "TypeRegistry: typeId 衝突を検出（手動登録との衝突の可能性）");
            if (!sizeMatch || !alignmentMatch)
            {
                std::fputs("TypeRegistry: typeId 衝突を検出したため異常終了します\n", stderr);
                std::abort();
            }
            return id;
        }
    }

    // ここまでで未登録が確定したため、ロック外で型名生成を行う。
    // __FUNCSIG__ から型名を抽出して NameTable に登録する（MSVC専用）
    Name name = detail::typeNameFromFuncsig(__FUNCSIG__);

    // double-checked locking: 排他ロックで再確認してから登録
    std::unique_lock lock(m_mutex);
    {
        auto it = m_types.find(id);
        if (it != m_types.end())
        {
            const TypeInfo& existing = it->second;
            const bool sizeMatch      = (existing.size == sizeof(T));
            const bool alignmentMatch = (existing.alignment == alignof(T));
            const bool nameMatch      = (existing.name == name);
            assert(sizeMatch && alignmentMatch && nameMatch
                   && "TypeRegistry: typeId 衝突を検出（手動登録との衝突の可能性 / name 不一致）");
            if (!sizeMatch || !alignmentMatch || !nameMatch)
            {
                std::fputs("TypeRegistry: typeId 衝突を検出したため異常終了します\n", stderr);
                std::abort();
            }
            return id;
        }
    }

    TypeInfo info;
    info.id        = id;
    info.name      = name;
    info.size      = sizeof(T);
    info.alignment = alignof(T);

    m_types.emplace(id, info);

    // emplace の戻り値を確認して Name 衝突を検出する
    // Logger マクロはヘッダでも利用可能だが、この層では依存を増やしたくないため利用しない。
    // 衝突は assert と std::abort() によって検出・停止し、致命的な不整合を即座に報告する。
    // 例外安全: m_nameIndex.emplace が例外を投げた場合は m_types をロールバックする
    try
    {
        const bool nameInserted = m_nameIndex.emplace(name.getId(), id).second;
        if (!nameInserted)
        {
            // m_types をロールバックして両インデックスの整合性を保つ
            m_types.erase(id);
            // テンプレート版では typeId<T>() が型ごとに一意なIDを生成する。
            // 同一 Name で異なる TypeId が衝突した場合は型名ハッシュの衝突であり、
            // エンジンの根本的な不整合を示すため、Release ビルドでも即座に終了する。
            // NDEBUG 時には assert が無効化されるため、std::abort() の前に理由を stderr に出力する
            std::fputs("TypeRegistry: 同一 Name で異なる TypeId の登録を検出したため、異常終了します。\n", stderr);
            assert(false && "TypeRegistry: 同一 Name で異なる TypeId の登録を検出");
            std::abort();
        }
    }
    catch (...)
    {
        // m_nameIndex.emplace が bad_alloc 等を投げた場合にロールバックして再送出
        m_types.erase(id);
        throw;
    }

    return id;
}

} // namespace mk
