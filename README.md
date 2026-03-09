# makai

## 動作環境

| 項目 | 要件 |
|------|------|
| OS | Windows 10/11 64bit |
| アーキテクチャ | x86-64（64bit 専用） |
| コンパイラ | MSVC 2022（C++20） |
| CMake | 3.20 以上 |

> **Note:** 32bit 環境はサポート対象外です。`MemoryManager` をはじめ一部モジュールが `sizeof(size_t) >= 8` を `static_assert` で要求します。
