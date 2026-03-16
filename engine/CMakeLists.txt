# エンジンライブラリ
# SDL3・ImGui などの基盤機能をまとめた静的ライブラリ

file(GLOB_RECURSE ENGINE_SOURCES "*.cpp" "*.hpp")

# Visual Studio フィルター（ディレクトリ構造を反映）
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_SOURCES})

add_library(makai_engine STATIC ${ENGINE_SOURCES})

target_include_directories(makai_engine PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(makai_engine PUBLIC
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    SDL3_image::SDL3_image
    imgui_sdl3
    glm::glm
)

# RapidJSON は header-only かつエンジン内部実装のみで使用するため PRIVATE に追加
target_include_directories(makai_engine PRIVATE
    ${rapidjson_SOURCE_DIR}/include
)

# spdlog はエンジン内部の実装詳細のため PRIVATE リンク（game 側に漏れない）
target_link_libraries(makai_engine PRIVATE
    spdlog::spdlog
)

# MSVC: UTF-8 without BOM support for Japanese comments
if(MSVC)
    target_compile_options(makai_engine PRIVATE /utf-8)
endif()
