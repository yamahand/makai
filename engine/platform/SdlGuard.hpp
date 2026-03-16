#pragma once
#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>

namespace mk {

// SDL の初期化・終了を RAII で管理するガード
class SdlGuard {
public:
    explicit SdlGuard(SDL_InitFlags flags) {
        if (!SDL_Init(flags)) {
            throw std::runtime_error(
                std::string("SDL_Init failed: ") + SDL_GetError());
        }
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
