cmake_minimum_required(VERSION 3.16)
project(tankgame C)

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
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2 -s USE_WEBGL2=1 -s FULL_ES3=1")
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
)

set(ENGINE_SOURCES
    src/engine/render/pz_renderer.c
    src/engine/render/pz_render_null.c
    src/engine/render/pz_render_gl33.c
    # src/engine/pz_engine.c
    # src/engine/pz_window.c
    # src/engine/pz_input.c
    # src/engine/pz_audio.c
    # src/engine/pz_assets.c
)

set(THIRD_PARTY_SOURCES
    third_party/stb_image_write_impl.c
)

set(GAME_SOURCES
    # src/game/pz_game.c
    # src/game/pz_tank.c
    # src/game/pz_projectile.c
    # src/game/pz_mine.c
    # src/game/pz_pickup.c
    # src/game/pz_map.c
    # src/game/pz_physics.c
    # src/game/pz_camera.c
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
)

# Dependencies
find_package(SDL2 REQUIRED)
target_link_libraries(tankgame PRIVATE SDL2::SDL2)

# macOS framework dependencies
if(APPLE)
    target_link_libraries(tankgame PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()

# Testing (M0.4)
enable_testing()
add_subdirectory(tests)

# Print build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
