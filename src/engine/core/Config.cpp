#include "Config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <SDL3/SDL_log.h>

namespace mk {

Config Config::load(const std::string& path) {
    Config config;

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                       "Config file not found: %s, using defaults", path.c_str());
            return config;
        }

        nlohmann::json j = nlohmann::json::parse(file);

        // ウィンドウ設定を別造
        if (j.contains("window")) {
            auto& w = j["window"];
            if (w.contains("title")) config.window.title = w["title"];
            if (w.contains("width")) config.window.width = w["width"];
            if (w.contains("height")) config.window.height = w["height"];
            if (w.contains("fullscreen")) config.window.fullscreen = w["fullscreen"];
            if (w.contains("vsync")) config.window.vsync = w["vsync"];
        }

        // フォント設定を別造
        if (j.contains("fonts")) {
            auto& fonts = j["fonts"];
            if (fonts.contains("default")) {
                config.defaultFont.path = fonts["default"]["path"];
                config.defaultFont.size = fonts["default"]["size"];
            }
            if (fonts.contains("large")) {
                config.largeFont.path = fonts["large"]["path"];
                config.largeFont.size = fonts["large"]["size"];
            }
        }

        // メモリ設定を解析
        if (j.contains("memory")) {
            auto& mem = j["memory"];
            if (mem.contains("frame_allocator_mb"))        config.memory.frameAllocatorMB       = mem["frame_allocator_mb"];
            if (mem.contains("scene_allocator_mb"))        config.memory.sceneAllocatorMB       = mem["scene_allocator_mb"];
            if (mem.contains("heap_allocator_mb"))         config.memory.heapAllocatorMB        = mem["heap_allocator_mb"];
            if (mem.contains("double_frame_allocator_mb")) config.memory.doubleFrameAllocatorMB = mem["double_frame_allocator_mb"];
            if (mem.contains("stack_allocator_mb"))        config.memory.stackAllocatorMB       = mem["stack_allocator_mb"];
            if (mem.contains("buddy_allocator_mb"))        config.memory.buddyAllocatorMB       = mem["buddy_allocator_mb"];
            if (mem.contains("paged_allocator_page_kb"))   config.memory.pagedAllocatorPageKB   = mem["paged_allocator_page_kb"];
        }

        // ゲーム設定を別造
        if (j.contains("game")) {
            auto& game = j["game"];
            if (game.contains("time_scale")) config.game.timeScale = game["time_scale"];
            if (game.contains("starting_money")) config.game.startingMoney = game["starting_money"];
            if (game.contains("starting_day")) config.game.startingDay = game["starting_day"];
        }

        // デバッグ設定を別造
        if (j.contains("debug")) {
            auto& dbg = j["debug"];
            if (dbg.contains("show_fps")) config.debug.showFPS = dbg["show_fps"];
            if (dbg.contains("show_imgui_demo")) config.debug.showImGuiDemo = dbg["show_imgui_demo"];
        }

        SDL_Log("Config loaded from %s", path.c_str());
    }
    catch (const std::exception& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to load config: %s", e.what());
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Using default configuration");
    }

    return config;
}

void Config::save(const std::string& path) const {
    try {
        nlohmann::json j;

        j["window"]["title"] = window.title;
        j["window"]["width"] = window.width;
        j["window"]["height"] = window.height;
        j["window"]["fullscreen"] = window.fullscreen;
        j["window"]["vsync"] = window.vsync;

        j["fonts"]["default"]["path"] = defaultFont.path;
        j["fonts"]["default"]["size"] = defaultFont.size;
        j["fonts"]["large"]["path"] = largeFont.path;
        j["fonts"]["large"]["size"] = largeFont.size;

        j["memory"]["frame_allocator_mb"]        = memory.frameAllocatorMB;
        j["memory"]["scene_allocator_mb"]        = memory.sceneAllocatorMB;
        j["memory"]["heap_allocator_mb"]         = memory.heapAllocatorMB;
        j["memory"]["double_frame_allocator_mb"] = memory.doubleFrameAllocatorMB;
        j["memory"]["stack_allocator_mb"]        = memory.stackAllocatorMB;
        j["memory"]["buddy_allocator_mb"]        = memory.buddyAllocatorMB;
        j["memory"]["paged_allocator_page_kb"]   = memory.pagedAllocatorPageKB;

        j["game"]["time_scale"] = game.timeScale;
        j["game"]["starting_money"] = game.startingMoney;
        j["game"]["starting_day"] = game.startingDay;

        j["debug"]["show_fps"] = debug.showFPS;
        j["debug"]["show_imgui_demo"] = debug.showImGuiDemo;

        std::ofstream file(path);
        file << j.dump(4); // 4文字限で整美出力

        SDL_Log("Config saved to %s", path.c_str());
    }
    catch (const std::exception& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to save config: %s", e.what());
    }
}

} // namespace mk
