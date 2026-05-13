// TypeRegistry の実装
#include "TypeRegistry.hpp"
#include "NameTable.hpp"
#include "engine/core/log/Logger.hpp"

#include <cassert>
#include <string>
#include <string_view>

namespace mk {

// ===========================================================================
// 無名 namespace — このTU内部でのみ使用するヘルパー
// ===========================================================================
namespace {

// __FUNCSIG__ から型名を抽出する（MSVC専用）
//
// 入力例:
//   "mk::TypeId __cdecl mk::TypeRegistry::registerType<struct mk::PlayerComponent>(void)"
//
// 抽出ロジック:
//   1. 最初の "<" を探す（registerType< の "<"）
//   2. 入れ子カウントを考慮しながら対応する ">" を探す
//   3. その間の文字列が型名（"struct mk::PlayerComponent" 等）
//   4. 先頭の "struct " / "class " / "enum " / "union " プレフィックスを除去する
std::string extractTypeNameFromFuncsig(const char* funcsig)
{
    std::string_view sv(funcsig);

    const auto ltPos = sv.find('<');
    if (ltPos == std::string_view::npos)
    {
        return std::string(sv); // パース失敗: 生のシグネチャを返す
    }

    // 入れ子カウントで対応する ">" を探す
    size_t depth     = 0;
    size_t nameStart = ltPos + 1;
    size_t nameEnd   = std::string_view::npos;
    for (size_t i = ltPos; i < sv.size(); ++i)
    {
        if      (sv[i] == '<') ++depth;
        else if (sv[i] == '>')
        {
            --depth;
            if (depth == 0)
            {
                nameEnd = i;
                break;
            }
        }
    }

    if (nameEnd == std::string_view::npos)
    {
        return std::string(sv); // パース失敗
    }

    std::string typeName(sv.substr(nameStart, nameEnd - nameStart));

    // 先頭の "struct " / "class " / "enum " / "union " を除去する
    for (const char* prefix : {"struct ", "class ", "enum ", "union "})
    {
        std::string_view p(prefix);
        if (typeName.starts_with(p))
        {
            typeName.erase(0, p.size());
            break;
        }
    }

    return typeName;
}

} // 無名 namespace

// ===========================================================================
// detail::typeNameFromFuncsig — ヘッダから参照される実装
// ===========================================================================
namespace detail {

Name typeNameFromFuncsig(const char* funcsig)
{
    const std::string typeName = extractTypeNameFromFuncsig(funcsig);
    return NameTable::instance().make(typeName.c_str());
}

} // namespace detail

// ===========================================================================
// TypeRegistry 実装
// ===========================================================================

TypeRegistry& TypeRegistry::instance()
{
    static TypeRegistry s_instance;
    return s_instance;
}

TypeId TypeRegistry::registerType(
    TypeId id, Name name, size_t size, size_t alignment)
{
    // Release ビルドでも不正な入力は必ず弾く
    if (id == 0)
    {
        assert(false && "TypeRegistry: 無効な TypeId (0) は登録できない");
        CORE_ERROR("TypeRegistry: 無効な TypeId (0) は登録できない — 登録を拒否します");
        return 0;
    }
    if (!name.isValid())
    {
        assert(false && "TypeRegistry: 無効な Name は登録できない");
        CORE_ERROR("TypeRegistry: 無効な Name は登録できない — 登録を拒否します");
        return 0;
    }
    // Name は NameTable でインターン済みでなければならない
    if (!NameTable::instance().exists(name))
    {
        assert(false && "TypeRegistry: NameTable に未登録の Name は登録できない");
        const char* typeName = name.toString();
        CORE_ERROR("TypeRegistry: NameTable に未登録の Name は登録できない — 登録を拒否します (Name={}, TypeId={})",
                   typeName ? typeName : "(unregistered)", id);
        return 0;
    }
    if (size == 0)
    {
        // サイズ 0 の型は登録できない（不正な TypeInfo を防ぐ）
        assert(false && "TypeRegistry: size==0 の型は登録できない");
        const char* typeName = name.toString();
        CORE_ERROR("TypeRegistry: size==0 の型は登録できない — 登録を拒否します (Name={}, TypeId={})",
                   typeName ? typeName : "(unregistered)", id);
        return 0;
    }
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        // alignment は 0 ではなく 2 の冪でなければならない
        assert(false && "TypeRegistry: 無効な alignment 値 (0 または 2 の冪ではない) は登録できない");
        const char* typeName = name.toString();
        CORE_ERROR("TypeRegistry: 無効な alignment={} は登録できない — 登録を拒否します (Name={}, TypeId={})",
                   alignment, typeName ? typeName : "(unregistered)", id);
        return 0;
    }

