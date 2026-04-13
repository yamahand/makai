#pragma once
#include <SDL3/SDL.h>
#include "../core/log/Assert.hpp"

namespace mk {

// SDL の初期化・終了を RAII で管理するガード
// 初期化失敗は致命的エラーとして abort する
class SdlGuard {
public:
    explicit SdlGuard(SDL_InitFlags flags) {
        MK_VERIFY_MSG(SDL_Init(flags), "SDL_Init failed: {}", SDL_GetError());
    }
    ~SdlGuard() {
        SDL_Quit();
    }

    SdlGuard(const SdlGuard&) = delete;
    SdlGuard& operator=(const SdlGuard&) = delete;
    SdlGuard(SdlGuard&&) = delete;
    SdlGuard& operator=(SdlGuard&&) = delete;
};

} // namespace mk
