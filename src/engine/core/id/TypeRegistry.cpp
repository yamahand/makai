// TypeRegistry の実装
#include "TypeRegistry.hpp"
#include "NameTable.hpp"
#include "engine/utils/Logger.hpp"

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
    assert(id != 0        && "TypeRegistry: 無効な TypeId (0) は登録できない");
    assert(name.isValid() && "TypeRegistry: 無効な Name は登録できない");

    std::unique_lock lock(m_mutex);

    if (m_types.contains(id))
    {
        assert(false && "TypeRegistry: 既に登録済みの TypeId を再登録しようとした");
        CORE_WARN("TypeRegistry: TypeId={} は既に登録済み — 既存の登録を維持する", id);
        return id;
    }

    TypeInfo info;
    info.id        = id;
    info.name      = name;
    info.size      = size;
    info.alignment = alignment;

    m_types.emplace(id, info);

    // emplace の戻り値を確認して Name 衝突を検出する
    const char* nameStr = NameTable::instance().toString(name);
    auto [nameIt, nameInserted] = m_nameIndex.emplace(name.getId(), id);
    if (!nameInserted)
    {
        assert(false && "TypeRegistry: 同一 Name で異なる TypeId の登録を検出");
        CORE_WARN("TypeRegistry: Name の衝突を検出 — name=\"{}\" 既存 TypeId={} 新規 TypeId={}",
                  nameStr ? nameStr : "(unknown)", nameIt->second, id);
    }

    // toString() が nullptr を返す場合に備えてフォールバック文字列を使用する
    CORE_INFO("TypeRegistry: 型を登録 — id={} name=\"{}\" size={} align={}",
              id,
              nameStr ? nameStr : "(unknown)",
              size,
              alignment);

    return id;
}

const TypeInfo* TypeRegistry::getType(TypeId id) const
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
