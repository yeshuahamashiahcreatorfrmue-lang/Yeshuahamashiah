cmake_minimum_required(VERSION 3.16)
project(TsukuruEngine VERSION 1.0.0 LANGUAGES CXX)

# ----- Build options -----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Statically link the C/C++ runtime on MSVC (/MT) so the .exe runs on a stock
# Windows PC with NOTHING installed — no Visual C++ Redistributable required.
# Applies to every target below (raylib, core, the engine) for a consistent CRT.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Optimization flags (C++ 최적화)
# /utf-8 (MSVC) and -fexec-charset (GCC/Clang default) ensure the Korean
# UTF-8 string literals in the source are stored as UTF-8 at runtime, which is
# what the font renderer (DrawTextU) expects.
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/O2> /W3 /utf-8)
else()
    add_compile_options($<$<CONFIG:Release>:-O3> -Wall -Wextra -Wno-unused-parameter
                        -finput-charset=UTF-8 -fexec-charset=UTF-8)
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
    ${CMAKE_SOURCE_DIR}/src/net/Net.cpp
)
add_library(tsukuru_core STATIC ${CORE_SOURCES})
target_include_directories(tsukuru_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tsukuru_core PUBLIC nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(tsukuru_core PUBLIC ws2_32)   # TCP sockets (Net) on Windows
endif()

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

    # raylib's default config disables JPG/BMP/TGA/PSD/HDR decoding, so those
    # images registered but never rendered ("이미지가 안 올라간다"). Enable every
    # common image format we advertise in the importer (PNG/GIF/QOI are already on).
    target_compile_definitions(raylib PRIVATE
        SUPPORT_FILEFORMAT_JPG=1
        SUPPORT_FILEFORMAT_BMP=1
        SUPPORT_FILEFORMAT_TGA=1
        SUPPORT_FILEFORMAT_PSD=1
        SUPPORT_FILEFORMAT_HDR=1
        SUPPORT_FILEFORMAT_PIC=1
        SUPPORT_FILEFORMAT_PNM=1)

    file(GLOB_RECURSE ENGINE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/main.cpp
        ${CMAKE_SOURCE_DIR}/src/core/*.cpp
        ${CMAKE_SOURCE_DIR}/src/editor/*.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlay.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlayMovement.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlayEvents.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlayMonsters.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlaySkills.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlayFx.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlayNpc.cpp
        ${CMAKE_SOURCE_DIR}/src/game/GamePlayRender.cpp
        ${CMAKE_SOURCE_DIR}/src/game/TitleScreen.cpp
        ${CMAKE_SOURCE_DIR}/src/game/Menu.cpp
        ${CMAKE_SOURCE_DIR}/src/render/*.cpp
    )
    add_executable(TsukuruEngine ${ENGINE_SOURCES})
    target_link_libraries(TsukuruEngine PRIVATE tsukuru_core raylib)
    target_include_directories(TsukuruEngine PRIVATE ${CMAKE_SOURCE_DIR}/src)

    # ----- AI subject segmentation (optional, phone-grade "lift subject") -----
    # If ONNX Runtime is available, compile the neural U^2-Net cut-out path
    # (src/render/Segmenter.cpp) for real; otherwise it builds as a no-op stub and
    # the engine falls back to the classic colour-based background removal. Point
    # ONNXRUNTIME_ROOT at a folder containing include/ and lib/.
    set(ONNXRUNTIME_ROOT "" CACHE PATH "ONNX Runtime root (with include/ and lib/) — enables AI cut-out")
    if(ONNXRUNTIME_ROOT AND EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
        message(STATUS "AI cut-out: ONNX Runtime found at ${ONNXRUNTIME_ROOT}")
        target_compile_definitions(TsukuruEngine PRIVATE TSUKURU_ONNX=1)
        target_include_directories(TsukuruEngine PRIVATE "${ONNXRUNTIME_ROOT}/include")
        if(WIN32)
            target_link_libraries(TsukuruEngine PRIVATE "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib")
            # Delay-load onnxruntime.dll: the engine starts even if the AI runtime
            # (or its deps) is absent; Segmenter::load() probes it first and falls
            # back to colour cut-out. So a bare PC never fails to launch over AI.
            if(MSVC)
                target_link_options(TsukuruEngine PRIVATE /DELAYLOAD:onnxruntime.dll)
                target_link_libraries(TsukuruEngine PRIVATE delayimp)
            endif()
            # Ship onnxruntime.dll next to the exe so the AI model loads at runtime.
            file(GLOB _ort_dlls "${ONNXRUNTIME_ROOT}/lib/*.dll")
            foreach(_dll ${_ort_dlls})
                add_custom_command(TARGET TsukuruEngine POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" $<TARGET_FILE_DIR:TsukuruEngine>)
            endforeach()
        else()
            target_link_libraries(TsukuruEngine PRIVATE "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so")
        endif()
    else()
        message(STATUS "AI cut-out: ONNX Runtime not set (classic background removal only)")
    endif()

    # Embed Windows version-info metadata into the .exe (reduces "unknown
    # publisher / info-less binary" AV & SmartScreen false positives), and link
    # comdlg32 for the native Open-File dialog (Windows Explorer picker).
    if(WIN32)
        enable_language(RC)
        target_sources(TsukuruEngine PRIVATE ${CMAKE_SOURCE_DIR}/src/win/TsukuruEngine.rc)
        target_link_libraries(TsukuruEngine PRIVATE comdlg32)
    endif()

    if(APPLE)
        target_link_libraries(TsukuruEngine PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
    endif()

    # Make it a GUI app (no console window) on both MinGW and MSVC.
    if(MINGW)
        target_link_options(TsukuruEngine PRIVATE -mwindows)
    elseif(MSVC)
        target_link_options(TsukuruEngine PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
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

# World generator (data only; links core, no graphics) — host tool.
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(gen_world ${CMAKE_SOURCE_DIR}/tools/gen_world.cpp)
    target_link_libraries(gen_world PRIVATE tsukuru_core)

    # Polished single-map game generator (art via raylib + data via core).
    add_executable(gen_game ${CMAKE_SOURCE_DIR}/tools/gen_game.cpp)
    target_link_libraries(gen_game PRIVATE tsukuru_core raylib)
    target_include_directories(gen_game PRIVATE ${CMAKE_SOURCE_DIR}/src)

    # Concept map pack: a 112-tile themed tileset + 14 themed maps.
    add_executable(gen_concept_maps ${CMAKE_SOURCE_DIR}/tools/gen_concept_maps.cpp)
    target_link_libraries(gen_concept_maps PRIVATE tsukuru_core raylib)
    target_include_directories(gen_concept_maps PRIVATE ${CMAKE_SOURCE_DIR}/src)
endif()
