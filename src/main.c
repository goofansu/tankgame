/*
 * Tank Game - Main Entry Point
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#include "core/pz_log.h"
#include "core/pz_math.h"
#include "core/pz_mem.h"
#include "core/pz_platform.h"
#include "engine/render/pz_renderer.h"

#define WINDOW_TITLE "Tank Game"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

int
main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

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

    // Load test shader
    pz_shader_handle shader = pz_renderer_load_shader(
        renderer, "shaders/test.vert", "shaders/test.frag", "test");
    if (shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to load test shader");
    }

    // Create triangle vertex data
    // clang-format off
    float vertices[] = {
        // Position          // Color
        -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // bottom left - red
         0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // bottom right - green
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // top - blue
    };
    // clang-format on

    // Create vertex buffer
    pz_buffer_desc vb_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STATIC,
        .data = vertices,
        .size = sizeof(vertices),
    };
    pz_buffer_handle vertex_buffer
        = pz_renderer_create_buffer(renderer, &vb_desc);

    // Create pipeline
    pz_vertex_attr attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
        { .name = "a_color",
            .type = PZ_ATTR_FLOAT3,
            .offset = 3 * sizeof(float) },
    };

    pz_pipeline_desc pipeline_desc = {
        .shader = shader,
        .vertex_layout = {
            .attrs = attrs,
            .attr_count = 2,
            .stride = 6 * sizeof(float),
        },
        .blend = PZ_BLEND_NONE,
        .depth = PZ_DEPTH_NONE,
        .cull = PZ_CULL_NONE,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    pz_pipeline_handle pipeline
        = pz_renderer_create_pipeline(renderer, &pipeline_desc);

    // Main loop
    bool running = true;
    SDL_Event event;

    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    int w = event.window.data1;
                    int h = event.window.data2;
                    pz_renderer_set_viewport(renderer, w, h);
                    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
                        "Window resized: %dx%d", w, h);
                }
                break;
            }
        }

        // Begin frame
        pz_renderer_begin_frame(renderer);

        // Clear to cornflower blue
        pz_renderer_clear(renderer, 0.392f, 0.584f, 0.929f, 1.0f, 1.0f);

        // Set MVP (identity for now - triangle in clip space)
        pz_mat4 mvp = pz_mat4_identity();
        pz_renderer_set_uniform_mat4(renderer, shader, "u_mvp", &mvp);

        // Draw triangle
        pz_draw_cmd draw_cmd = {
            .pipeline = pipeline,
            .vertex_buffer = vertex_buffer,
            .vertex_count = 3,
        };
        pz_renderer_draw(renderer, &draw_cmd);

        // End frame
        pz_renderer_end_frame(renderer);

        // Swap buffers
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    pz_renderer_destroy_pipeline(renderer, pipeline);
    pz_renderer_destroy_buffer(renderer, vertex_buffer);
    pz_renderer_destroy_shader(renderer, shader);
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
