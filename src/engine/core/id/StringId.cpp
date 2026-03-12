// StringID の実装
// 現在は全て constexpr でヘッダに定義されているため、このファイルは将来の拡張用
#include "StringId.hpp"

namespace mk
{
    // MSVC の静的ライブラリ生成時に LNK4221 を避けるためのダミーシンボル
    // 将来このファイルに実装を追加する場合は、この関数は削除してもよい
    void StringId_TranslationUnitAnchor()
    {
    }
}