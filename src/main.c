/*
 * Tank Game - Main Entry Point
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "core/pz_debug_cmd.h"
#include "core/pz_log.h"
#include "core/pz_math.h"
#include "core/pz_mem.h"
#include "core/pz_platform.h"
#include "core/pz_str.h"
#include "engine/pz_camera.h"
#include "engine/pz_debug_overlay.h"
#include "engine/render/pz_renderer.h"
#include "engine/render/pz_texture.h"
#include "game/pz_map.h"
#include "game/pz_map_render.h"

#define WINDOW_TITLE "Tank Game"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

// Generate a timestamped screenshot filename
static char *
generate_screenshot_path(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char filename[128];
    snprintf(filename, sizeof(filename),
        "screenshots/screenshot_%04d%02d%02d_%02d%02d%02d.png",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min,
        t->tm_sec);

    return pz_str_dup(filename);
}

int
main(int argc, char *argv[])
{
    bool auto_screenshot = false;
    const char *screenshot_path = NULL;
    int screenshot_frames = 1; // Number of frames to wait before screenshot

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            auto_screenshot = true;
            screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--screenshot-frames") == 0
            && i + 1 < argc) {
            screenshot_frames = atoi(argv[++i]);
        }
    }

    printf("Tank Game - Starting...\n");

#ifdef PZ_DEBUG
    printf("Build: Debug\n");
#elif defined(PZ_DEV)
    printf("Build: Dev\n");
#elif defined(PZ_RELEASE)
    printf("Build: Release\n");
#endif

    // Initialize subsystems
    pz_log_init();
    pz_time_init();

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_CORE, "SDL_Init failed: %s",
            SDL_GetError());
        return EXIT_FAILURE;
    }
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE, "SDL initialized");

    // Request OpenGL 3.3 Core Profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window with OpenGL context
    SDL_Window *window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!window) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_CORE, "SDL_CreateWindow failed: %s",
            SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE, "Window created: %dx%d", WINDOW_WIDTH,
        WINDOW_HEIGHT);

    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_CORE, "SDL_GL_CreateContext failed: %s",
            SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Enable VSync
    SDL_GL_SetSwapInterval(1);

    // Create renderer
    pz_renderer_config renderer_config = {
        .backend = PZ_BACKEND_GL33,
        .window_handle = window,
        .viewport_width = WINDOW_WIDTH,
        .viewport_height = WINDOW_HEIGHT,
    };

    pz_renderer *renderer = pz_renderer_create(&renderer_config);
    if (!renderer) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_CORE, "Failed to create renderer");
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Create texture manager
    pz_texture_manager *tex_manager = pz_texture_manager_create(renderer);

    // Initialize camera
    pz_camera camera;
    pz_camera_init(&camera, WINDOW_WIDTH, WINDOW_HEIGHT);
    pz_camera_setup_game_view(
        &camera, (pz_vec3) { 0, 0, 0 }, 35.0f, 20.0f); // Height 35, 20° pitch

    // Create the test map
    pz_map *game_map = pz_map_create_test();
    if (!game_map) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to create test map");
    }

    // Create map renderer
    pz_map_renderer *map_renderer
        = pz_map_renderer_create(renderer, tex_manager);
    if (map_renderer && game_map) {
        pz_map_renderer_set_map(map_renderer, game_map);
    }

    // Initialize debug command interface
    pz_debug_cmd_init(NULL);

    // Create debug overlay
    pz_debug_overlay *debug_overlay = pz_debug_overlay_create(renderer);
    if (!debug_overlay) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE, "Failed to create debug overlay");
    }
    // Start with overlay hidden (F2 to toggle)

    // Note: Map renderer handles its own shaders and pipelines

    // ========================================================================
    // Main loop
    // ========================================================================

    bool running = true;
    SDL_Event event;
    int frame_count = 0;
    uint64_t last_hot_reload_check = pz_time_now();

    // Camera movement
    float cam_speed = 0.5f;

    while (running) {
        // Poll debug commands
        if (!pz_debug_cmd_poll(renderer)) {
            running = false;
        }

        // Handle events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_F2) {
                    pz_debug_overlay_toggle(debug_overlay);
                } else if (event.key.keysym.sym == SDLK_F12) {
                    char *path = generate_screenshot_path();
                    if (path) {
                        pz_renderer_save_screenshot(renderer, path);
                        pz_free(path);
                    }
                }
                break;
            case SDL_MOUSEWHEEL: {
                // Zoom with mouse wheel
                float zoom_delta = -event.wheel.y * 2.0f;
                pz_camera_zoom(&camera, zoom_delta);
            } break;
            case SDL_MOUSEMOTION: {
                // Get world position under mouse
                int mx = event.motion.x;
                int my = event.motion.y;
                pz_vec3 world_pos = pz_camera_screen_to_world(&camera, mx, my);
                // Just log it for now (could display in debug overlay later)
                (void)world_pos;
            } break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    int w = event.window.data1;
                    int h = event.window.data2;
                    pz_renderer_set_viewport(renderer, w, h);
                    pz_camera_set_viewport(&camera, w, h);
                    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
                        "Window resized: %dx%d", w, h);
                }
                break;
            }
        }

        // Keyboard camera movement
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        pz_vec3 cam_move = { 0, 0, 0 };
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
            cam_move.z += cam_speed;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
            cam_move.z -= cam_speed;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
            cam_move.x -= cam_speed;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
            cam_move.x += cam_speed;

        if (cam_move.x != 0 || cam_move.z != 0) {
            pz_camera_translate(&camera, cam_move);
        }

        // Check for hot-reload every 500ms
        uint64_t now = pz_time_now();
        if (now - last_hot_reload_check > 500000) { // 500ms in microseconds
            pz_texture_check_hot_reload(tex_manager);
            last_hot_reload_check = now;
        }

        // Begin frame
        pz_debug_overlay_begin_frame(debug_overlay);
        pz_renderer_begin_frame(renderer);

        // Clear to dark gray (sky color for now)
        pz_renderer_clear(renderer, 0.2f, 0.2f, 0.25f, 1.0f, 1.0f);

        // ====================================================================
        // Draw map
        // ====================================================================
        {
            const pz_mat4 *vp = pz_camera_get_view_projection(&camera);
            pz_map_renderer_draw(map_renderer, vp);
        }

        // Render debug overlay (before end frame)
        pz_debug_overlay_render(debug_overlay);

        // End frame
        pz_debug_overlay_end_frame(debug_overlay);
        pz_renderer_end_frame(renderer);

        // Auto-screenshot mode (before swap to capture back buffer)
        frame_count++;
        if (auto_screenshot && frame_count >= screenshot_frames) {
            pz_renderer_save_screenshot(renderer, screenshot_path);
            running = false;
        }

        // Swap buffers
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    pz_debug_overlay_destroy(debug_overlay);
    pz_debug_cmd_shutdown();

    pz_map_renderer_destroy(map_renderer);
    pz_map_destroy(game_map);

    pz_texture_manager_destroy(tex_manager);
    pz_renderer_destroy(renderer);

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    pz_log_shutdown();

    // Check for memory leaks
    pz_mem_dump_leaks();

    printf("Tank Game - Exiting.\n");
    return EXIT_SUCCESS;
}
