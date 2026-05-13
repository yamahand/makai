// NameTable の実装
#include "NameTable.hpp"
#include "engine/core/log/Logger.hpp"

#include <cassert>

namespace mk {

NameTable& NameTable::instance()
{
    static NameTable s_instance;
    return s_instance;
}

Name NameTable::make(const char* str)
{
    if (str == nullptr)
    {
        return Name();
    }

    StringID id = sid(str);

    std::lock_guard<std::mutex> lock(m_mutex);

    // 同一 StringID が既に登録されているか確認する
    auto it = m_indexMap.find(id);
    if (it != m_indexMap.end())
    {
        const auto& existing = m_entries[it->second].string;

        // ハッシュ衝突検出: 異なる文字列が同一 StringID にマッピングされた場合
        if (existing != str)
        {
            // Debug ビルドでは即座に停止して開発者に衝突を通知する
            assert(false && "NameTable: StringID ハッシュ衝突を検出");

            // Release ビルドではログを出力し、先に登録された文字列の Name を返す
            CORE_ERROR("NameTable: ハッシュ衝突を検出 — "
                       "StringID={:#010x} 既存=\"{}\" 新規=\"{}\"",
                       id, existing, str);
        }

        return Name(id);
    }

    // 新規登録
    // 例外無効環境では push_back / emplace の失敗（OOM）はランタイムにより abort される
    m_entries.push_back(NameEntry{id, std::string(str)});
    m_indexMap.emplace(id, static_cast<uint32_t>(m_entries.size() - 1));

    return Name(id);
}

const char* NameTable::toString(Name name) const
{
    if (!name.isValid())
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_indexMap.find(name.getId());
    if (it == m_indexMap.end())
    {
        return nullptr;
    }

    return m_entries[it->second].string.c_str();
}

bool NameTable::exists(Name name) const
{
    if (!name.isValid())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    return m_indexMap.contains(name.getId());
}

size_t NameTable::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

} // namespace mk
