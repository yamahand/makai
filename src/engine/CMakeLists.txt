# エンジンライブラリ
# SDL3・ImGui などの基盤機能をまとめた静的ライブラリ

file(GLOB_RECURSE ENGINE_SOURCES "*.cpp" "*.hpp")

# Visual Studio フィルター（ディレクトリ構造を反映）
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_SOURCES})

add_library(makai_engine STATIC ${ENGINE_SOURCES})

target_include_directories(makai_engine PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/SDL3_image-3.4.0/include
)

target_link_libraries(makai_engine PUBLIC
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    ${CMAKE_SOURCE_DIR}/external/SDL3_image-3.4.0/lib/x64/SDL3_image.lib
    imgui_sdl3
    nlohmann_json::nlohmann_json
    glm::glm
)

# MSVC: UTF-8 without BOM support for Japanese comments
if(MSVC)
    target_compile_options(makai_engine PRIVATE /utf-8)
endif()
