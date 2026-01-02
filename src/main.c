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
#include "game/pz_ai.h"
#include "game/pz_map.h"
#include "game/pz_map_render.h"
#include "game/pz_mesh.h"
#include "game/pz_particle.h"
#include "game/pz_powerup.h"
#include "game/pz_projectile.h"
#include "game/pz_tank.h"
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
    // Tank system (M4.3 - tank entity structure)
    // ========================================================================
    pz_tank_manager *tank_mgr = pz_tank_manager_create(renderer, NULL);

    // Spawn player tank at first spawn point (olive green)
    pz_vec2 player_spawn_pos = { 12.0f, 7.0f }; // Default
    if (game_map && pz_map_get_spawn_count(game_map) > 0) {
        const pz_spawn_point *sp = pz_map_get_spawn(game_map, 0);
        if (sp) {
            player_spawn_pos = sp->pos;
        }
    }
    pz_tank *player_tank = pz_tank_spawn(
        tank_mgr, player_spawn_pos, (pz_vec4) { 0.3f, 0.5f, 0.3f, 1.0f }, true);

    // ========================================================================
    // AI system (M7B.1, M7B.2 - enemy AI)
    // ========================================================================
    pz_ai_manager *ai_mgr = pz_ai_manager_create(tank_mgr, game_map);

    // Spawn enemies from map data
    if (game_map && ai_mgr) {
        int enemy_count = pz_map_get_enemy_count(game_map);
        for (int i = 0; i < enemy_count; i++) {
            const pz_enemy_spawn *es = pz_map_get_enemy(game_map, i);
            if (es) {
                pz_ai_spawn_enemy(
                    ai_mgr, es->pos, es->angle, (pz_enemy_level)es->level);
            }
        }
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Spawned %d enemies from map",
            enemy_count);
    }

    // ========================================================================
    // Projectile system (M6.1, M6.2, M6.3, M6.4)
    // ========================================================================
    pz_projectile_manager *projectile_mgr
        = pz_projectile_manager_create(renderer);

    // ========================================================================
    // Particle system (smoke effects)
    // ========================================================================
    pz_particle_manager *particle_mgr = pz_particle_manager_create(renderer);

    // ========================================================================
    // Powerup system
    // ========================================================================
    pz_powerup_manager *powerup_mgr = pz_powerup_manager_create(renderer);

    // Add powerups in the center of the map
    // Map is 24x14 tiles with tile_size 2.0, so world is 48x28 centered at
    // origin Center is (0, 0)
    pz_powerup_add(powerup_mgr, (pz_vec2) { -2.0f, 0.0f },
        PZ_POWERUP_MACHINE_GUN, 45.0f); // 45 second respawn
    pz_powerup_add(powerup_mgr, (pz_vec2) { 2.0f, 0.0f }, PZ_POWERUP_RICOCHET,
        45.0f); // 45 second respawn

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
    bool mouse_left_down = false; // Is left mouse button currently held?
    bool mouse_left_just_pressed = false; // Was it just pressed this frame?
    float scroll_accumulator = 0.0f; // Accumulated scroll for weapon switching
    const float SCROLL_THRESHOLD = 3.0f; // Amount of scroll needed to switch
    bool key_f_just_pressed = false; // F key for cycling weapons

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
                } else if (event.key.keysym.sym == SDLK_f) {
                    key_f_just_pressed = true;
                }
                break;
            case SDL_MOUSEMOTION: {
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
            } break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse_left_down = true;
                    mouse_left_just_pressed = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse_left_down = false;
                }
                break;
            case SDL_MOUSEWHEEL:
                // Scroll wheel for weapon switching (accumulate for touchpad)
                // y > 0 = scroll up, y < 0 = scroll down
                scroll_accumulator += (float)event.wheel.y;
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
        // Tank input and physics (using tank manager)
        // ====================================================================
        const Uint8 *keys = SDL_GetKeyboardState(NULL);

        // Build player input
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
        pz_tank_input player_input = { 0 };
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
            player_input.move_dir.y += 1.0f;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
            player_input.move_dir.y -= 1.0f;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
            player_input.move_dir.x += 1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
            player_input.move_dir.x -= 1.0f;

        // Calculate turret aim from mouse position
        if (player_tank && !(player_tank->flags & PZ_TANK_FLAG_DEAD)) {
            pz_vec3 mouse_world
                = pz_camera_screen_to_world(&camera, mouse_x, mouse_y);
            float aim_dx = mouse_world.x - player_tank->pos.x;
            float aim_dz = mouse_world.z - player_tank->pos.y;
            player_input.target_turret = atan2f(aim_dx, aim_dz);
            player_input.fire = mouse_left_down; // Still used for input struct

            // Update player tank
            pz_tank_update(tank_mgr, player_tank, &player_input, game_map, dt);

            // ====================================================================
            // Track marks (when tank is moving)
            // ====================================================================
            if (tracks && pz_vec2_len(player_tank->vel) > 0.1f) {
                pz_tracks_add_mark(tracks, player_tank->pos.x,
                    player_tank->pos.y, player_tank->body_angle, 0.45f);
            }

            // ====================================================================
            // Weapon switching (scroll wheel with threshold, or F key)
            // ====================================================================
            if (scroll_accumulator >= SCROLL_THRESHOLD) {
                pz_tank_cycle_weapon(player_tank, 1);
                scroll_accumulator = 0.0f;
            } else if (scroll_accumulator <= -SCROLL_THRESHOLD) {
                pz_tank_cycle_weapon(player_tank, -1);
                scroll_accumulator = 0.0f;
            }
            if (key_f_just_pressed) {
                pz_tank_cycle_weapon(player_tank, 1);
            }

            // ====================================================================
            // Powerup collection
            // ====================================================================
            pz_powerup_type collected = pz_powerup_check_collection(
                powerup_mgr, player_tank->pos, 0.7f); // Tank collision radius
            if (collected != PZ_POWERUP_NONE) {
                pz_tank_add_weapon(player_tank, (int)collected);
                pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Player collected: %s",
                    pz_powerup_type_name(collected));
            }

            // ====================================================================
            // Firing (left mouse button)
            // ====================================================================
            // Get weapon stats for current weapon
            int current_weapon = pz_tank_get_current_weapon(player_tank);
            const pz_weapon_stats *weapon
                = pz_weapon_get_stats((pz_powerup_type)current_weapon);

            // Determine if we should fire:
            // - Auto-fire weapons fire when mouse is held down
            // - Non-auto weapons only fire on new click
            bool should_fire
                = weapon->auto_fire ? mouse_left_down : mouse_left_just_pressed;

            // Check if we have room for more projectiles
            int active_projectiles
                = pz_projectile_count_by_owner(projectile_mgr, player_tank->id);
            bool can_fire = active_projectiles < weapon->max_active_projectiles;

            if (should_fire && can_fire && player_tank->fire_cooldown <= 0.0f) {
                pz_vec2 spawn_pos = pz_tank_get_barrel_tip(player_tank);
                pz_vec2 fire_dir = pz_tank_get_fire_direction(player_tank);

                // Build projectile config from weapon stats
                pz_projectile_config proj_config = {
                    .speed = weapon->projectile_speed,
                    .max_bounces = weapon->max_bounces,
                    .lifetime = -1.0f, // Infinite
                    .damage = weapon->damage,
                    .scale = weapon->projectile_scale,
                    .color = weapon->projectile_color,
                };

                pz_projectile_spawn(projectile_mgr, spawn_pos, fire_dir,
                    &proj_config, player_tank->id);

                player_tank->fire_cooldown = weapon->fire_cooldown;
            }
        }

        // Update all tanks (respawn timers, etc.)
        pz_tank_update_all(tank_mgr, game_map, dt);

        // ====================================================================
        // Update AI-controlled enemies
        // ====================================================================
        if (ai_mgr && player_tank
            && !(player_tank->flags & PZ_TANK_FLAG_DEAD)) {
            pz_ai_update(ai_mgr, player_tank->pos, dt);
            pz_ai_fire(ai_mgr, projectile_mgr);
        }

        // ====================================================================
        // Update powerups (animation, respawn timers)
        // ====================================================================
        pz_powerup_update(powerup_mgr, dt);

        // ====================================================================
        // Update projectiles (now with tank collision!)
        // ====================================================================
        pz_projectile_update(projectile_mgr, game_map, tank_mgr, dt);

        // Spawn smoke particles for projectile hits
        {
            pz_projectile_hit hits[PZ_MAX_PROJECTILE_HITS];
            int hit_count = pz_projectile_get_hits(
                projectile_mgr, hits, PZ_MAX_PROJECTILE_HITS);

            for (int i = 0; i < hit_count; i++) {
                // Projectile height is 1.18
                pz_vec3 hit_pos = { hits[i].pos.x, 1.18f, hits[i].pos.y };

                pz_smoke_config smoke = PZ_SMOKE_BULLET_IMPACT;
                smoke.position = hit_pos;

                // Bigger smoke for tank hits
                if (hits[i].type == PZ_HIT_TANK) {
                    smoke = PZ_SMOKE_TANK_HIT;
                    smoke.position = hit_pos;
                }

                pz_particle_spawn_smoke(particle_mgr, &smoke);
            }
        }

        // ====================================================================
        // Update particles
        // ====================================================================
        pz_particle_update(particle_mgr, dt);

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
        // Draw all tanks (using tank manager)
        // ====================================================================
        pz_tank_render(tank_mgr, renderer, vp);

        // ====================================================================
        // Draw powerups
        // ====================================================================
        pz_powerup_render(powerup_mgr, renderer, vp);

        // ====================================================================
        // Draw laser sight (only for player tank when alive)
        // ====================================================================
        if (laser_pipeline != PZ_INVALID_HANDLE && game_map && player_tank
            && !(player_tank->flags & PZ_TANK_FLAG_DEAD)) {
            // Calculate laser start position (at barrel tip)
            pz_vec2 laser_start = pz_tank_get_barrel_tip(player_tank);
            pz_vec2 laser_dir = pz_tank_get_fire_direction(player_tank);

            // Raycast to find where laser hits wall
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

        // ====================================================================
        // Draw particles (after opaque geometry, uses alpha blending)
        // ====================================================================
        {
            // Get camera right and up vectors for billboarding from view matrix
            // The view matrix columns contain the camera's basis vectors
            const pz_mat4 *view = pz_camera_get_view(&camera);
            // Column 0 = right, Column 1 = up (in view space, transposed)
            // For column-major: row 0 = right, row 1 = up
            pz_vec3 cam_right = { view->m[0], view->m[4], view->m[8] };
            pz_vec3 cam_up = { view->m[1], view->m[5], view->m[9] };
            pz_particle_render(particle_mgr, renderer, vp, cam_right, cam_up);
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

        // Reset per-frame input state
        mouse_left_just_pressed = false;
        key_f_just_pressed = false;
        // Note: scroll_accumulator is NOT reset - it accumulates until
        // threshold
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

    // Cleanup AI system
    pz_ai_manager_destroy(ai_mgr);

    // Cleanup tank system
    pz_tank_manager_destroy(tank_mgr, renderer);

    // Cleanup projectile system
    pz_projectile_manager_destroy(projectile_mgr, renderer);

    // Cleanup particle system
    pz_particle_manager_destroy(particle_mgr, renderer);

    // Cleanup powerup system
    pz_powerup_manager_destroy(powerup_mgr, renderer);

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
