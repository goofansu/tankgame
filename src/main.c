/*
 * Tank Game - Main Entry Point
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "third_party/sokol/sokol_app.h"

#include "core/pz_debug_cmd.h"
#include "core/pz_log.h"
#include "core/pz_math.h"
#include "core/pz_mem.h"
#include "core/pz_platform.h"
#include "core/pz_sim.h"
#include "core/pz_str.h"
#include "engine/pz_camera.h"
#include "engine/pz_debug_overlay.h"
#include "engine/pz_font.h"
#include "engine/render/pz_renderer.h"
#include "engine/render/pz_texture.h"
#include "game/pz_ai.h"
#include "game/pz_lighting.h"
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
#define SAPP_KEYCODE_COUNT (SAPP_KEYCODE_MENU + 1)

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

typedef struct {
    pz_vec2 pos;
    float timer; // Remaining time
    float duration; // Total duration
    bool is_tank; // Tank explosion vs bullet impact
} explosion_light;

#define MAX_EXPLOSION_LIGHTS 16

// Game states
typedef enum {
    GAME_STATE_PLAYING,
    GAME_STATE_PLAYER_DEAD, // Player died, waiting for respawn
    GAME_STATE_VICTORY, // All enemies defeated
} game_state;

typedef struct app_state {
    bool auto_screenshot;
    const char *screenshot_path;
    int screenshot_frames;
    const char *lightmap_debug_path;
    const char *map_path_arg;

    pz_renderer *renderer;
    pz_texture_manager *tex_manager;
    pz_camera camera;
    pz_map *game_map;
    pz_map_renderer *map_renderer;
    pz_map_hot_reload *map_hot_reload;
    pz_tracks *tracks;
    pz_lighting *lighting;
    pz_debug_overlay *debug_overlay;
    pz_font_manager *font_mgr;
    pz_font *font_russo;
    pz_font *font_caveat;

    pz_tank_manager *tank_mgr;
    pz_tank *player_tank;
    pz_ai_manager *ai_mgr;
    pz_projectile_manager *projectile_mgr;
    pz_particle_manager *particle_mgr;
    pz_powerup_manager *powerup_mgr;

    pz_shader_handle laser_shader;
    pz_pipeline_handle laser_pipeline;
    pz_buffer_handle laser_vb;

    pz_sim *sim; // Fixed timestep simulation system

    explosion_light explosion_lights[MAX_EXPLOSION_LIGHTS];

    // Game state
    game_state state;
    int initial_enemy_count;
    float victory_timer; // Timer for restart prompt

    int frame_count;
    double last_hot_reload_check;
    double last_frame_time;

    float mouse_x;
    float mouse_y;
    bool mouse_left_down;
    bool mouse_left_just_pressed;
    float scroll_accumulator;
    bool key_f_just_pressed;
    bool key_down[SAPP_KEYCODE_COUNT];
} app_state;

static app_state g_app;

static const float LASER_WIDTH = 0.08f;
static const float LASER_MAX_DIST = 50.0f;

static void
parse_args(int argc, char *argv[])
{
    g_app.auto_screenshot = false;
    g_app.screenshot_path = NULL;
    g_app.screenshot_frames = 1;
    g_app.lightmap_debug_path = NULL;
    g_app.map_path_arg = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            g_app.auto_screenshot = true;
            g_app.screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--screenshot-frames") == 0
            && i + 1 < argc) {
            g_app.screenshot_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--lightmap-debug") == 0 && i + 1 < argc) {
            g_app.lightmap_debug_path = argv[++i];
        } else if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
            g_app.map_path_arg = argv[++i];
        }
    }
}

static void
app_init(void)
{
    int width = sapp_width();
    int height = sapp_height();

    printf("Tank Game - Starting...\n");

#ifdef PZ_DEBUG
    printf("Build: Debug\n");
#elif defined(PZ_DEV)
    printf("Build: Dev\n");
#elif defined(PZ_RELEASE)
    printf("Build: Release\n");
#endif

    pz_log_init();
    pz_time_init();

    pz_renderer_config renderer_config = {
        .backend = PZ_BACKEND_SOKOL,
        .window_handle = NULL,
        .viewport_width = width,
        .viewport_height = height,
    };

    g_app.renderer = pz_renderer_create(&renderer_config);
    if (!g_app.renderer) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_CORE, "Failed to create renderer");
        sapp_quit();
        return;
    }

    g_app.tex_manager = pz_texture_manager_create(g_app.renderer);

    pz_camera_init(&g_app.camera, width, height);

    const char *map_path = g_app.map_path_arg ? g_app.map_path_arg
                                              : "assets/maps/night_arena.map";
    g_app.game_map = pz_map_load(map_path);
    if (!g_app.game_map) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Failed to load map from %s, creating test map", map_path);
        g_app.game_map = pz_map_create_test();
        if (g_app.game_map) {
            pz_map_save(g_app.game_map, map_path);
        }
    }
    if (!g_app.game_map) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to create map");
    }

    if (g_app.game_map) {
        pz_camera_fit_map(&g_app.camera, g_app.game_map->world_width,
            g_app.game_map->world_height, 20.0f);
    }

    g_app.map_renderer
        = pz_map_renderer_create(g_app.renderer, g_app.tex_manager);
    if (g_app.map_renderer && g_app.game_map) {
        pz_map_renderer_set_map(g_app.map_renderer, g_app.game_map);
    }

    if (g_app.map_renderer) {
        g_app.map_hot_reload = pz_map_hot_reload_create(
            map_path, &g_app.game_map, g_app.map_renderer);
    }

    if (g_app.game_map) {
        pz_tracks_config track_config = {
            .world_width = g_app.game_map->world_width,
            .world_height = g_app.game_map->world_height,
            .texture_size = 1024,
        };
        g_app.tracks = pz_tracks_create(
            g_app.renderer, g_app.tex_manager, &track_config);
    }

    if (g_app.game_map) {
        const pz_map_lighting *map_light = pz_map_get_lighting(g_app.game_map);
        pz_lighting_config light_config = {
            .world_width = g_app.game_map->world_width,
            .world_height = g_app.game_map->world_height,
            .texture_size = 512,
            .ambient = map_light->ambient_color,
        };
        g_app.lighting = pz_lighting_create(g_app.renderer, &light_config);
    }

    pz_debug_cmd_init(NULL);

    g_app.debug_overlay = pz_debug_overlay_create(g_app.renderer);
    if (!g_app.debug_overlay) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE, "Failed to create debug overlay");
    }

    // Initialize font system
    g_app.font_mgr = pz_font_manager_create(g_app.renderer);
    if (g_app.font_mgr) {
        g_app.font_russo
            = pz_font_load(g_app.font_mgr, "assets/fonts/RussoOne-Regular.ttf");
        if (!g_app.font_russo) {
            pz_log(
                PZ_LOG_WARN, PZ_LOG_CAT_CORE, "Failed to load Russo One font");
        }
        g_app.font_caveat = pz_font_load(
            g_app.font_mgr, "assets/fonts/CaveatBrush-Regular.ttf");
        if (!g_app.font_caveat) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE,
                "Failed to load Caveat Brush font");
        }
    }

    g_app.tank_mgr = pz_tank_manager_create(g_app.renderer, NULL);

    pz_vec2 player_spawn_pos = { 12.0f, 7.0f };
    if (g_app.game_map && pz_map_get_spawn_count(g_app.game_map) > 0) {
        const pz_spawn_point *sp = pz_map_get_spawn(g_app.game_map, 0);
        if (sp) {
            player_spawn_pos = sp->pos;
        }
    }
    g_app.player_tank = pz_tank_spawn(g_app.tank_mgr, player_spawn_pos,
        (pz_vec4) { 0.3f, 0.5f, 0.3f, 1.0f }, true);

    g_app.ai_mgr = pz_ai_manager_create(g_app.tank_mgr, g_app.game_map);

    if (g_app.game_map && g_app.ai_mgr) {
        int enemy_count = pz_map_get_enemy_count(g_app.game_map);
        for (int i = 0; i < enemy_count; i++) {
            const pz_enemy_spawn *es = pz_map_get_enemy(g_app.game_map, i);
            if (es) {
                pz_ai_spawn_enemy(g_app.ai_mgr, es->pos, es->angle,
                    (pz_enemy_level)es->level);
            }
        }
        g_app.initial_enemy_count = enemy_count;
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Spawned %d enemies from map",
            enemy_count);
    }

    g_app.state = GAME_STATE_PLAYING;
    g_app.victory_timer = 0.0f;

    g_app.projectile_mgr = pz_projectile_manager_create(g_app.renderer);
    g_app.particle_mgr = pz_particle_manager_create(g_app.renderer);
    g_app.powerup_mgr = pz_powerup_manager_create(g_app.renderer);

    pz_powerup_add(g_app.powerup_mgr, (pz_vec2) { -2.0f, 0.0f },
        PZ_POWERUP_MACHINE_GUN, 45.0f);
    pz_powerup_add(g_app.powerup_mgr, (pz_vec2) { 2.0f, 0.0f },
        PZ_POWERUP_RICOCHET, 45.0f);

    g_app.laser_shader = pz_renderer_load_shader(
        g_app.renderer, "shaders/laser.vert", "shaders/laser.frag", "laser");

    g_app.laser_pipeline = PZ_INVALID_HANDLE;
    g_app.laser_vb = PZ_INVALID_HANDLE;

    if (g_app.laser_shader != PZ_INVALID_HANDLE) {
        pz_vertex_attr laser_attrs[] = {
            { .name = "a_position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
            { .name = "a_texcoord",
                .type = PZ_ATTR_FLOAT2,
                .offset = 3 * sizeof(float) },
        };

        pz_pipeline_desc laser_desc = {
            .shader = g_app.laser_shader,
            .vertex_layout = { .attrs = laser_attrs,
                .attr_count = 2,
                .stride = sizeof(float) * 5 },
            .blend = PZ_BLEND_ALPHA,
            .depth = PZ_DEPTH_READ,
            .cull = PZ_CULL_NONE,
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        g_app.laser_pipeline
            = pz_renderer_create_pipeline(g_app.renderer, &laser_desc);

        pz_buffer_desc laser_vb_desc = {
            .type = PZ_BUFFER_VERTEX,
            .usage = PZ_BUFFER_DYNAMIC,
            .data = NULL,
            .size = 6 * sizeof(float) * 5,
        };
        g_app.laser_vb
            = pz_renderer_create_buffer(g_app.renderer, &laser_vb_desc);
    }

    // Initialize simulation system with fixed timestep
    // Seed with current time for variety, but can be set to fixed value for
    // replays
    g_app.sim = pz_sim_create((uint32_t)time(NULL));

    g_app.frame_count = 0;
    g_app.last_hot_reload_check = pz_time_now();
    g_app.last_frame_time = pz_time_now();

    g_app.mouse_x = (float)width * 0.5f;
    g_app.mouse_y = (float)height * 0.5f;
    g_app.mouse_left_down = false;
    g_app.mouse_left_just_pressed = false;
    g_app.scroll_accumulator = 0.0f;
    g_app.key_f_just_pressed = false;

    memset(g_app.explosion_lights, 0, sizeof(g_app.explosion_lights));
}

static void
app_frame(void)
{
    if (!g_app.renderer)
        return;

    if (!pz_debug_cmd_poll(g_app.renderer)) {
        sapp_quit();
        return;
    }

    double current_time = pz_time_now();
    float frame_dt = (float)(current_time - g_app.last_frame_time);
    g_app.last_frame_time = current_time;
    if (frame_dt > 0.1f)
        frame_dt = 0.1f;
    if (frame_dt < 0.0001f)
        frame_dt = 0.0001f;

    // Determine number of simulation ticks to run this frame
    int sim_ticks = pz_sim_accumulate(g_app.sim, frame_dt);
    float dt = pz_sim_dt(); // Fixed timestep for simulation

    // Gather input (once per frame)
    pz_tank_input player_input = { 0 };
    if (g_app.key_down[SAPP_KEYCODE_W] || g_app.key_down[SAPP_KEYCODE_UP])
        player_input.move_dir.y += 1.0f;
    if (g_app.key_down[SAPP_KEYCODE_S] || g_app.key_down[SAPP_KEYCODE_DOWN])
        player_input.move_dir.y -= 1.0f;
    if (g_app.key_down[SAPP_KEYCODE_A] || g_app.key_down[SAPP_KEYCODE_LEFT])
        player_input.move_dir.x += 1.0f;
    if (g_app.key_down[SAPP_KEYCODE_D] || g_app.key_down[SAPP_KEYCODE_RIGHT])
        player_input.move_dir.x -= 1.0f;

    if (g_app.player_tank && !(g_app.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
        pz_vec3 mouse_world = pz_camera_screen_to_world(
            &g_app.camera, (int)g_app.mouse_x, (int)g_app.mouse_y);
        float aim_dx = mouse_world.x - g_app.player_tank->pos.x;
        float aim_dz = mouse_world.z - g_app.player_tank->pos.y;
        player_input.target_turret = atan2f(aim_dx, aim_dz);
        player_input.fire = g_app.mouse_left_down;
    }

    // Handle weapon cycling (once per frame, not per sim tick)
    if (g_app.player_tank && !(g_app.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
        if (g_app.scroll_accumulator >= 3.0f) {
            pz_tank_cycle_weapon(g_app.player_tank, 1);
            g_app.scroll_accumulator = 0.0f;
        } else if (g_app.scroll_accumulator <= -3.0f) {
            pz_tank_cycle_weapon(g_app.player_tank, -1);
            g_app.scroll_accumulator = 0.0f;
        }
        if (g_app.key_f_just_pressed) {
            pz_tank_cycle_weapon(g_app.player_tank, 1);
        }
    }

    // =========================================================================
    // FIXED TIMESTEP SIMULATION LOOP
    // Run N simulation ticks at fixed dt for deterministic gameplay
    // =========================================================================
    for (int tick = 0; tick < sim_ticks; tick++) {
        pz_sim_begin_tick(g_app.sim);

        // Player tank update
        if (g_app.player_tank
            && !(g_app.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
            pz_tank_update(g_app.tank_mgr, g_app.player_tank, &player_input,
                g_app.game_map, dt);

            // Track marks for player
            if (g_app.tracks && pz_vec2_len(g_app.player_tank->vel) > 0.1f) {
                pz_tracks_add_mark(g_app.tracks, g_app.player_tank->id,
                    g_app.player_tank->pos.x, g_app.player_tank->pos.y,
                    g_app.player_tank->body_angle, 0.45f);
            }

            // Powerup collection
            pz_powerup_type collected = pz_powerup_check_collection(
                g_app.powerup_mgr, g_app.player_tank->pos, 0.7f);
            if (collected != PZ_POWERUP_NONE) {
                pz_tank_add_weapon(g_app.player_tank, (int)collected);
                pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Player collected: %s",
                    pz_powerup_type_name(collected));
            }

            // Player firing
            int current_weapon = pz_tank_get_current_weapon(g_app.player_tank);
            const pz_weapon_stats *weapon
                = pz_weapon_get_stats((pz_powerup_type)current_weapon);

            bool should_fire = weapon->auto_fire
                ? g_app.mouse_left_down
                : g_app.mouse_left_just_pressed;

            int active_projectiles = pz_projectile_count_by_owner(
                g_app.projectile_mgr, g_app.player_tank->id);
            bool can_fire = active_projectiles < weapon->max_active_projectiles;

            if (should_fire && can_fire
                && g_app.player_tank->fire_cooldown <= 0.0f) {
                pz_vec2 spawn_pos = pz_tank_get_barrel_tip(g_app.player_tank);
                pz_vec2 fire_dir
                    = pz_tank_get_fire_direction(g_app.player_tank);

                pz_projectile_config proj_config = {
                    .speed = weapon->projectile_speed,
                    .max_bounces = weapon->max_bounces,
                    .lifetime = -1.0f,
                    .damage = weapon->damage,
                    .scale = weapon->projectile_scale,
                    .color = weapon->projectile_color,
                };

                pz_projectile_spawn(g_app.projectile_mgr, spawn_pos, fire_dir,
                    &proj_config, g_app.player_tank->id);

                g_app.player_tank->fire_cooldown = weapon->fire_cooldown;
            }
        }

        // Update all tanks (respawn timers, etc.)
        pz_tank_update_all(g_app.tank_mgr, g_app.game_map, dt);

        // AI update
        if (g_app.ai_mgr && g_app.player_tank
            && !(g_app.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
            pz_ai_update(
                g_app.ai_mgr, g_app.player_tank->pos, g_app.projectile_mgr, dt);
            pz_ai_fire(g_app.ai_mgr, g_app.projectile_mgr);

            // Track marks for enemy tanks
            if (g_app.tracks) {
                for (int i = 0; i < g_app.ai_mgr->controller_count; i++) {
                    pz_ai_controller *ctrl = &g_app.ai_mgr->controllers[i];
                    pz_tank *enemy
                        = pz_tank_get_by_id(g_app.tank_mgr, ctrl->tank_id);
                    if (enemy && !(enemy->flags & PZ_TANK_FLAG_DEAD)) {
                        if (pz_vec2_len(enemy->vel) > 0.1f) {
                            pz_tracks_add_mark(g_app.tracks, enemy->id,
                                enemy->pos.x, enemy->pos.y, enemy->body_angle,
                                0.45f);
                        }
                    }
                }
            }
        }

        // Powerup and projectile updates
        pz_powerup_update(g_app.powerup_mgr, dt);
        pz_projectile_update(
            g_app.projectile_mgr, g_app.game_map, g_app.tank_mgr, dt);

        // Hash game state for determinism verification
        if (g_app.player_tank) {
            pz_sim_hash_vec2(
                g_app.sim, g_app.player_tank->pos.x, g_app.player_tank->pos.y);
            pz_sim_hash_float(g_app.sim, g_app.player_tank->body_angle);
        }

        pz_sim_end_tick(g_app.sim);
    }

    {
        pz_projectile_hit hits[PZ_MAX_PROJECTILE_HITS];
        int hit_count = pz_projectile_get_hits(
            g_app.projectile_mgr, hits, PZ_MAX_PROJECTILE_HITS);

        for (int i = 0; i < hit_count; i++) {
            pz_vec3 hit_pos = { hits[i].pos.x, 1.18f, hits[i].pos.y };

            pz_smoke_config smoke = PZ_SMOKE_BULLET_IMPACT;
            smoke.position = hit_pos;

            if (hits[i].type == PZ_HIT_TANK) {
                smoke = PZ_SMOKE_TANK_HIT;
                smoke.position = hit_pos;
            }

            pz_particle_spawn_smoke(g_app.particle_mgr, &smoke);

            for (int j = 0; j < MAX_EXPLOSION_LIGHTS; j++) {
                if (g_app.explosion_lights[j].timer <= 0.0f) {
                    g_app.explosion_lights[j].pos = hits[i].pos;
                    g_app.explosion_lights[j].is_tank = false;
                    g_app.explosion_lights[j].duration = 0.15f;
                    g_app.explosion_lights[j].timer
                        = g_app.explosion_lights[j].duration;
                    break;
                }
            }
        }
    }

    // Process tank death events
    {
        pz_tank_death_event death_events[PZ_MAX_DEATH_EVENTS];
        int death_count = pz_tank_get_death_events(
            g_app.tank_mgr, death_events, PZ_MAX_DEATH_EVENTS);

        for (int i = 0; i < death_count; i++) {
            pz_vec3 death_pos
                = { death_events[i].pos.x, 0.6f, death_events[i].pos.y };

            // Spawn explosion particles
            pz_smoke_config explosion = PZ_SMOKE_TANK_EXPLOSION;
            explosion.position = death_pos;
            pz_particle_spawn_smoke(g_app.particle_mgr, &explosion);

            // Add explosion light
            for (int j = 0; j < MAX_EXPLOSION_LIGHTS; j++) {
                if (g_app.explosion_lights[j].timer <= 0.0f) {
                    g_app.explosion_lights[j].pos = death_events[i].pos;
                    g_app.explosion_lights[j].is_tank = true;
                    g_app.explosion_lights[j].duration = 0.4f;
                    g_app.explosion_lights[j].timer
                        = g_app.explosion_lights[j].duration;
                    break;
                }
            }

            // Check win condition (all enemies defeated)
            if (!death_events[i].is_player && g_app.state == GAME_STATE_PLAYING
                && g_app.initial_enemy_count > 0) {
                int enemies_remaining
                    = pz_tank_count_enemies_alive(g_app.tank_mgr);
                if (enemies_remaining == 0) {
                    g_app.state = GAME_STATE_VICTORY;
                    g_app.victory_timer = 0.0f;
                    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
                        "Victory! All enemies defeated.");
                }
            }
        }

        // Clear events for next frame
        pz_tank_clear_death_events(g_app.tank_mgr);
    }

    // =========================================================================
    // VISUAL-ONLY UPDATES (use frame_dt for smooth animation)
    // =========================================================================
    for (int i = 0; i < MAX_EXPLOSION_LIGHTS; i++) {
        if (g_app.explosion_lights[i].timer > 0.0f) {
            g_app.explosion_lights[i].timer -= frame_dt;
        }
    }

    pz_particle_update(g_app.particle_mgr, frame_dt);

    double now = pz_time_now();
    if (now - g_app.last_hot_reload_check > 0.5) {
        pz_texture_check_hot_reload(g_app.tex_manager);
        pz_map_hot_reload_check(g_app.map_hot_reload);
        g_app.last_hot_reload_check = now;
    }

    pz_debug_overlay_begin_frame(g_app.debug_overlay);
    pz_renderer_begin_frame(g_app.renderer);
    pz_renderer_clear(g_app.renderer, 0.2f, 0.2f, 0.25f, 1.0f, 1.0f);

    pz_tracks_update(g_app.tracks);

    if (g_app.lighting && g_app.game_map) {
        pz_lighting_clear_occluders(g_app.lighting);
        pz_lighting_add_map_occluders(g_app.lighting, g_app.game_map);

        pz_lighting_clear_lights(g_app.lighting);

        for (int i = 0; i < PZ_MAX_TANKS; i++) {
            pz_tank *tank = &g_app.tank_mgr->tanks[i];
            if ((tank->flags & PZ_TANK_FLAG_ACTIVE)
                && !(tank->flags & PZ_TANK_FLAG_DEAD)) {

                float light_offset = 0.8f;
                pz_vec2 light_dir
                    = { sinf(tank->turret_angle), cosf(tank->turret_angle) };
                pz_vec2 light_pos = pz_vec2_add(
                    tank->pos, pz_vec2_scale(light_dir, light_offset));

                if (g_app.game_map) {
                    bool hit = false;
                    pz_vec2 hit_pos = pz_map_raycast(g_app.game_map, tank->pos,
                        light_dir, light_offset, &hit);
                    if (hit) {
                        light_pos = hit_pos;
                    }
                }

                pz_vec3 light_color;
                if (tank->flags & PZ_TANK_FLAG_PLAYER) {
                    light_color = (pz_vec3) { 0.9f, 0.95f, 1.0f };
                } else {
                    light_color = (pz_vec3) { 1.0f, 0.6f, 0.4f };
                }

                float light_dir_2d = atan2f(
                    cosf(tank->turret_angle), sinf(tank->turret_angle));

                pz_lighting_add_spotlight(g_app.lighting, light_pos,
                    light_dir_2d, light_color, 1.2f, 22.5f, PZ_PI * 0.35f,
                    0.3f);
            }
        }

        for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
            pz_projectile *proj = &g_app.projectile_mgr->projectiles[i];
            if (proj->active) {
                pz_vec3 proj_light_color
                    = { proj->color.x, proj->color.y, proj->color.z };

                float backward_angle
                    = atan2f(-proj->velocity.y, -proj->velocity.x);

                pz_lighting_add_spotlight(g_app.lighting, proj->pos,
                    backward_angle, proj_light_color, 0.4f, 2.8f,
                    PZ_PI * 0.125f, 0.5f);
            }
        }

        for (int i = 0; i < MAX_EXPLOSION_LIGHTS; i++) {
            if (g_app.explosion_lights[i].timer > 0.0f) {
                float t = g_app.explosion_lights[i].timer
                    / g_app.explosion_lights[i].duration;
                float intensity = t * t;

                if (g_app.explosion_lights[i].is_tank) {
                    pz_vec3 exp_color = { 1.0f, 0.3f + 0.5f * t, 0.1f * t };
                    pz_lighting_add_point_light(g_app.lighting,
                        g_app.explosion_lights[i].pos, exp_color,
                        3.0f * intensity, 6.0f);
                } else {
                    pz_vec3 exp_color = { 0.7f, 0.8f, 1.0f };
                    pz_lighting_add_point_light(g_app.lighting,
                        g_app.explosion_lights[i].pos, exp_color,
                        2.0f * intensity, 4.0f);
                }
            }
        }

        for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
            pz_powerup *powerup = &g_app.powerup_mgr->powerups[i];
            if (!powerup->active || powerup->collected)
                continue;

            const pz_weapon_stats *stats = pz_weapon_get_stats(powerup->type);
            pz_vec3 powerup_color = { stats->projectile_color.x,
                stats->projectile_color.y, stats->projectile_color.z };

            float flicker = pz_powerup_get_flicker(g_app.powerup_mgr, i);

            pz_lighting_add_point_light(g_app.lighting, powerup->pos,
                powerup_color, 1.0f * flicker, 3.5f);
        }

        pz_lighting_render(g_app.lighting);
    }

    const pz_mat4 *vp = pz_camera_get_view_projection(&g_app.camera);

    pz_map_render_params render_params = { 0 };
    if (g_app.tracks) {
        render_params.track_texture = pz_tracks_get_texture(g_app.tracks);
        pz_tracks_get_uv_transform(g_app.tracks, &render_params.track_scale_x,
            &render_params.track_scale_z, &render_params.track_offset_x,
            &render_params.track_offset_z);
    }
    if (g_app.lighting) {
        render_params.light_texture = pz_lighting_get_texture(g_app.lighting);
        pz_lighting_get_uv_transform(g_app.lighting,
            &render_params.light_scale_x, &render_params.light_scale_z,
            &render_params.light_offset_x, &render_params.light_offset_z);
    }
    if (g_app.game_map) {
        const pz_map_lighting *map_light = pz_map_get_lighting(g_app.game_map);
        render_params.has_sun = map_light->has_sun;
        render_params.sun_direction = map_light->sun_direction;
        render_params.sun_color = map_light->sun_color;
    }

    pz_map_renderer_draw(g_app.map_renderer, vp, &render_params);

    pz_tank_render_params tank_params = { 0 };
    if (g_app.lighting) {
        tank_params.light_texture = pz_lighting_get_texture(g_app.lighting);
        pz_lighting_get_uv_transform(g_app.lighting, &tank_params.light_scale_x,
            &tank_params.light_scale_z, &tank_params.light_offset_x,
            &tank_params.light_offset_z);
    }
    pz_tank_render(g_app.tank_mgr, g_app.renderer, vp, &tank_params);

    pz_powerup_render(g_app.powerup_mgr, g_app.renderer, vp);

    if (g_app.laser_pipeline != PZ_INVALID_HANDLE && g_app.game_map
        && g_app.player_tank
        && !(g_app.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
        pz_vec2 laser_start = pz_tank_get_barrel_tip(g_app.player_tank);
        pz_vec2 laser_dir = pz_tank_get_fire_direction(g_app.player_tank);

        bool hit_wall = false;
        pz_vec2 laser_end = pz_map_raycast(
            g_app.game_map, laser_start, laser_dir, LASER_MAX_DIST, &hit_wall);

        float laser_len = pz_vec2_dist(laser_start, laser_end);
        if (laser_len > 0.01f) {
            float laser_height = 1.18f;

            pz_vec2 perp = { -laser_dir.y, laser_dir.x };
            float half_w = LASER_WIDTH * 0.5f;

            typedef struct {
                float x, y, z;
                float u, v;
            } laser_vertex;

            laser_vertex verts[6];

            pz_vec2 bl = pz_vec2_add(laser_start, pz_vec2_scale(perp, -half_w));
            pz_vec2 br = pz_vec2_add(laser_start, pz_vec2_scale(perp, half_w));
            pz_vec2 tl = pz_vec2_add(laser_end, pz_vec2_scale(perp, -half_w));
            pz_vec2 tr = pz_vec2_add(laser_end, pz_vec2_scale(perp, half_w));

            verts[0] = (laser_vertex) { bl.x, laser_height, bl.y, 0.0f, 0.0f };
            verts[1] = (laser_vertex) { br.x, laser_height, br.y, 1.0f, 0.0f };
            verts[2] = (laser_vertex) { tr.x, laser_height, tr.y, 1.0f, 1.0f };
            verts[3] = (laser_vertex) { bl.x, laser_height, bl.y, 0.0f, 0.0f };
            verts[4] = (laser_vertex) { tr.x, laser_height, tr.y, 1.0f, 1.0f };
            verts[5] = (laser_vertex) { tl.x, laser_height, tl.y, 0.0f, 1.0f };

            pz_renderer_update_buffer(
                g_app.renderer, g_app.laser_vb, 0, verts, sizeof(verts));

            pz_mat4 laser_mvp = *vp;
            pz_renderer_set_uniform_mat4(
                g_app.renderer, g_app.laser_shader, "u_mvp", &laser_mvp);
            pz_renderer_set_uniform_vec4(g_app.renderer, g_app.laser_shader,
                "u_color", (pz_vec4) { 1.0f, 0.2f, 0.2f, 0.6f });

            pz_draw_cmd laser_cmd = {
                .pipeline = g_app.laser_pipeline,
                .vertex_buffer = g_app.laser_vb,
                .vertex_count = 6,
            };
            pz_renderer_draw(g_app.renderer, &laser_cmd);
        }
    }

    pz_projectile_render_params proj_params = { 0 };
    if (g_app.lighting) {
        proj_params.light_texture = pz_lighting_get_texture(g_app.lighting);
        pz_lighting_get_uv_transform(g_app.lighting, &proj_params.light_scale_x,
            &proj_params.light_scale_z, &proj_params.light_offset_x,
            &proj_params.light_offset_z);
    }
    pz_projectile_render(
        g_app.projectile_mgr, g_app.renderer, vp, &proj_params);

    {
        const pz_mat4 *view = pz_camera_get_view(&g_app.camera);
        pz_vec3 cam_right = { view->m[0], view->m[4], view->m[8] };
        pz_vec3 cam_up = { view->m[1], view->m[5], view->m[9] };
        pz_particle_render(
            g_app.particle_mgr, g_app.renderer, vp, cam_right, cam_up);
    }

    pz_debug_overlay_render(g_app.debug_overlay);
    pz_debug_overlay_end_frame(g_app.debug_overlay);

    // Render HUD
    if (g_app.font_mgr && g_app.font_russo) {
        pz_font_begin_frame(g_app.font_mgr);

        // Get logical viewport size (framebuffer / dpi_scale)
        int fb_width, fb_height;
        pz_renderer_get_viewport(g_app.renderer, &fb_width, &fb_height);
        float dpi_scale = sapp_dpi_scale();
        float vp_width = (float)fb_width / dpi_scale;
        float vp_height = (float)fb_height / dpi_scale;

        // Font sizes and positions are in logical pixels - DPI scaling is
        // handled internally
        pz_text_style health_style
            = PZ_TEXT_STYLE_DEFAULT(g_app.font_russo, 36.0f);
        health_style.align_h = PZ_FONT_ALIGN_RIGHT;
        health_style.align_v = PZ_FONT_ALIGN_BOTTOM;

        // White text with black outline for visibility
        health_style.color = pz_vec4_new(1.0f, 1.0f, 1.0f, 1.0f);
        health_style.outline_width = 3.0f;
        health_style.outline_color = pz_vec4_new(0.0f, 0.0f, 0.0f, 1.0f);

        // Player health (bottom-right)
        if (g_app.player_tank) {
            pz_font_drawf(g_app.font_mgr, &health_style, vp_width - 20.0f,
                vp_height - 20.0f, "HP: %d", g_app.player_tank->health);
        }

        // Enemies remaining (top-right)
        if (g_app.initial_enemy_count > 0) {
            int enemies_alive = pz_tank_count_enemies_alive(g_app.tank_mgr);

            pz_text_style enemy_style
                = PZ_TEXT_STYLE_DEFAULT(g_app.font_russo, 28.0f);
            enemy_style.align_h = PZ_FONT_ALIGN_RIGHT;
            enemy_style.align_v = PZ_FONT_ALIGN_TOP;
            enemy_style.color = pz_vec4_new(1.0f, 0.8f, 0.6f, 1.0f);
            enemy_style.outline_width = 2.0f;
            enemy_style.outline_color = pz_vec4_new(0.0f, 0.0f, 0.0f, 1.0f);

            pz_font_drawf(g_app.font_mgr, &enemy_style, vp_width - 20.0f, 20.0f,
                "Enemies: %d", enemies_alive);
        }

        // Victory message (centered)
        if (g_app.state == GAME_STATE_VICTORY) {
            g_app.victory_timer += frame_dt;

            pz_text_style victory_style
                = PZ_TEXT_STYLE_DEFAULT(g_app.font_russo, 64.0f);
            victory_style.align_h = PZ_FONT_ALIGN_CENTER;
            victory_style.align_v = PZ_FONT_ALIGN_CENTER;
            victory_style.color = pz_vec4_new(1.0f, 0.9f, 0.3f, 1.0f);
            victory_style.outline_width = 4.0f;
            victory_style.outline_color = pz_vec4_new(0.2f, 0.15f, 0.0f, 1.0f);

            pz_font_draw(g_app.font_mgr, &victory_style, vp_width * 0.5f,
                vp_height * 0.4f, "VICTORY!");

            // Show restart prompt after a short delay
            if (g_app.victory_timer > 1.5f) {
                pz_text_style restart_style
                    = PZ_TEXT_STYLE_DEFAULT(g_app.font_russo, 28.0f);
                restart_style.align_h = PZ_FONT_ALIGN_CENTER;
                restart_style.align_v = PZ_FONT_ALIGN_CENTER;
                restart_style.color = pz_vec4_new(0.9f, 0.9f, 0.9f, 1.0f);
                restart_style.outline_width = 2.0f;
                restart_style.outline_color
                    = pz_vec4_new(0.0f, 0.0f, 0.0f, 1.0f);

                pz_font_draw(g_app.font_mgr, &restart_style, vp_width * 0.5f,
                    vp_height * 0.55f, "Press R to restart");
            }
        }

        pz_font_end_frame(g_app.font_mgr);
    }

    bool should_quit = false;
    g_app.frame_count++;
    if (g_app.auto_screenshot && g_app.frame_count >= g_app.screenshot_frames) {
        pz_renderer_save_screenshot(g_app.renderer, g_app.screenshot_path);
        should_quit = true;
    }

    if (g_app.lightmap_debug_path
        && g_app.frame_count >= g_app.screenshot_frames) {
        pz_lighting_save_debug(g_app.lighting, g_app.lightmap_debug_path);
        g_app.lightmap_debug_path = NULL;
    }

    pz_renderer_end_frame(g_app.renderer);

    if (should_quit) {
        sapp_quit();
    }

    g_app.mouse_left_just_pressed = false;
    g_app.key_f_just_pressed = false;
}

static void
app_event(const sapp_event *event)
{
    if (!event)
        return;

    switch (event->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (event->key_code >= 0 && event->key_code < SAPP_KEYCODE_COUNT) {
            g_app.key_down[event->key_code] = true;
        }
        if (!event->key_repeat) {
            if (event->key_code == SAPP_KEYCODE_ESCAPE) {
                sapp_quit();
            } else if (event->key_code == SAPP_KEYCODE_F2) {
                pz_debug_overlay_toggle(g_app.debug_overlay);
            } else if (event->key_code == SAPP_KEYCODE_F11) {
                if (g_app.lighting) {
                    pz_lighting_save_debug(
                        g_app.lighting, "screenshots/lightmap_debug.png");
                }
            } else if (event->key_code == SAPP_KEYCODE_F12) {
                char *path = generate_screenshot_path();
                if (path) {
                    pz_renderer_save_screenshot(g_app.renderer, path);
                    pz_free(path);
                }
            } else if (event->key_code == SAPP_KEYCODE_F) {
                g_app.key_f_just_pressed = true;
            } else if (event->key_code == SAPP_KEYCODE_R) {
                // Restart game on R key (only in victory state or when dead)
                if (g_app.state == GAME_STATE_VICTORY) {
                    // Reset game state
                    g_app.state = GAME_STATE_PLAYING;
                    g_app.victory_timer = 0.0f;

                    // Respawn player and reset loadout
                    if (g_app.player_tank) {
                        pz_tank_respawn(g_app.player_tank);
                        pz_tank_reset_loadout(g_app.player_tank);
                    }

                    // Re-spawn all enemies from map
                    if (g_app.game_map && g_app.ai_mgr) {
                        // Clear existing AI controllers
                        g_app.ai_mgr->controller_count = 0;

                        // Remove all non-player tanks
                        for (int i = 0; i < PZ_MAX_TANKS; i++) {
                            pz_tank *tank = &g_app.tank_mgr->tanks[i];
                            if ((tank->flags & PZ_TANK_FLAG_ACTIVE)
                                && !(tank->flags & PZ_TANK_FLAG_PLAYER)) {
                                tank->flags = 0; // Deactivate
                                g_app.tank_mgr->tank_count--;
                            }
                        }

                        // Spawn enemies again
                        int enemy_count
                            = pz_map_get_enemy_count(g_app.game_map);
                        for (int i = 0; i < enemy_count; i++) {
                            const pz_enemy_spawn *es
                                = pz_map_get_enemy(g_app.game_map, i);
                            if (es) {
                                pz_ai_spawn_enemy(g_app.ai_mgr, es->pos,
                                    es->angle, (pz_enemy_level)es->level);
                            }
                        }
                    }

                    // Clear projectiles and particles
                    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
                        g_app.projectile_mgr->projectiles[i].active = false;
                    }
                    g_app.projectile_mgr->active_count = 0;

                    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Game restarted");
                }
            }
        }
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        if (event->key_code >= 0 && event->key_code < SAPP_KEYCODE_COUNT) {
            g_app.key_down[event->key_code] = false;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        g_app.mouse_x = event->mouse_x;
        g_app.mouse_y = event->mouse_y;
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        if (event->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g_app.mouse_left_down = true;
            g_app.mouse_left_just_pressed = true;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        if (event->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g_app.mouse_left_down = false;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        g_app.scroll_accumulator += event->scroll_y;
        break;
    case SAPP_EVENTTYPE_RESIZED: {
        int width = sapp_width();
        int height = sapp_height();
        pz_renderer_set_viewport(g_app.renderer, width, height);
        pz_camera_set_viewport(&g_app.camera, width, height);
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE, "Window resized: %dx%d", width,
            height);
    } break;
    default:
        break;
    }
}

static void
app_cleanup(void)
{
    pz_font_manager_destroy(g_app.font_mgr);
    pz_debug_overlay_destroy(g_app.debug_overlay);
    pz_debug_cmd_shutdown();

    if (g_app.laser_vb != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(g_app.renderer, g_app.laser_vb);
    }
    if (g_app.laser_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(g_app.renderer, g_app.laser_pipeline);
    }
    if (g_app.laser_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(g_app.renderer, g_app.laser_shader);
    }

    pz_sim_destroy(g_app.sim);
    pz_ai_manager_destroy(g_app.ai_mgr);
    pz_tank_manager_destroy(g_app.tank_mgr, g_app.renderer);
    pz_projectile_manager_destroy(g_app.projectile_mgr, g_app.renderer);
    pz_particle_manager_destroy(g_app.particle_mgr, g_app.renderer);
    pz_powerup_manager_destroy(g_app.powerup_mgr, g_app.renderer);

    pz_tracks_destroy(g_app.tracks);
    pz_lighting_destroy(g_app.lighting);
    pz_map_hot_reload_destroy(g_app.map_hot_reload);
    pz_map_renderer_destroy(g_app.map_renderer);
    pz_map_destroy(g_app.game_map);

    pz_texture_manager_destroy(g_app.tex_manager);
    pz_renderer_destroy(g_app.renderer);

    pz_log_shutdown();
    pz_mem_dump_leaks();

    printf("Tank Game - Exiting.\n");
}

sapp_desc
sokol_main(int argc, char *argv[])
{
    parse_args(argc, argv);

    return (sapp_desc) {
        .init_cb = app_init,
        .frame_cb = app_frame,
        .cleanup_cb = app_cleanup,
        .event_cb = app_event,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
        .sample_count = 4,
        .high_dpi = true,
        .window_title = WINDOW_TITLE,
    };
}
