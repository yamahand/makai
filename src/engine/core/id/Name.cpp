// Name の実装
// 現在は全て constexpr でヘッダに定義されているため、このファイルは将来の拡張用
#include "Name.hpp"

// この翻訳単位が静的ライブラリ生成時に LNK4221 を出さないようにするためのダミーシンボル
// 将来 Name 関連の実装が追加されるまでは、この関数のみが非 inline シンボルとして存在する
namespace mk
{
    [[maybe_unused]] void NameTranslationUnitAnchor()
    {
        // 何も処理しないダミー実装
    }
}