    std::unique_lock lock(m_mutex);

    auto it = m_types.find(id);
    if (it != m_types.end())
    {
        const TypeInfo& existing = it->second;

        const bool sameName      = (existing.name == name);
        const bool sameSize      = (existing.size == size);
        const bool sameAlignment = (existing.alignment == alignment);

        if (!sameName || !sameSize || !sameAlignment)
        {
            // ログ出力に必要な情報を退避してからロックを解放する
            const auto existingName      = existing.name;
            const auto existingSize      = existing.size;
            const auto existingAlignment = existing.alignment;
            const auto newName           = name;


            // TypeRegistry のロックを保持したまま Name::toString() を呼ぶと
            // NameTable 側のロック取得との順序逆転でデッドロックする可能性があるため、
            // ここで明示的にロックを解放してから文字列化とログ出力を行う。
            lock.unlock();

            const char* existingNameCStr = existingName.toString();
            const char* newNameCStr      = newName.toString();

            // 同一 TypeId に対して異なるメタデータが登録されようとしているのはバグ
            assert(false && "TypeRegistry: 同一 TypeId に異なるメタデータを再登録しようとした");
            CORE_ERROR(
                "TypeRegistry: TypeId={} は既に登録済みですが、異なるメタデータで再登録されました。\n"
                "  既存: Name={}, size={}, alignment={}\n"
                "  新規: Name={}, size={}, alignment={}",
                id,
                existingNameCStr ? existingNameCStr : "(unregistered)", existingSize, existingAlignment,
                newNameCStr ? newNameCStr : "(unregistered)", size, alignment);

                return 0;
        }
        else
        {
            // 同一メタデータでの再登録は冪等として扱う
            CORE_WARN("TypeRegistry: TypeId={} は既に同一メタデータで登録済み — 再登録要求を無視します", id);
            return id;
        }
    }

    TypeInfo info;
    info.id        = id;
    info.name      = name;
    info.size      = size;
    info.alignment = alignment;

    m_types.emplace(id, info);

    // emplace の戻り値を確認して Name 衝突を検出する
    // 例外無効環境では emplace の失敗（OOM）はランタイムにより abort されるためロールバック不要
    const auto nameResult = m_nameIndex.emplace(name.getId(), id);
    if (!nameResult.second)
    {
        // m_types をロールバックして両インデックスの整合性を保つ
        m_types.erase(id);
        // 手動登録での Name 衝突はプログラマーのミス（同一名を別 TypeId で登録しようとした）。
        // 仕様通り 0 を返して「登録失敗」を通知する。
        // 既存の TypeId を知りたい場合は findType(name) を使うこと。
        assert(false && "TypeRegistry: 同一 Name で異なる TypeId の登録を検出");
        CORE_ERROR("TypeRegistry: Name の衝突を検出 — 登録失敗 nameId={} 既存 TypeId={} 新規 TypeId={}",
                   name.getId(), nameResult.first->second, id);
        return 0;
    }

    CORE_INFO("TypeRegistry: 型を登録 — id={} nameId={} size={} align={}",
              id,
              name.getId(),
              size,
              alignment);

    return id;
}

const TypeInfo* TypeRegistry::findType(TypeId id) const
{
    std::shared_lock lock(m_mutex);

    const auto it = m_types.find(id);
    return (it != m_types.end()) ? &it->second : nullptr;
}

const TypeInfo* TypeRegistry::findType(Name name) const
{
    if (!name.isValid())
    {
        return nullptr;
    }

    std::shared_lock lock(m_mutex);

    const auto nameIt = m_nameIndex.find(name.getId());
    if (nameIt == m_nameIndex.end())
    {
        return nullptr;
    }

    const auto typeIt = m_types.find(nameIt->second);
    if (typeIt == m_types.end())
    {
        assert(false && "TypeRegistry: 内部インデックス不整合");
        return nullptr;
    }
    return &typeIt->second;
}

size_t TypeRegistry::typeCount() const
{
    std::shared_lock lock(m_mutex);
    return m_types.size();
}

} // namespace mk
