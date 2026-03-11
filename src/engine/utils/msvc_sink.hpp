#pragma once
// Visual Studio の「出力」ウィンドウへ spdlog のログを転送するカスタムシンク。
// UTF-8 文字列を UTF-16 に変換して OutputDebugStringW に渡すため日本語も正しく表示される。
// Windows 専用。非 Windows 環境ではこのヘッダーをインクルードしないこと。
#ifdef _WIN32

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/log_msg.h>
#include <mutex>
#include <string>
#include <Windows.h>

namespace mk {

// スレッドセーフ版（_mt）のみ提供する。
// spdlog の base_sink<Mutex> を継承することで flush() / set_pattern() 等が自動的に機能する。
template<typename Mutex>
class msvc_sink final : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // デバッガーの接続時だけ出力する
        if (::IsDebuggerPresent()) {
            // base_sink が保持する formatter_ でフォーマット済み文字列を生成する
            spdlog::memory_buf_t buf;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, buf);
            buf.push_back('\0');
            // UTF-8 → UTF-16 変換して OutputDebugStringW に渡す
            // OutputDebugStringA は ANSI 文字列を期待するため日本語が文字化けする
            const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, buf.data(), -1, nullptr, 0);
            if (wlen > 0) {
                std::wstring wbuf(static_cast<std::size_t>(wlen), L'\0');
                ::MultiByteToWideChar(CP_UTF8, 0, buf.data(), -1, wbuf.data(), wlen);
                ::OutputDebugStringW(wbuf.c_str());
            }
        }
    }

    void flush_() override {
        // OutputDebugStringW は即時出力のため flush 処理は不要
    }
};

// 利便性のためエイリアスを提供
using msvc_sink_mt = msvc_sink<std::mutex>;
//using msvc_sink_st = msvc_sink<spdlog::details::null_mutex>;

} // namespace mk

#endif // _WIN32
