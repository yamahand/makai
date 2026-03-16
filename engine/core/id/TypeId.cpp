// TypeId の実装
// テンプレートベースのためヘッダに定義されている。このファイルは将来の拡張用
#include "TypeId.hpp"

namespace mk
{
    // MSVC の静的ライブラリ生成時に LNK4221（no public symbols found）警告を出さないためのアンカー関数
    // 将来的にこのファイルに実装が追加された場合は、この関数は削除してもよい
    [[maybe_unused]] void TypeId_Anchor()
    {
    }
}
