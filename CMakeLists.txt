cmake_minimum_required(VERSION 3.25)
project(makai VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: UTF-8 without BOM support for Japanese comments
if(MSVC)
    add_compile_options(/utf-8)
endif()

# SDL3 と SDL3_ttf を自動取得
include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
)
FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        main
)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.6
)

# nlohmann/json (header-only)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

# GLM (OpenGL Mathematics) - header-only
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.2
)

# GLM options: disable library build (header-only mode)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# SDL3_image: Use prebuilt binary (FetchContent had git submodule issues)
set(SDL3_image_DIR "${CMAKE_SOURCE_DIR}/external/SDL3_image-3.4.0/cmake")
find_package(SDL3_image REQUIRED CONFIG)

FetchContent_MakeAvailable(SDL3 SDL3_ttf imgui json glm)

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

# ソースファイル収集
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Set as startup project in Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/SDL3_image-3.4.0/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    ${CMAKE_SOURCE_DIR}/external/SDL3_image-3.4.0/lib/x64/SDL3_image.lib
    imgui_sdl3
    nlohmann_json::nlohmann_json
    glm::glm
)

# Copy SDL3 and SDL3_ttf DLLs to executable directory (Windows only)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/external/SDL3_image-3.4.0/lib/x64/SDL3_image.dll"
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
