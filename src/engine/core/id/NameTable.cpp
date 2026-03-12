// NameTable の実装
#include "NameTable.hpp"

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

    // 既に登録済みならそのまま返す
    auto it = m_indexMap.find(id);
    if (it != m_indexMap.end())
    {
        return Name(id);
    }

    // 新規登録
    uint32_t index = static_cast<uint32_t>(m_entries.size());
    m_entries.push_back(NameEntry{id, std::string(str)});
    m_indexMap.emplace(id, index);

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
