// Name の実装
#include "Name.hpp"
#include "NameTable.hpp"

namespace mk
{
    const char* Name::toString() const
    {
        return NameTable::instance().toString(*this);
    }
}