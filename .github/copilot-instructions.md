# makai — Copilot Instructions

## Build

```bash
cmake -S . -B build
cmake --build build
.\build\Debug\makai.exe
```

**Prerequisites:** Place `SDL3_image-devel-3.4.0-VC.zip` contents into `external/SDL3_image-3.4.0/` before first build (download from https://github.com/libsdl-org/SDL_image/releases). SDL3, SDL3_ttf, ImGui, nlohmann/json, and GLM are fetched automatically via FetchContent.

There are no automated tests or linters.

## Architecture

```
Game
├── Window            (SDL3 window/renderer wrapper)
├── Config            (JSON config loader — config.json)
├── FontManager       (SDL3_ttf font cache)
├── TextureManager    (SDL3_image texture cache)
├── ImGuiManager      (Dear ImGui integration)
└── SceneManager      (stack-based scene manager)
    └── Stack<Scene>
        └── GameScene
            └── Player (GameObject + PlayerStats + StateMachine)
                └── StateMachine → State (Idle/Working/Resting/Eating)
```

**Game loop order (Game::run):** `processEvents()` → `update(delta)` → `render()` → `applyPendingChanges()`

Delta time is capped at 50ms. Game time: 0.5 real seconds = 1 game hour (`config.json: game.time_scale`).

## Scene System

- `SceneManager` maintains a stack of `Scene` objects.
- **CRITICAL:** Never modify the stack directly during `update()` or `render()`. Always use the deferred commands `push`, `pop`, `replace`, or `clear`; they are applied via `applyPendingChanges()` after the frame.
- Scene lifecycle hooks: `onEnter()`, `onExit()`, `onResume()`.
- Current flow: `TitleScene` → `GameScene`.

## State Machine

`StateMachine` (in `src/states/`) is generic — it owns the current `State` via `unique_ptr` and calls `onExit()`/`onEnter()` on transitions. Player states: `IdleState`, `WorkingState`, `RestingState`, `EatingState`. Each state auto-transitions back to `IdleState` when its timer completes.

## Coding Conventions

- Namespace: `makai::`
- Member variables: `m_` prefix (e.g., `m_running`, `m_scenes`)
- Use smart pointers; `new`/`delete` are prohibited
- Comments must be written in Japanese
- MSVC `/utf-8` flag is set so Japanese comments compile correctly on Windows
