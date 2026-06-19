cmake_minimum_required(VERSION 3.16)
project(TsukuruEngine VERSION 1.0.0 LANGUAGES CXX)

# ----- Build options -----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Optimization flags (C++ 최적화)
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/O2> /W3)
else()
    add_compile_options($<$<CONFIG:Release>:-O2> -Wall -Wextra -Wno-unused-parameter)
endif()

# Link-time optimization for Release
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg)
if(ipo_ok AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

include(FetchContent)

# ----- nlohmann/json (header-only) -----
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# =====================================================================
#  tsukuru_core : pure logic + serialization (NO raylib) -> testable
# =====================================================================
file(GLOB_RECURSE CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/project/*.cpp
    ${CMAKE_SOURCE_DIR}/src/world/*.cpp
    ${CMAKE_SOURCE_DIR}/src/database/*.cpp
    ${CMAKE_SOURCE_DIR}/src/entity/Event.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Inventory.cpp
    ${CMAKE_SOURCE_DIR}/src/game/GameState.cpp
    ${CMAKE_SOURCE_DIR}/src/battle/Battle.cpp
)
add_library(tsukuru_core STATIC ${CORE_SOURCES})
target_include_directories(tsukuru_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tsukuru_core PUBLIC nlohmann_json::nlohmann_json)

# =====================================================================
#  Self-test executable (headless, core only)
# =====================================================================
add_executable(tsukuru_selftest ${CMAKE_SOURCE_DIR}/src/selftest.cpp)
target_link_libraries(tsukuru_selftest PRIVATE tsukuru_core)

# =====================================================================
#  raylib (graphics) — only needed for the full engine executable
# =====================================================================
option(BUILD_ENGINE "Build the full graphical engine (requires OpenGL)" ON)

if(BUILD_ENGINE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
    set(BUILD_GAMES OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(raylib)

    file(GLOB_RECURSE ENGINE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/main.cpp
        ${CMAKE_SOURCE_DIR}/src/core/*.cpp
        ${CMAKE_SOURCE_DIR}/src/editor/*.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlay.cpp
        ${CMAKE_SOURCE_DIR}/src/game/TitleScreen.cpp
        ${CMAKE_SOURCE_DIR}/src/game/Menu.cpp
        ${CMAKE_SOURCE_DIR}/src/render/*.cpp
    )
    add_executable(TsukuruEngine ${ENGINE_SOURCES})
    target_link_libraries(TsukuruEngine PRIVATE tsukuru_core raylib)
    target_include_directories(TsukuruEngine PRIVATE ${CMAKE_SOURCE_DIR}/src)

    if(APPLE)
        target_link_libraries(TsukuruEngine PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
    endif()

    # On MinGW (incl. cross builds) make it a GUI app so no console window pops up.
    if(MINGW)
        target_link_options(TsukuruEngine PRIVATE -mwindows)
    endif()

    # Headless asset generator (CPU image API only) — host tool, skip when cross-compiling.
    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(gen_assets ${CMAKE_SOURCE_DIR}/tools/gen_assets.cpp)
        target_link_libraries(gen_assets PRIVATE raylib)
    endif()

    # Copy assets/projects next to the binary for "double-click and run"
    add_custom_command(TARGET TsukuruEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:TsukuruEngine>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/projects $<TARGET_FILE_DIR:TsukuruEngine>/projects
    )
endif()
