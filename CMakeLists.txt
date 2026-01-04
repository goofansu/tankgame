cmake_minimum_required(VERSION 3.16)
project(tankgame C OBJC)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Dev" CACHE STRING "Build type" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Dev" "Release")

# Compiler flags per build type
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG -DPZ_DEBUG")
set(CMAKE_C_FLAGS_DEV "-g -O2 -DNDEBUG -DPZ_DEV")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -DPZ_RELEASE")

# Platform-specific flags
if(APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -DGL_SILENCE_DEPRECATION")
elseif(UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# Emscripten web build
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_WEBGL2=1 -s FULL_ES3=1")
endif()

# Sources
set(CORE_SOURCES
    src/core/pz_mem.c
    src/core/pz_log.c
    src/core/pz_math.c
    src/core/pz_ds.c
    src/core/pz_str.c
    src/core/pz_platform.c
    src/core/pz_debug_cmd.c
    src/core/pz_sim.c
)

set(ENGINE_SOURCES
    src/engine/render/pz_renderer.c
    src/engine/render/pz_render_null.c
    src/engine/render/pz_render_gl33.c
    src/engine/render/pz_render_sokol.c
    src/engine/render/pz_texture.c
    src/engine/pz_audio.c
    src/engine/pz_sfx.c
    src/engine/pz_camera.c
    src/engine/pz_debug_overlay.c
    src/engine/pz_font.c
    src/engine/pz_music.c
    src/engine/pz_sokol.m
    # src/engine/pz_engine.c
    # src/engine/pz_window.c
    # src/engine/pz_input.c
    # src/engine/pz_audio.c
    # src/engine/pz_assets.c
)

set(THIRD_PARTY_SOURCES
    third_party/stb_image_impl.c
    third_party/stb_image_write_impl.c
    third_party/stb_truetype_impl.c
    third_party/tsf_impl.c
)

set(GAME_SOURCES
    src/game/pz_map.c
    src/game/pz_map_render.c
    src/game/pz_mesh.c
    src/game/pz_tile_registry.c
    src/game/pz_tracks.c
    src/game/pz_lighting.c
    src/game/pz_particle.c
    src/game/pz_background.c
    src/game/pz_barrier.c
    src/game/pz_game_music.c
    src/game/pz_game_sfx.c
    src/game/pz_powerup.c
    src/game/pz_projectile.c
    src/game/pz_tank.c
    src/game/pz_ai.c
    src/game/pz_pathfinding.c
    src/game/pz_campaign.c
    src/game/pz_musicset.c
    # src/game/pz_game.c
    # src/game/pz_mine.c
    # src/game/pz_pickup.c
    # src/game/pz_physics.c
    # src/game/modes/pz_mode.c
    # src/game/modes/pz_deathmatch.c
    # src/game/modes/pz_ctf.c
    # src/game/modes/pz_domination.c
)

set(EDITOR_SOURCES
    # src/editor/pz_editor.c
)

set(NET_SOURCES
    # src/net/pz_net.c
    # src/net/pz_protocol.c
    # src/net/pz_client.c
    # src/net/pz_server.c
)

# Main executable
add_executable(tankgame
    src/main.c
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${GAME_SOURCES}
    ${EDITOR_SOURCES}
    ${NET_SOURCES}
    ${THIRD_PARTY_SOURCES}
)

target_include_directories(tankgame PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/sokol
)

# Shader compilation with sokol-shdc
set(SHADER_INPUT ${CMAKE_SOURCE_DIR}/shaders/tankgame.glsl)
set(SHADER_OUTPUT ${CMAKE_SOURCE_DIR}/src/engine/render/pz_sokol_shaders.h)

# Find sokol-shdc binary based on platform
if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(SOKOL_SHDC_BIN ${CMAKE_SOURCE_DIR}/third_party/sokol-tools-bin/bin/osx_arm64/sokol-shdc)
    else()
        set(SOKOL_SHDC_BIN ${CMAKE_SOURCE_DIR}/third_party/sokol-tools-bin/bin/osx/sokol-shdc)
    endif()
elseif(UNIX)
    set(SOKOL_SHDC_BIN ${CMAKE_SOURCE_DIR}/third_party/sokol-tools-bin/bin/linux/sokol-shdc)
elseif(WIN32)
    set(SOKOL_SHDC_BIN ${CMAKE_SOURCE_DIR}/third_party/sokol-tools-bin/bin/win32/sokol-shdc.exe)
endif()

if(EXISTS ${SOKOL_SHDC_BIN})
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${SOKOL_SHDC_BIN}
            --input ${SHADER_INPUT}
            --output ${SHADER_OUTPUT}
            --slang glsl410:metal_macos
            --reflection
        DEPENDS ${SHADER_INPUT}
        COMMENT "Compiling shaders with sokol-shdc"
        VERBATIM
    )
    add_custom_target(shaders DEPENDS ${SHADER_OUTPUT})
    add_dependencies(tankgame shaders)
else()
    message(WARNING "sokol-shdc not found at ${SOKOL_SHDC_BIN}, shaders won't be auto-compiled")
endif()

# Dependencies
target_compile_definitions(tankgame PRIVATE SOKOL_GLCORE PZ_ENABLE_SOKOL)

# macOS framework dependencies
if(APPLE)
    target_link_libraries(tankgame PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework AudioToolbox"
    )
elseif(UNIX)
    target_link_libraries(tankgame PRIVATE asound)
endif()

# Testing (M0.4)
enable_testing()
add_subdirectory(tests)

# Print build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
