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
#include "game/pz_mesh.h"
#include "game/pz_projectile.h"
#include "game/pz_tracks.h"

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

    // Enable MSAA (4x multisampling for anti-aliasing)
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

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

    // Initialize camera (will be set up after map loads)
    pz_camera camera;
    pz_camera_init(&camera, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Try to load map from file, fall back to test map
    const char *map_path = "assets/maps/test_arena.map";
    pz_map *game_map = pz_map_load(map_path);
    if (!game_map) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Failed to load map from %s, creating test map", map_path);
        game_map = pz_map_create_test();
        // Save the test map so we have a file to edit
        if (game_map) {
            pz_map_save(game_map, map_path);
        }
    }
    if (!game_map) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to create map");
    }

    // Set up camera to fit the map
    if (game_map) {
        pz_camera_fit_map(
            &camera, game_map->world_width, game_map->world_height, 20.0f);
    }

    // Create map renderer
    pz_map_renderer *map_renderer
        = pz_map_renderer_create(renderer, tex_manager);
    if (map_renderer && game_map) {
        pz_map_renderer_set_map(map_renderer, game_map);
    }

    // Set up map hot-reload
    pz_map_hot_reload *map_hot_reload = NULL;
    if (map_renderer) {
        map_hot_reload
            = pz_map_hot_reload_create(map_path, &game_map, map_renderer);
    }

    // Create track accumulation system
    pz_tracks *tracks = NULL;
    if (game_map) {
        pz_tracks_config track_config = {
            .world_width = game_map->world_width,
            .world_height = game_map->world_height,
            .texture_size = 1024, // 1024x1024 track texture
        };
        tracks = pz_tracks_create(renderer, tex_manager, &track_config);
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
    // Tank mesh test (M4.1)
    // ========================================================================
    pz_mesh *tank_body = pz_mesh_create_tank_body();
    pz_mesh *tank_turret = pz_mesh_create_tank_turret();

    pz_mesh_upload(tank_body, renderer);
    pz_mesh_upload(tank_turret, renderer);

    // Create shader and pipeline for entity rendering
    pz_shader_handle entity_shader = pz_renderer_load_shader(
        renderer, "shaders/entity.vert", "shaders/entity.frag", "entity");

    pz_pipeline_handle entity_pipeline = PZ_INVALID_HANDLE;
    if (entity_shader != PZ_INVALID_HANDLE) {
        pz_pipeline_desc pipeline_desc = {
            .shader = entity_shader,
            .vertex_layout = pz_mesh_get_vertex_layout(),
            .blend = PZ_BLEND_NONE,
            .depth = PZ_DEPTH_READ_WRITE,
            .cull = PZ_CULL_BACK,
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        entity_pipeline = pz_renderer_create_pipeline(renderer, &pipeline_desc);
    }

    // Tank state
    pz_vec2 tank_pos = { 12.0f, 7.0f };
    pz_vec2 tank_vel = { 0.0f, 0.0f };
    float tank_angle = 0.0f; // Body rotation (radians)
    float turret_angle = 0.0f; // Turret rotation (world space, radians)

    // Tank movement parameters
    const float tank_accel = 40.0f; // Acceleration (must be > friction)
    const float tank_friction
        = 25.0f; // Friction/damping (heavy tank stops fast)
    const float tank_max_speed = 5.0f; // Max speed
    const float tank_turn_speed = 5.0f; // Body turn rate (rad/s)
    const float turret_turn_speed = 8.0f; // Turret turn rate (rad/s)

    // Tank collision parameters
    const float tank_radius = 0.7f; // Collision radius (half of body width)

    // ========================================================================
    // Projectile system (M6.1, M6.2, M6.3)
    // ========================================================================
    pz_projectile_manager *projectile_mgr
        = pz_projectile_manager_create(renderer);

    // Fire cooldown tracking
    float fire_cooldown = 0.0f;
    const float fire_cooldown_time
        = 0.25f; // 0.25 seconds between shots (2x faster)

    // Turret barrel offset (how far from turret center the projectile spawns)
    // Must match turret mesh: hd (0.45) + barrel_length (1.2) = 1.65
    const float barrel_length = 1.65f;

    // ========================================================================
    // Laser sight system
    // ========================================================================
    pz_shader_handle laser_shader = pz_renderer_load_shader(
        renderer, "shaders/laser.vert", "shaders/laser.frag", "laser");

    pz_pipeline_handle laser_pipeline = PZ_INVALID_HANDLE;
    pz_buffer_handle laser_vb = PZ_INVALID_HANDLE;

    // Laser sight vertex layout (position + texcoord)
    typedef struct {
        float x, y, z;
        float u, v;
    } laser_vertex;

    if (laser_shader != PZ_INVALID_HANDLE) {
        pz_vertex_attr laser_attrs[] = {
            { .name = "a_position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
            { .name = "a_texcoord",
                .type = PZ_ATTR_FLOAT2,
                .offset = 3 * sizeof(float) },
        };

        pz_pipeline_desc laser_desc = {
            .shader = laser_shader,
            .vertex_layout = { .attrs = laser_attrs,
                .attr_count = 2,
                .stride = sizeof(laser_vertex) },
            .blend = PZ_BLEND_ALPHA,
            .depth = PZ_DEPTH_READ, // Read depth but don't write
            .cull = PZ_CULL_NONE,
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        laser_pipeline = pz_renderer_create_pipeline(renderer, &laser_desc);

        // Create dynamic buffer for laser quad (6 vertices)
        pz_buffer_desc laser_vb_desc = {
            .type = PZ_BUFFER_VERTEX,
            .usage = PZ_BUFFER_DYNAMIC,
            .data = NULL,
            .size = 6 * sizeof(laser_vertex),
        };
        laser_vb = pz_renderer_create_buffer(renderer, &laser_vb_desc);
    }

    const float laser_width = 0.08f; // Width of the laser beam
    const float laser_max_dist = 50.0f; // Maximum laser range

    // ========================================================================
    // Main loop
    // ========================================================================

    bool running = true;
    SDL_Event event;
    int frame_count = 0;
    double last_hot_reload_check = pz_time_now();
    double last_frame_time = pz_time_now();

    // Mouse state for turret aiming
    int mouse_x = WINDOW_WIDTH / 2;
    int mouse_y = WINDOW_HEIGHT / 2;
    bool mouse_left_pressed = false; // Track left mouse button

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
            case SDL_MOUSEMOTION: {
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
            } break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse_left_pressed = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse_left_pressed = false;
                }
                break;
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

        // Calculate delta time
        double current_time = pz_time_now();
        float dt
            = (float)(current_time - last_frame_time); // already in seconds
        last_frame_time = current_time;
        // Clamp dt to avoid physics issues on lag spikes
        if (dt > 0.1f)
            dt = 0.1f;
        if (dt < 0.0001f)
            dt = 0.0001f; // Avoid zero dt on first frame

        // ====================================================================
        // Tank input and physics
        // ====================================================================
        const Uint8 *keys = SDL_GetKeyboardState(NULL);

        // Get movement input (tank_pos.x = world X, tank_pos.y = world Z)
        //
        // IMPORTANT - SCREEN TO WORLD MAPPING (DO NOT CHANGE):
        // The camera looks from -Z towards +Z, positioned above.
        // This means world +X appears as LEFT on screen, -X as RIGHT.
        // World +Z appears as UP on screen, -Z as DOWN.
        //
        // So the mapping is:
        //   W/Up    -> +Y (which is +Z in 3D) -> moves tank UP on screen
        //   S/Down  -> -Y (which is -Z in 3D) -> moves tank DOWN on screen
        //   A/Left  -> +X                     -> moves tank LEFT on screen
        //   D/Right -> -X                     -> moves tank RIGHT on screen
        //
        pz_vec2 input_dir = { 0.0f, 0.0f };
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
            input_dir.y += 1.0f;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
            input_dir.y -= 1.0f;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
            input_dir.x += 1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
            input_dir.x -= 1.0f;

        // Apply acceleration in input direction
        if (pz_vec2_len_sq(input_dir) > 0.0f) {
            input_dir = pz_vec2_normalize(input_dir);
            tank_vel = pz_vec2_add(
                tank_vel, pz_vec2_scale(input_dir, tank_accel * dt));

            // Rotate body towards movement direction (with damping)
            float target_angle = atan2f(input_dir.x, input_dir.y);
            float angle_diff = target_angle - tank_angle;
            // Normalize angle difference to [-PI, PI]
            while (angle_diff > PZ_PI)
                angle_diff -= 2.0f * PZ_PI;
            while (angle_diff < -PZ_PI)
                angle_diff += 2.0f * PZ_PI;
            tank_angle += angle_diff * pz_minf(1.0f, tank_turn_speed * dt);
        }

        // Apply friction
        float speed = pz_vec2_len(tank_vel);
        if (speed > 0.0f) {
            float friction_amount = tank_friction * dt;
            if (friction_amount > speed)
                friction_amount = speed;
            tank_vel = pz_vec2_sub(tank_vel,
                pz_vec2_scale(pz_vec2_normalize(tank_vel), friction_amount));
        }

        // Clamp to max speed
        speed = pz_vec2_len(tank_vel);
        if (speed > tank_max_speed) {
            tank_vel
                = pz_vec2_scale(pz_vec2_normalize(tank_vel), tank_max_speed);
        }

        // Update position
        pz_vec2 new_pos = pz_vec2_add(tank_pos, pz_vec2_scale(tank_vel, dt));

        // ====================================================================
        // Wall collision detection
        // ====================================================================
        if (game_map) {
            // Check collision for X-axis movement (test points along X edges)
            // We check at current Y and at Y +/- radius to catch corner cases
            bool blocked_x = false;
            float test_x = new_pos.x;
            float test_y = tank_pos.y;
            // Check right edge
            if (pz_map_is_solid(
                    game_map, (pz_vec2) { test_x + tank_radius, test_y })
                || pz_map_is_solid(game_map,
                    (pz_vec2) {
                        test_x + tank_radius, test_y + tank_radius * 0.7f })
                || pz_map_is_solid(game_map,
                    (pz_vec2) {
                        test_x + tank_radius, test_y - tank_radius * 0.7f })) {
                blocked_x = true;
            }
            // Check left edge
            if (!blocked_x
                && (pz_map_is_solid(
                        game_map, (pz_vec2) { test_x - tank_radius, test_y })
                    || pz_map_is_solid(game_map,
                        (pz_vec2) {
                            test_x - tank_radius, test_y + tank_radius * 0.7f })
                    || pz_map_is_solid(game_map,
                        (pz_vec2) { test_x - tank_radius,
                            test_y - tank_radius * 0.7f }))) {
                blocked_x = true;
            }

            // Check collision for Y-axis movement (test points along Y edges)
            bool blocked_y = false;
            test_x = tank_pos.x;
            test_y = new_pos.y;
            // Check top edge (+Y)
            if (pz_map_is_solid(
                    game_map, (pz_vec2) { test_x, test_y + tank_radius })
                || pz_map_is_solid(game_map,
                    (pz_vec2) {
                        test_x + tank_radius * 0.7f, test_y + tank_radius })
                || pz_map_is_solid(game_map,
                    (pz_vec2) {
                        test_x - tank_radius * 0.7f, test_y + tank_radius })) {
                blocked_y = true;
            }
            // Check bottom edge (-Y)
            if (!blocked_y
                && (pz_map_is_solid(
                        game_map, (pz_vec2) { test_x, test_y - tank_radius })
                    || pz_map_is_solid(game_map,
                        (pz_vec2) {
                            test_x + tank_radius * 0.7f, test_y - tank_radius })
                    || pz_map_is_solid(game_map,
                        (pz_vec2) { test_x - tank_radius * 0.7f,
                            test_y - tank_radius }))) {
                blocked_y = true;
            }

            // Apply movement on unblocked axes
            if (!blocked_x) {
                tank_pos.x = new_pos.x;
            } else {
                tank_vel.x = 0;
            }

            if (!blocked_y) {
                tank_pos.y = new_pos.y;
            } else {
                tank_vel.y = 0;
            }
        } else {
            // No map, just move freely
            tank_pos = new_pos;
        }

        // ====================================================================
        // Track marks (when tank is moving)
        // ====================================================================
        if (tracks && pz_vec2_len(tank_vel) > 0.1f) {
            // Add track marks - tread offset is half the tank body width
            pz_tracks_add_mark(
                tracks, tank_pos.x, tank_pos.y, tank_angle, 0.45f);
        }

        // ====================================================================
        // Turret aiming (mouse controls)
        // ====================================================================
        pz_vec3 mouse_world
            = pz_camera_screen_to_world(&camera, mouse_x, mouse_y);
        // Calculate direction from tank to mouse in XZ plane
        float aim_dx = mouse_world.x - tank_pos.x;
        float aim_dz = mouse_world.z - tank_pos.y; // tank_pos.y is world Z
        float target_turret_angle = atan2f(aim_dx, aim_dz);

        // Smoothly rotate turret towards target (with damping)
        float turret_diff = target_turret_angle - turret_angle;
        // Normalize angle difference to [-PI, PI]
        while (turret_diff > PZ_PI)
            turret_diff -= 2.0f * PZ_PI;
        while (turret_diff < -PZ_PI)
            turret_diff += 2.0f * PZ_PI;
        turret_angle += turret_diff * pz_minf(1.0f, turret_turn_speed * dt);

        // ====================================================================
        // Firing (left mouse button)
        // ====================================================================
        fire_cooldown -= dt;
        if (fire_cooldown < 0.0f)
            fire_cooldown = 0.0f;

        if (mouse_left_pressed && fire_cooldown <= 0.0f) {
            // Calculate spawn position at end of barrel
            // turret_angle is world space rotation around Y axis
            // Forward direction in XZ plane: sin(angle), cos(angle)
            float spawn_dx = sinf(turret_angle) * barrel_length;
            float spawn_dz = cosf(turret_angle) * barrel_length;

            pz_vec2 spawn_pos = {
                tank_pos.x + spawn_dx,
                tank_pos.y + spawn_dz // tank_pos.y is world Z
            };

            // Direction matches turret facing, with tank momentum added
            pz_vec2 fire_dir = { sinf(turret_angle), cosf(turret_angle) };

            // Add tank velocity to bullet (inherit momentum)
            // Scale it down slightly so it's not too extreme
            pz_vec2 inherited_vel = pz_vec2_scale(tank_vel, 0.5f);
            pz_vec2 bullet_vel = pz_vec2_add(
                pz_vec2_scale(fire_dir, PZ_PROJECTILE_DEFAULT.speed),
                inherited_vel);

            // Calculate the new direction and speed from combined velocity
            float bullet_speed = pz_vec2_len(bullet_vel);
            if (bullet_speed > 0.001f) {
                fire_dir = pz_vec2_scale(bullet_vel, 1.0f / bullet_speed);
            }

            // Create a modified config with the adjusted speed
            pz_projectile_config fire_config = PZ_PROJECTILE_DEFAULT;
            fire_config.speed = bullet_speed;

            pz_projectile_spawn(
                projectile_mgr, spawn_pos, fire_dir, &fire_config, 0);

            fire_cooldown = fire_cooldown_time;
        }

        // ====================================================================
        // Update projectiles
        // ====================================================================
        pz_projectile_update(projectile_mgr, game_map, dt);

        // Check for hot-reload every 500ms
        double now = pz_time_now();
        if (now - last_hot_reload_check > 0.5) { // 500ms
            pz_texture_check_hot_reload(tex_manager);
            pz_map_hot_reload_check(map_hot_reload);
            last_hot_reload_check = now;
        }

        // Begin frame
        pz_debug_overlay_begin_frame(debug_overlay);
        pz_renderer_begin_frame(renderer);

        // Clear to dark gray (sky color for now)
        pz_renderer_clear(renderer, 0.2f, 0.2f, 0.25f, 1.0f, 1.0f);

        // ====================================================================
        // Update and render tracks (before drawing ground)
        // ====================================================================
        pz_tracks_update(tracks);

        // ====================================================================
        // Draw map
        // ====================================================================
        const pz_mat4 *vp = pz_camera_get_view_projection(&camera);
        {
            // Get track texture and UV transform
            pz_texture_handle track_tex = pz_tracks_get_texture(tracks);
            float track_scale_x = 0.0f, track_scale_z = 0.0f;
            float track_offset_x = 0.0f, track_offset_z = 0.0f;
            if (tracks) {
                pz_tracks_get_uv_transform(tracks, &track_scale_x,
                    &track_scale_z, &track_offset_x, &track_offset_z);
            }
            pz_map_renderer_draw(map_renderer, vp, track_tex, track_scale_x,
                track_scale_z, track_offset_x, track_offset_z);
        }

        // ====================================================================
        // Draw tank (M4.1 test)
        // ====================================================================
        if (entity_pipeline != PZ_INVALID_HANDLE) {
            // Light direction and color (same as walls)
            pz_vec3 light_dir = { 0.5f, 1.0f, 0.3f };
            pz_vec3 light_color = { 0.8f, 0.75f, 0.7f };
            pz_vec3 ambient = { 0.3f, 0.35f, 0.4f };

            // Tank body model matrix
            pz_mat4 body_model = pz_mat4_identity();
            body_model = pz_mat4_mul(body_model,
                pz_mat4_translate((pz_vec3) { tank_pos.x, 0.0f, tank_pos.y }));
            body_model = pz_mat4_mul(body_model, pz_mat4_rotate_y(tank_angle));

            pz_mat4 body_mvp = pz_mat4_mul(*vp, body_model);

            // Set uniforms for body
            pz_renderer_set_uniform_mat4(
                renderer, entity_shader, "u_mvp", &body_mvp);
            pz_renderer_set_uniform_mat4(
                renderer, entity_shader, "u_model", &body_model);
            pz_renderer_set_uniform_vec4(renderer, entity_shader, "u_color",
                (pz_vec4) { 0.3f, 0.5f, 0.3f, 1.0f }); // Olive green
            pz_renderer_set_uniform_vec3(
                renderer, entity_shader, "u_light_dir", light_dir);
            pz_renderer_set_uniform_vec3(
                renderer, entity_shader, "u_light_color", light_color);
            pz_renderer_set_uniform_vec3(
                renderer, entity_shader, "u_ambient", ambient);

            // Draw body
            pz_draw_cmd body_cmd = {
                .pipeline = entity_pipeline,
                .vertex_buffer = tank_body->buffer,
                .index_buffer = PZ_INVALID_HANDLE,
                .vertex_count = tank_body->vertex_count,
                .index_count = 0,
                .vertex_offset = 0,
                .index_offset = 0,
            };
            pz_renderer_draw(renderer, &body_cmd);

            // Turret model matrix (positioned on top of body, rotates
            // independently in world space)
            float turret_y_offset = 0.65f; // Height where turret sits on body
            pz_mat4 turret_model = pz_mat4_identity();
            turret_model = pz_mat4_mul(turret_model,
                pz_mat4_translate(
                    (pz_vec3) { tank_pos.x, turret_y_offset, tank_pos.y }));
            turret_model
                = pz_mat4_mul(turret_model, pz_mat4_rotate_y(turret_angle));

            pz_mat4 turret_mvp = pz_mat4_mul(*vp, turret_model);

            // Set uniforms for turret
            pz_renderer_set_uniform_mat4(
                renderer, entity_shader, "u_mvp", &turret_mvp);
            pz_renderer_set_uniform_mat4(
                renderer, entity_shader, "u_model", &turret_model);
            pz_renderer_set_uniform_vec4(renderer, entity_shader, "u_color",
                (pz_vec4) {
                    0.25f, 0.45f, 0.25f, 1.0f }); // Slightly darker green

            // Draw turret
            pz_draw_cmd turret_cmd = {
                .pipeline = entity_pipeline,
                .vertex_buffer = tank_turret->buffer,
                .index_buffer = PZ_INVALID_HANDLE,
                .vertex_count = tank_turret->vertex_count,
                .index_count = 0,
                .vertex_offset = 0,
                .index_offset = 0,
            };
            pz_renderer_draw(renderer, &turret_cmd);
        }

        // ====================================================================
        // Draw laser sight
        // ====================================================================
        if (laser_pipeline != PZ_INVALID_HANDLE && game_map) {
            // Calculate laser start position (at barrel tip)
            float laser_start_dx = sinf(turret_angle) * barrel_length;
            float laser_start_dz = cosf(turret_angle) * barrel_length;
            pz_vec2 laser_start = {
                tank_pos.x + laser_start_dx,
                tank_pos.y + laser_start_dz,
            };

            // Raycast to find where laser hits wall
            pz_vec2 laser_dir = { sinf(turret_angle), cosf(turret_angle) };
            bool hit_wall = false;
            pz_vec2 laser_end = pz_map_raycast(
                game_map, laser_start, laser_dir, laser_max_dist, &hit_wall);

            // Calculate laser length
            float laser_len = pz_vec2_dist(laser_start, laser_end);
            if (laser_len > 0.01f) {
                // Build quad perpendicular to laser direction
                // The laser is rendered at barrel height (same as projectile)
                float laser_height = 1.18f;

                // Perpendicular direction in XZ plane
                pz_vec2 perp = { -laser_dir.y, laser_dir.x };
                float half_w = laser_width * 0.5f;

                // Build 6 vertices for 2 triangles (quad)
                typedef struct {
                    float x, y, z;
                    float u, v;
                } laser_vertex;

                laser_vertex verts[6];

                // Bottom-left (start, left edge)
                pz_vec2 bl
                    = pz_vec2_add(laser_start, pz_vec2_scale(perp, -half_w));
                // Bottom-right (start, right edge)
                pz_vec2 br
                    = pz_vec2_add(laser_start, pz_vec2_scale(perp, half_w));
                // Top-left (end, left edge)
                pz_vec2 tl
                    = pz_vec2_add(laser_end, pz_vec2_scale(perp, -half_w));
                // Top-right (end, right edge)
                pz_vec2 tr
                    = pz_vec2_add(laser_end, pz_vec2_scale(perp, half_w));

                // Triangle 1: bl, br, tr
                verts[0]
                    = (laser_vertex) { bl.x, laser_height, bl.y, 0.0f, 0.0f };
                verts[1]
                    = (laser_vertex) { br.x, laser_height, br.y, 1.0f, 0.0f };
                verts[2]
                    = (laser_vertex) { tr.x, laser_height, tr.y, 1.0f, 1.0f };

                // Triangle 2: bl, tr, tl
                verts[3]
                    = (laser_vertex) { bl.x, laser_height, bl.y, 0.0f, 0.0f };
                verts[4]
                    = (laser_vertex) { tr.x, laser_height, tr.y, 1.0f, 1.0f };
                verts[5]
                    = (laser_vertex) { tl.x, laser_height, tl.y, 0.0f, 1.0f };

                // Upload vertices
                pz_renderer_update_buffer(
                    renderer, laser_vb, 0, verts, sizeof(verts));

                // Set uniforms
                pz_mat4 laser_mvp = *vp; // No model transform, vertices are in
                                         // world space
                pz_renderer_set_uniform_mat4(
                    renderer, laser_shader, "u_mvp", &laser_mvp);
                pz_renderer_set_uniform_vec4(renderer, laser_shader, "u_color",
                    (pz_vec4) {
                        1.0f, 0.2f, 0.2f, 0.6f }); // Red, semi-transparent

                // Draw
                pz_draw_cmd laser_cmd = {
                    .pipeline = laser_pipeline,
                    .vertex_buffer = laser_vb,
                    .vertex_count = 6,
                };
                pz_renderer_draw(renderer, &laser_cmd);
            }
        }

        // ====================================================================
        // Draw projectiles
        // ====================================================================
        pz_projectile_render(projectile_mgr, renderer, vp);

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

    // Cleanup laser sight
    if (laser_vb != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(renderer, laser_vb);
    }
    if (laser_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(renderer, laser_pipeline);
    }
    if (laser_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(renderer, laser_shader);
    }

    // Cleanup tank meshes (M4.1)
    if (entity_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(renderer, entity_pipeline);
    }
    if (entity_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(renderer, entity_shader);
    }
    pz_mesh_destroy(tank_turret, renderer);
    pz_mesh_destroy(tank_body, renderer);

    // Cleanup projectile system
    pz_projectile_manager_destroy(projectile_mgr, renderer);

    pz_tracks_destroy(tracks);
    pz_map_hot_reload_destroy(map_hot_reload);
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
