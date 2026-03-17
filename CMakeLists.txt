cmake_minimum_required(VERSION 3.25)
project(makai VERSION 0.1.0 LANGUAGES CXX)

# Visual Studio でターゲットをフォルダーに整理
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: UTF-8 without BOM support for Japanese comments
if(MSVC)
    add_compile_options(/utf-8)
endif()

# VC版プレビルドバイナリを使用するため MSVC / 64bit 環境を要求
if(NOT MSVC)
    message(FATAL_ERROR "このプロジェクトは MSVC (Visual Studio) のみサポートしています。")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "このプロジェクトは 64bit 環境のみサポートしています。")
endif()

# SDL3・SDL3_ttf・SDL3_image プレビルドバイナリ（VC版）を自動ダウンロード
include(FetchContent)

# CMake 3.30+ の FetchContent_Populate 非推奨警告をスコープ限定で抑制
if(POLICY CMP0169)
    cmake_policy(PUSH)
    cmake_policy(SET CMP0169 OLD)
endif()

set(SDL3_VERSION       "3.4.2")
set(SDL3_ttf_VERSION   "3.2.2")
set(SDL3_image_VERSION "3.4.0")

FetchContent_Declare(sdl3
    URL      "https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}/SDL3-devel-${SDL3_VERSION}-VC.zip"
    URL_HASH SHA256=fd4c60400634df13a86b69432b22b32f23bccd6a2f4c6a34b42b471c65eb4fd2
)
FetchContent_GetProperties(sdl3)
if(NOT sdl3_POPULATED)
    FetchContent_Populate(sdl3)
endif()
set(SDL3_DIR "${sdl3_SOURCE_DIR}/cmake")
find_package(SDL3 REQUIRED CONFIG)

FetchContent_Declare(sdl3_ttf
    URL      "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL3_ttf_VERSION}/SDL3_ttf-devel-${SDL3_ttf_VERSION}-VC.zip"
    URL_HASH SHA256=67805c5babfc49ca0c56882dc9b8cabbcdd1e6f9edde10ddac91ddb38f3afb8c
)
FetchContent_GetProperties(sdl3_ttf)
if(NOT sdl3_ttf_POPULATED)
    FetchContent_Populate(sdl3_ttf)
endif()
set(SDL3_ttf_DIR "${sdl3_ttf_SOURCE_DIR}/cmake")
find_package(SDL3_ttf REQUIRED CONFIG)

FetchContent_Declare(sdl3_image
    URL      "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL3_image_VERSION}/SDL3_image-devel-${SDL3_image_VERSION}-VC.zip"
    URL_HASH SHA256=6448af4a2f112e6546ad3b56260750c601c02e65a1a937d8ed3e5f5cd9c1f568
)
FetchContent_GetProperties(sdl3_image)
if(NOT sdl3_image_POPULATED)
    FetchContent_Populate(sdl3_image)
endif()
set(SDL3_image_DIR "${sdl3_image_SOURCE_DIR}/cmake")
find_package(SDL3_image REQUIRED CONFIG)

if(POLICY CMP0169)
    cmake_policy(POP)
endif()

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.6
)

# RapidJSON (header-only) — v1.1.0 の不変コミット SHA にピン留め（サプライチェーン攻撃対策）
FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG        f54b0e47a08782a6131cc3d60f94d038fa6e0a51
)

# GLM (OpenGL Mathematics) - header-only
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.2
)

# spdlog (v1.15.3 の不変コミット SHA にピン留め — サプライチェーン攻撃対策)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        6fa36017cfd5731d617e1a934f0e5ea9c4445b13
)

# GLM options: disable library build (header-only mode)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# spdlog options: テスト・ベンチマーク・サンプルを無効化してビルド時間を削減
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH   OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)

# RapidJSON は header-only のため、ソースのダウンロードのみ行い CMake ビルドは実行しない
if(POLICY CMP0169)
    cmake_policy(PUSH)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
    FetchContent_Populate(rapidjson)
endif()
if(POLICY CMP0169)
    cmake_policy(POP)
endif()

FetchContent_MakeAvailable(imgui glm spdlog)

# Create ImGui library with SDL3 backend
add_library(imgui_sdl3 STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(imgui_sdl3 PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_sdl3 PUBLIC SDL3::SDL3)

# 外部ライブラリのターゲットを Visual Studio の "External" フォルダーにまとめる
foreach(_target IN ITEMS imgui_sdl3 spdlog)
    if(TARGET ${_target})
        set_target_properties(${_target} PROPERTIES FOLDER "External")
    endif()
endforeach()

# エンジンライブラリ（SDL3・ImGui などの基盤機能）
add_subdirectory(engine)

# ゲーム実行ファイル（game/ + main.cpp）
file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_SOURCE_DIR}/game/*.cpp"
    "${CMAKE_SOURCE_DIR}/game/*.hpp"
)
list(APPEND GAME_SOURCES "${CMAKE_SOURCE_DIR}/main.cpp")

# Visual Studio フィルター（ディレクトリ構造を反映）
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${GAME_SOURCES})

add_executable(${PROJECT_NAME} ${GAME_SOURCES})

# Visual Studio デバッガーの作業ディレクトリを実行ファイルと同じ場所に設定
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# makai_engine の PUBLIC リンク情報が推移的に伝播するため、追加 include/link は不要
target_link_libraries(${PROJECT_NAME} PRIVATE
    makai_engine
)

# Copy SDL3 and SDL3_ttf, SDL3_image DLLs to executable directory (Windows only)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3_image::SDL3_image>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

# アセットをビルドディレクトリにコピー
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# データをビルドディレクトリにコピー
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/data
)

# config.jsonをビルドディレクトリにコピー
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config.json
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.json
)

# Set as startup project in Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})