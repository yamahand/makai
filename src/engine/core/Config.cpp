#include "Config.hpp"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/error/en.h>
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

        rapidjson::IStreamWrapper isw(file);
        rapidjson::Document doc;
        doc.ParseStream(isw);

        if (doc.HasParseError()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "JSON parse error in '%s' at offset %zu: %s",
                        path.c_str(),
                        doc.GetErrorOffset(),
                        rapidjson::GetParseError_En(doc.GetParseError()));
            return config;
        }

        // ウィンドウ設定を読込
        if (doc.HasMember("window") && doc["window"].IsObject()) {
            const auto& w = doc["window"];
            if (w.HasMember("title") && w["title"].IsString())       config.window.title      = w["title"].GetString();
            if (w.HasMember("width") && w["width"].IsInt())          config.window.width      = w["width"].GetInt();
            if (w.HasMember("height") && w["height"].IsInt())        config.window.height     = w["height"].GetInt();
            if (w.HasMember("fullscreen") && w["fullscreen"].IsBool()) config.window.fullscreen = w["fullscreen"].GetBool();
            if (w.HasMember("vsync") && w["vsync"].IsBool())         config.window.vsync      = w["vsync"].GetBool();
        }

        // フォント設定を読込
        if (doc.HasMember("fonts") && doc["fonts"].IsObject()) {
            const auto& fonts = doc["fonts"];
            if (fonts.HasMember("default") && fonts["default"].IsObject()) {
                const auto& def = fonts["default"];
                if (def.HasMember("path") && def["path"].IsString()) config.defaultFont.path = def["path"].GetString();
                if (def.HasMember("size") && def["size"].IsInt())    config.defaultFont.size = def["size"].GetInt();
            }
            if (fonts.HasMember("large") && fonts["large"].IsObject()) {
                const auto& lg = fonts["large"];
                if (lg.HasMember("path") && lg["path"].IsString()) config.largeFont.path = lg["path"].GetString();
                if (lg.HasMember("size") && lg["size"].IsInt())    config.largeFont.size = lg["size"].GetInt();
            }
        }

        // メモリ設定を解析
        if (doc.HasMember("memory") && doc["memory"].IsObject()) {
            const auto& mem = doc["memory"];
            if (mem.HasMember("frame_allocator_mb") && mem["frame_allocator_mb"].IsInt())
                config.memory.frameAllocatorMB = mem["frame_allocator_mb"].GetInt();
            if (mem.HasMember("scene_allocator_mb") && mem["scene_allocator_mb"].IsInt())
                config.memory.sceneAllocatorMB = mem["scene_allocator_mb"].GetInt();
            if (mem.HasMember("heap_allocator_mb") && mem["heap_allocator_mb"].IsInt())
                config.memory.heapAllocatorMB = mem["heap_allocator_mb"].GetInt();
            if (mem.HasMember("double_frame_allocator_mb") && mem["double_frame_allocator_mb"].IsInt())
                config.memory.doubleFrameAllocatorMB = mem["double_frame_allocator_mb"].GetInt();
            if (mem.HasMember("stack_allocator_mb") && mem["stack_allocator_mb"].IsInt())
                config.memory.stackAllocatorMB = mem["stack_allocator_mb"].GetInt();
            if (mem.HasMember("buddy_allocator_mb") && mem["buddy_allocator_mb"].IsInt())
                config.memory.buddyAllocatorMB = mem["buddy_allocator_mb"].GetInt();
            if (mem.HasMember("paged_allocator_page_kb") && mem["paged_allocator_page_kb"].IsInt())
                config.memory.pagedAllocatorPageKB = mem["paged_allocator_page_kb"].GetInt();
        }

        // ゲーム設定を読込
        if (doc.HasMember("game") && doc["game"].IsObject()) {
            const auto& game = doc["game"];
            if (game.HasMember("time_scale") && game["time_scale"].IsNumber())
                config.game.timeScale = game["time_scale"].GetFloat();
            if (game.HasMember("starting_money") && game["starting_money"].IsInt())
                config.game.startingMoney = game["starting_money"].GetInt();
            if (game.HasMember("starting_day") && game["starting_day"].IsInt())
                config.game.startingDay = game["starting_day"].GetInt();
        }

        // デバッグ設定を読込
        if (doc.HasMember("debug") && doc["debug"].IsObject()) {
            const auto& dbg = doc["debug"];
            if (dbg.HasMember("show_fps") && dbg["show_fps"].IsBool())
                config.debug.showFPS = dbg["show_fps"].GetBool();
            if (dbg.HasMember("show_imgui_demo") && dbg["show_imgui_demo"].IsBool())
                config.debug.showImGuiDemo = dbg["show_imgui_demo"].GetBool();
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
        rapidjson::Document doc;
        doc.SetObject();
        auto& alloc = doc.GetAllocator();

        // ウィンドウ設定
        rapidjson::Value windowObj(rapidjson::kObjectType);
        windowObj.AddMember("title", rapidjson::Value(window.title.c_str(), alloc), alloc);
        windowObj.AddMember("width", window.width, alloc);
        windowObj.AddMember("height", window.height, alloc);
        windowObj.AddMember("fullscreen", window.fullscreen, alloc);
        windowObj.AddMember("vsync", window.vsync, alloc);
        doc.AddMember("window", windowObj, alloc);

        // フォント設定
        rapidjson::Value fontsObj(rapidjson::kObjectType);
        {
            rapidjson::Value defObj(rapidjson::kObjectType);
            defObj.AddMember("path", rapidjson::Value(defaultFont.path.c_str(), alloc), alloc);
            defObj.AddMember("size", defaultFont.size, alloc);
            fontsObj.AddMember("default", defObj, alloc);
        }
        {
            rapidjson::Value lgObj(rapidjson::kObjectType);
            lgObj.AddMember("path", rapidjson::Value(largeFont.path.c_str(), alloc), alloc);
            lgObj.AddMember("size", largeFont.size, alloc);
            fontsObj.AddMember("large", lgObj, alloc);
        }
        doc.AddMember("fonts", fontsObj, alloc);

        // メモリ設定
        rapidjson::Value memObj(rapidjson::kObjectType);
        memObj.AddMember("frame_allocator_mb", memory.frameAllocatorMB, alloc);
        memObj.AddMember("scene_allocator_mb", memory.sceneAllocatorMB, alloc);
        memObj.AddMember("heap_allocator_mb", memory.heapAllocatorMB, alloc);
        memObj.AddMember("double_frame_allocator_mb", memory.doubleFrameAllocatorMB, alloc);
        memObj.AddMember("stack_allocator_mb", memory.stackAllocatorMB, alloc);
        memObj.AddMember("buddy_allocator_mb", memory.buddyAllocatorMB, alloc);
        memObj.AddMember("paged_allocator_page_kb", memory.pagedAllocatorPageKB, alloc);
        doc.AddMember("memory", memObj, alloc);

        // ゲーム設定
        rapidjson::Value gameObj(rapidjson::kObjectType);
        gameObj.AddMember("time_scale", game.timeScale, alloc);
        gameObj.AddMember("starting_money", game.startingMoney, alloc);
        gameObj.AddMember("starting_day", game.startingDay, alloc);
        doc.AddMember("game", gameObj, alloc);

        // デバッグ設定
        rapidjson::Value debugObj(rapidjson::kObjectType);
        debugObj.AddMember("show_fps", debug.showFPS, alloc);
        debugObj.AddMember("show_imgui_demo", debug.showImGuiDemo, alloc);
        doc.AddMember("debug", debugObj, alloc);

        // 整形出力（4スペースインデント）
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        writer.SetIndent(' ', 4);
        doc.Accept(writer);

        std::ofstream file(path);
        file << buffer.GetString();

        SDL_Log("Config saved to %s", path.c_str());
    }
    catch (const std::exception& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to save config: %s", e.what());
    }
}

} // namespace mk
