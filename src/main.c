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
#include "engine/pz_audio.h"
#include "engine/pz_camera.h"
#include "engine/pz_debug_overlay.h"
#include "engine/pz_font.h"
#include "engine/pz_music.h"
#include "engine/render/pz_renderer.h"
#include "engine/render/pz_texture.h"
#include "game/pz_ai.h"
#include "game/pz_background.h"
#include "game/pz_barrier.h"
#include "game/pz_campaign.h"
#include "game/pz_game_music.h"
#include "game/pz_game_sfx.h"
#include "game/pz_lighting.h"
#include "game/pz_map.h"
#include "game/pz_map_render.h"
#include "game/pz_mesh.h"
#include "game/pz_particle.h"
#include "game/pz_powerup.h"
#include "game/pz_projectile.h"
#include "game/pz_tank.h"
#include "game/pz_tile_registry.h"
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

static float
track_strength_for_tank(const pz_tank *tank)
{
    float recoil = pz_clampf(tank->recoil, 0.0f, 1.5f);
    return 1.0f + recoil * 0.35f;
}

static void
spawn_tank_fog(
    pz_particle_manager *particle_mgr, pz_tank_manager *tank_mgr, float dt)
{
    if (!particle_mgr || !tank_mgr)
        return;

    float max_speed = tank_mgr->max_speed > 0.0f ? tank_mgr->max_speed : 1.0f;

    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &tank_mgr->tanks[i];
        if (!(tank->flags & PZ_TANK_FLAG_ACTIVE)
            || (tank->flags & PZ_TANK_FLAG_DEAD)) {
            continue;
        }

        float speed = pz_vec2_len(tank->vel);
        if (speed < 0.15f) {
            tank->idle_time = pz_minf(tank->idle_time + dt, 3.0f);
        } else {
            tank->idle_time = 0.0f;
        }

        float idle_factor = pz_clampf(tank->idle_time / 2.0f, 0.0f, 1.0f);
        float moving_factor
            = pz_clampf(speed / (max_speed * 0.75f), 0.0f, 1.0f);

        float spawn_interval = pz_lerpf(0.25f, 0.08f, moving_factor);
        if (moving_factor < 0.1f) {
            // Idle tanks produce ~30% of the smoke (longer interval)
            spawn_interval = pz_lerpf(0.25f, 0.85f, idle_factor);
        }

        tank->fog_timer -= dt;
        if (tank->fog_timer <= 0.0f) {
            pz_vec2 forward
                = { sinf(tank->body_angle), cosf(tank->body_angle) };
            // Spawn in back half of tank (tank is ~2.5 units long)
            float trail_offset = pz_lerpf(0.9f, 1.25f, moving_factor);
            pz_vec3 fog_pos = { tank->pos.x - forward.x * trail_offset, 0.35f,
                tank->pos.y - forward.y * trail_offset };

            pz_particle_spawn_fog(particle_mgr, fog_pos, idle_factor);
            tank->fog_timer = spawn_interval;
        }
    }
}

static void
spawn_projectile_fog(pz_particle_manager *particle_mgr,
    pz_projectile_manager *projectile_mgr, float dt)
{
    if (!particle_mgr || !projectile_mgr)
        return;

    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
        pz_projectile *proj = &projectile_mgr->projectiles[i];
        if (!proj->active)
            continue;

        float speed = pz_vec2_len(proj->velocity);
        float speed_factor = pz_clampf(speed / 12.0f, 0.0f, 1.0f);
        float spawn_interval = pz_lerpf(0.07f, 0.025f, speed_factor);

        proj->fog_timer -= dt;
        if (proj->fog_timer <= 0.0f) {
            pz_vec2 forward = { 0.0f, 1.0f };
            if (speed > 0.001f) {
                forward = pz_vec2_scale(proj->velocity, 1.0f / speed);
            }

            float trail_offset = pz_lerpf(0.12f, 0.18f, speed_factor);
            pz_vec3 fog_pos = { proj->pos.x - forward.x * trail_offset, 0.85f,
                proj->pos.y - forward.y * trail_offset };

            pz_particle_spawn_bullet_fog(particle_mgr, fog_pos);
            proj->fog_timer = spawn_interval;
        }
    }
}

typedef struct {
    pz_vec2 pos;
    float timer; // Remaining time
    float duration; // Total duration
    bool is_tank; // Tank explosion vs bullet impact
} explosion_light;

#define MAX_EXPLOSION_LIGHTS 16
#define MAX_FOG_MARKS 128
#define FOG_MARK_LIFETIME 3.0f
#define FOG_MARK_TANK_MIN_DIST 0.6f
#define FOG_MARK_PROJ_MIN_DIST 0.4f

typedef struct fog_mark {
    bool active;
    pz_vec2 pos;
    float timer;
    float duration;
    float radius;
    float strength;
} fog_mark;

// Game states
typedef enum {
    GAME_STATE_PLAYING,
    GAME_STATE_PLAYER_DEAD, // Player died, waiting for respawn
    GAME_STATE_LEVEL_COMPLETE, // All enemies defeated, waiting for transition
    GAME_STATE_GAME_OVER, // No lives left
    GAME_STATE_CAMPAIGN_COMPLETE, // All levels done
} game_state;

// Map session - all state that needs to be reset when loading a new map
// This struct helps ensure we don't leak state between map transitions
typedef struct map_session {
    // Map data
    pz_map *map;
    pz_map_renderer *renderer;
    pz_map_hot_reload *hot_reload;
    char map_path[256]; // Current map path for hot-reload

    // Map-specific rendering
    pz_tracks *tracks;
    pz_lighting *lighting;

    // Entities (all cleared on map change)
    pz_tank_manager *tank_mgr;
    pz_tank *player_tank; // Convenience pointer into tank_mgr
    pz_ai_manager *ai_mgr;
    pz_projectile_manager *projectile_mgr;
    pz_particle_manager *particle_mgr;
    pz_powerup_manager *powerup_mgr;
    pz_barrier_manager *barrier_mgr;

    // Map gameplay state
    int initial_enemy_count;
    explosion_light explosion_lights[MAX_EXPLOSION_LIGHTS];

    // Fog disturbance trail
    fog_mark fog_marks[MAX_FOG_MARKS];
    int fog_mark_count;
    pz_vec2 fog_last_tank_pos[PZ_MAX_TANKS];
    bool fog_has_tank_pos[PZ_MAX_TANKS];
    pz_vec2 fog_last_projectile_pos[PZ_MAX_PROJECTILES];
    bool fog_has_projectile_pos[PZ_MAX_PROJECTILES];
} map_session;

typedef struct app_state {
    // Command line args
    bool auto_screenshot;
    const char *screenshot_path;
    int screenshot_frames;
    const char *lightmap_debug_path;
    const char *map_path_arg;
    const char *campaign_path_arg;
    bool show_debug_overlay;
    bool show_debug_texture_scale;

    // Core systems (persistent across maps)
    pz_renderer *renderer;
    pz_texture_manager *tex_manager;
    pz_tile_registry *tile_registry;
    pz_camera camera;
    pz_debug_overlay *debug_overlay;
    pz_font_manager *font_mgr;
    pz_font *font_russo;
    pz_font *font_caveat;
    pz_sim *sim;
    pz_audio *audio;
    pz_game_music *game_music;
    pz_game_sfx *game_sfx;

    // Laser rendering (persistent)
    pz_shader_handle laser_shader;
    pz_pipeline_handle laser_pipeline;
    pz_buffer_handle laser_vb;

    // Background rendering (persistent, configured per-map)
    pz_background *background;

    // Campaign system
    pz_campaign_manager *campaign_mgr;

    // Current map session (all map-dependent state)
    map_session session;

    // Game state
    game_state state;
    float state_timer; // Timer for state transitions

    // Frame timing
    int frame_count;
    double last_hot_reload_check;
    double last_frame_time;
    float total_time; // Cumulative time for animations

    // Input state
    float mouse_x;
    float mouse_y;
    bool mouse_left_down;
    bool mouse_left_just_pressed;
    bool space_down;
    bool space_just_pressed;
    float scroll_accumulator;
    bool key_f_just_pressed;
    bool key_down[SAPP_KEYCODE_COUNT];
} app_state;

static app_state g_app;

static const float LASER_WIDTH = 0.08f;
static const float LASER_MAX_DIST = 50.0f;

// Forward declarations
static void map_session_unload(map_session *session);
static bool map_session_load(map_session *session, const char *map_path);
static void fog_marks_clear(map_session *session);
static void audio_callback(
    float *buffer, int num_frames, int num_channels, void *userdata);

// ============================================================================
// Map Session Management
// ============================================================================

// Unload all map-dependent state
static void
map_session_unload(map_session *session)
{
    if (!session) {
        return;
    }

    // Destroy entity managers
    pz_ai_manager_destroy(session->ai_mgr);
    session->ai_mgr = NULL;

    pz_powerup_manager_destroy(session->powerup_mgr, g_app.renderer);
    session->powerup_mgr = NULL;

    pz_barrier_manager_destroy(session->barrier_mgr, g_app.renderer);
    session->barrier_mgr = NULL;

    pz_particle_manager_destroy(session->particle_mgr, g_app.renderer);
    session->particle_mgr = NULL;

    pz_projectile_manager_destroy(session->projectile_mgr, g_app.renderer);
    session->projectile_mgr = NULL;

    pz_tank_manager_destroy(session->tank_mgr, g_app.renderer);
    session->tank_mgr = NULL;
    session->player_tank = NULL;

    // Destroy map rendering
    pz_lighting_destroy(session->lighting);
    session->lighting = NULL;

    pz_tracks_destroy(session->tracks);
    session->tracks = NULL;

    pz_map_hot_reload_destroy(session->hot_reload);
    session->hot_reload = NULL;

    pz_map_renderer_destroy(session->renderer);
    session->renderer = NULL;

    pz_map_destroy(session->map);
    session->map = NULL;

    // Clear remaining state
    session->map_path[0] = '\0';
    session->initial_enemy_count = 0;
    memset(session->explosion_lights, 0, sizeof(session->explosion_lights));

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Map session unloaded");
}

// Load a new map and set up all map-dependent state
static bool
map_session_load(map_session *session, const char *map_path)
{
    if (!session || !map_path) {
        return false;
    }

    // Unload any existing session first
    map_session_unload(session);

    // Store path for hot-reload
    strncpy(session->map_path, map_path, sizeof(session->map_path) - 1);
    session->map_path[sizeof(session->map_path) - 1] = '\0';

    // Load map
    session->map = pz_map_load(map_path);
    if (!session->map) {
        pz_log(
            PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to load map: %s", map_path);
        return false;
    }

    // Fit camera to map
    pz_camera_fit_map(&g_app.camera, session->map->world_width,
        session->map->world_height, 20.0f);

    // Configure background from map settings
    if (g_app.background) {
        pz_background_set_from_map(g_app.background, session->map);
    }

    if (g_app.game_music) {
        if (session->map->has_music) {
            pz_game_music_load(g_app.game_music, session->map->music_name);
        } else {
            pz_game_music_stop(g_app.game_music);
        }
    }

    // Set tile registry on map for property lookups
    pz_map_set_tile_registry(session->map, g_app.tile_registry);

    // Create map renderer with tile registry
    session->renderer = pz_map_renderer_create(
        g_app.renderer, g_app.tex_manager, g_app.tile_registry);
    if (session->renderer) {
        pz_map_renderer_set_map(session->renderer, session->map);

        // Apply debug texture scale if requested via command line
        if (g_app.show_debug_texture_scale) {
            pz_map_renderer_set_debug_texture_scale(session->renderer, true);
        }
    }

    // Set up hot-reload
    session->hot_reload
        = pz_map_hot_reload_create(map_path, &session->map, session->renderer);

    // Create tracks system
    pz_tracks_config track_config = {
        .world_width = session->map->world_width,
        .world_height = session->map->world_height,
        .texture_size = 1024,
    };
    session->tracks
        = pz_tracks_create(g_app.renderer, g_app.tex_manager, &track_config);

    // Create lighting system
    const pz_map_lighting *map_light = pz_map_get_lighting(session->map);
    pz_lighting_config light_config = {
        .world_width = session->map->world_width,
        .world_height = session->map->world_height,
        .texture_size = 512,
        .ambient = map_light->ambient_color,
    };
    session->lighting = pz_lighting_create(g_app.renderer, &light_config);

    // Create entity managers
    session->tank_mgr = pz_tank_manager_create(g_app.renderer, NULL);
    session->projectile_mgr = pz_projectile_manager_create(g_app.renderer);
    session->particle_mgr = pz_particle_manager_create(g_app.renderer);
    session->powerup_mgr = pz_powerup_manager_create(g_app.renderer);
    session->barrier_mgr = pz_barrier_manager_create(
        g_app.renderer, g_app.tile_registry, session->map->tile_size);

    // Spawn player at first spawn point
    pz_vec2 player_spawn_pos = { 0.0f, 0.0f };
    if (pz_map_get_spawn_count(session->map) > 0) {
        const pz_spawn_point *sp = pz_map_get_spawn(session->map, 0);
        if (sp) {
            player_spawn_pos = sp->pos;
        }
    }
    session->player_tank = pz_tank_spawn(session->tank_mgr, player_spawn_pos,
        (pz_vec4) { 0.2f, 0.4f, 0.9f, 1.0f }, true);

    // Create AI manager and spawn enemies
    session->ai_mgr = pz_ai_manager_create(session->tank_mgr, session->map);
    int enemy_count = pz_map_get_enemy_count(session->map);
    for (int i = 0; i < enemy_count; i++) {
        const pz_enemy_spawn *es = pz_map_get_enemy(session->map, i);
        if (es) {
            pz_ai_spawn_enemy(
                session->ai_mgr, es->pos, es->angle, (pz_enemy_level)es->level);
        }
    }
    session->initial_enemy_count = enemy_count;

    // Spawn powerups from map data
    int powerup_count = pz_map_get_powerup_count(session->map);
    for (int i = 0; i < powerup_count; i++) {
        const pz_powerup_spawn *ps = pz_map_get_powerup(session->map, i);
        if (ps) {
            pz_powerup_type type = pz_powerup_type_from_name(ps->type_name);
            if (type != PZ_POWERUP_NONE) {
                pz_powerup_add(
                    session->powerup_mgr, ps->pos, type, ps->respawn_time);
            } else {
                pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "Unknown powerup type: %s",
                    ps->type_name);
            }
        }
    }

    // Spawn barriers from map data
    int barrier_count = pz_map_get_barrier_count(session->map);
    for (int i = 0; i < barrier_count; i++) {
        const pz_barrier_spawn *bs = pz_map_get_barrier(session->map, i);
        if (bs) {
            pz_barrier_add(
                session->barrier_mgr, bs->pos, bs->tile_name, bs->health);
        }
    }

    // Clear explosion lights
    memset(session->explosion_lights, 0, sizeof(session->explosion_lights));
    fog_marks_clear(session);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Map session loaded: %s (%d enemies)",
        map_path, enemy_count);

    return true;
}

// Reset the current map (respawn enemies, reset player position)
static void
map_session_reset(map_session *session)
{
    if (!session || !session->map) {
        return;
    }

    // Clear projectiles
    if (session->projectile_mgr) {
        for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
            session->projectile_mgr->projectiles[i].active = false;
        }
        session->projectile_mgr->active_count = 0;
    }

    // Clear particles
    if (session->particle_mgr) {
        pz_particle_clear(session->particle_mgr);
    }

    // Reset player
    if (session->player_tank) {
        pz_tank_respawn(session->player_tank);
        pz_tank_reset_loadout(session->player_tank);
    }

    // Clear and respawn enemies
    if (session->ai_mgr && session->tank_mgr) {
        // Clear AI controllers
        session->ai_mgr->controller_count = 0;

        // Remove all non-player tanks
        for (int i = 0; i < PZ_MAX_TANKS; i++) {
            pz_tank *tank = &session->tank_mgr->tanks[i];
            if ((tank->flags & PZ_TANK_FLAG_ACTIVE)
                && !(tank->flags & PZ_TANK_FLAG_PLAYER)) {
                tank->flags = 0;
                session->tank_mgr->tank_count--;
            }
        }

        // Respawn enemies from map
        int enemy_count = pz_map_get_enemy_count(session->map);
        for (int i = 0; i < enemy_count; i++) {
            const pz_enemy_spawn *es = pz_map_get_enemy(session->map, i);
            if (es) {
                pz_ai_spawn_enemy(session->ai_mgr, es->pos, es->angle,
                    (pz_enemy_level)es->level);
            }
        }
    }

    // Reset powerups (clear and respawn from map)
    if (session->powerup_mgr) {
        // Clear all powerups
        for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
            session->powerup_mgr->powerups[i].active = false;
        }
        session->powerup_mgr->active_count = 0;

        // Respawn from map
        int powerup_count = pz_map_get_powerup_count(session->map);
        for (int i = 0; i < powerup_count; i++) {
            const pz_powerup_spawn *ps = pz_map_get_powerup(session->map, i);
            if (ps) {
                pz_powerup_type type = pz_powerup_type_from_name(ps->type_name);
                if (type != PZ_POWERUP_NONE) {
                    pz_powerup_add(
                        session->powerup_mgr, ps->pos, type, ps->respawn_time);
                }
            }
        }
    }

    // Reset barriers (clear and respawn from map)
    if (session->barrier_mgr) {
        pz_barrier_clear(session->barrier_mgr);

        // Respawn from map
        int barrier_count = pz_map_get_barrier_count(session->map);
        for (int i = 0; i < barrier_count; i++) {
            const pz_barrier_spawn *bs = pz_map_get_barrier(session->map, i);
            if (bs) {
                pz_barrier_add(
                    session->barrier_mgr, bs->pos, bs->tile_name, bs->health);
            }
        }
    }

    // Clear explosion lights
    memset(session->explosion_lights, 0, sizeof(session->explosion_lights));

    // Clear tracks
    if (session->tracks) {
        pz_tracks_clear(session->tracks);
    }

    fog_marks_clear(session);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Map session reset");
}

static void
fog_marks_clear(map_session *session)
{
    if (!session) {
        return;
    }

    for (int i = 0; i < MAX_FOG_MARKS; i++) {
        session->fog_marks[i].active = false;
    }
    session->fog_mark_count = 0;

    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        session->fog_has_tank_pos[i] = false;
    }
    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
        session->fog_has_projectile_pos[i] = false;
    }
}

static void
fog_marks_update(map_session *session, float dt)
{
    if (!session || session->fog_mark_count == 0) {
        return;
    }

    int active_count = 0;
    for (int i = 0; i < MAX_FOG_MARKS; i++) {
        fog_mark *mark = &session->fog_marks[i];
        if (!mark->active) {
            continue;
        }

        mark->timer -= dt;
        if (mark->timer <= 0.0f) {
            mark->active = false;
            continue;
        }

        active_count++;
    }

    session->fog_mark_count = active_count;
}

static void
fog_marks_add(map_session *session, pz_vec2 pos, float radius, float strength)
{
    if (!session) {
        return;
    }

    int slot = -1;
    for (int i = 0; i < MAX_FOG_MARKS; i++) {
        if (!session->fog_marks[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        float lowest_timer = 9999.0f;
        for (int i = 0; i < MAX_FOG_MARKS; i++) {
            if (session->fog_marks[i].timer < lowest_timer) {
                lowest_timer = session->fog_marks[i].timer;
                slot = i;
            }
        }
    }

    if (slot < 0) {
        return;
    }

    fog_mark *mark = &session->fog_marks[slot];
    if (!mark->active) {
        session->fog_mark_count++;
    }

    mark->active = true;
    mark->pos = pos;
    mark->timer = FOG_MARK_LIFETIME;
    mark->duration = FOG_MARK_LIFETIME;
    mark->radius = radius;
    mark->strength = strength;
}

static void
fog_marks_emit(map_session *session)
{
    if (!session || !session->map || !session->map->has_fog) {
        return;
    }

    if (session->map->fog_level != 0 && session->map->fog_level != 1) {
        return;
    }

    if (session->tank_mgr) {
        for (int i = 0; i < PZ_MAX_TANKS; i++) {
            pz_tank *tank = &session->tank_mgr->tanks[i];
            if (!(tank->flags & PZ_TANK_FLAG_ACTIVE)
                || (tank->flags & PZ_TANK_FLAG_DEAD)) {
                session->fog_has_tank_pos[i] = false;
                continue;
            }
            if (pz_vec2_len(tank->vel) < 0.15f) {
                continue;
            }

            pz_vec2 pos = tank->pos;
            if (!session->fog_has_tank_pos[i]
                || pz_vec2_len(pz_vec2_sub(pos, session->fog_last_tank_pos[i]))
                    >= FOG_MARK_TANK_MIN_DIST) {
                fog_marks_add(session, pos, 2.4f, 1.0f);
                session->fog_last_tank_pos[i] = pos;
                session->fog_has_tank_pos[i] = true;
            }
        }
    }

    if (session->projectile_mgr) {
        for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
            pz_projectile *proj = &session->projectile_mgr->projectiles[i];
            if (!proj->active) {
                session->fog_has_projectile_pos[i] = false;
                continue;
            }

            pz_vec2 pos = proj->pos;
            if (!session->fog_has_projectile_pos[i]
                || pz_vec2_len(
                       pz_vec2_sub(pos, session->fog_last_projectile_pos[i]))
                    >= FOG_MARK_PROJ_MIN_DIST) {
                fog_marks_add(session, pos, 1.3f, 0.85f);
                session->fog_last_projectile_pos[i] = pos;
                session->fog_has_projectile_pos[i] = true;
            }
        }
    }
}

// ============================================================================
// Argument Parsing
// ============================================================================

static void
parse_args(int argc, char *argv[])
{
    g_app.auto_screenshot = false;
    g_app.screenshot_path = NULL;
    g_app.screenshot_frames = 1;
    g_app.lightmap_debug_path = NULL;
    g_app.map_path_arg = NULL;
    g_app.campaign_path_arg = NULL;
    g_app.show_debug_overlay = false;

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
        } else if (strcmp(argv[i], "--campaign") == 0 && i + 1 < argc) {
            g_app.campaign_path_arg = argv[++i];
        } else if (strcmp(argv[i], "--debug") == 0) {
            g_app.show_debug_overlay = true;
        } else if (strcmp(argv[i], "--debug-texture-scale") == 0) {
            g_app.show_debug_texture_scale = true;
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

    g_app.audio = pz_audio_init();
    if (g_app.audio) {
        int sample_rate = pz_audio_get_sample_rate(g_app.audio);
        g_app.game_music = pz_game_music_create("assets/sounds/soundfont.sf2");
        g_app.game_sfx = pz_game_sfx_create(sample_rate);

        if (g_app.game_music || g_app.game_sfx) {
            pz_audio_set_callback(g_app.audio, audio_callback, NULL);
        } else {
            pz_audio_shutdown(g_app.audio);
            g_app.audio = NULL;
        }
    } else {
        g_app.game_music = NULL;
        g_app.game_sfx = NULL;
    }

    // Initialize core systems (persistent across maps)
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

    // Create and load tile registry
    g_app.tile_registry = pz_tile_registry_create();
    if (g_app.tile_registry) {
        int tiles_loaded = pz_tile_registry_load_all(
            g_app.tile_registry, g_app.tex_manager, "assets/tiles");
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
            "Tile registry initialized with %d tiles", tiles_loaded);
    }

    pz_camera_init(&g_app.camera, width, height);

    pz_debug_cmd_init(NULL);

    g_app.debug_overlay = pz_debug_overlay_create(g_app.renderer);
    if (!g_app.debug_overlay) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE, "Failed to create debug overlay");
    } else if (g_app.show_debug_overlay) {
        pz_debug_overlay_set_visible(g_app.debug_overlay, true);
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

    // Create laser rendering resources (persistent)
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

    // Create background renderer (persistent, configured per-map)
    g_app.background = pz_background_create(g_app.renderer);
    if (!g_app.background) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE,
            "Failed to create background renderer");
    }

    // Initialize simulation system
    g_app.sim = pz_sim_create((uint32_t)time(NULL));

    // Load campaign or single map
    g_app.campaign_mgr = pz_campaign_create();

    const char *first_map_path = NULL;

    if (g_app.map_path_arg) {
        // Single map mode (--map flag)
        first_map_path = g_app.map_path_arg;
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Single map mode: %s",
            first_map_path);
    } else {
        // Campaign mode
        const char *campaign_path = g_app.campaign_path_arg
            ? g_app.campaign_path_arg
            : "assets/campaigns/main.campaign";

        if (pz_campaign_load(g_app.campaign_mgr, campaign_path)) {
            pz_campaign_start(g_app.campaign_mgr, 0); // Use campaign's lives
            first_map_path = pz_campaign_get_current_map(g_app.campaign_mgr);
        } else {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
                "Failed to load campaign, falling back to default map");
            first_map_path = "assets/maps/night_arena.map";
        }
    }

    // Load the first map
    if (!map_session_load(&g_app.session, first_map_path)) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
            "Failed to load initial map, exiting");
        sapp_quit();
        return;
    }

    // Initialize game state
    g_app.state = GAME_STATE_PLAYING;
    g_app.state_timer = 0.0f;

    // Frame timing
    g_app.frame_count = 0;
    g_app.last_hot_reload_check = pz_time_now();
    g_app.last_frame_time = pz_time_now();

    // Input state
    g_app.mouse_x = (float)width * 0.5f;
    g_app.mouse_y = (float)height * 0.5f;
    g_app.mouse_left_down = false;
    g_app.mouse_left_just_pressed = false;
    g_app.space_down = false;
    g_app.space_just_pressed = false;
    g_app.scroll_accumulator = 0.0f;
    g_app.key_f_just_pressed = false;
}

// Render music debug overlay (called when debug overlay is visible)
static void
render_music_debug_overlay(void)
{
    if (!pz_debug_overlay_is_visible(g_app.debug_overlay)) {
        return;
    }
    if (!g_app.game_music) {
        return;
    }

    pz_game_music_debug_info info;
    if (!pz_game_music_get_debug_info(g_app.game_music, &info)) {
        return;
    }

    // Position music debug panel on the right side of the screen
    // Font is now 16x16 (2x scaled from 8x8)
    int fb_width, fb_height;
    pz_renderer_get_viewport(g_app.renderer, &fb_width, &fb_height);
    int panel_x = fb_width - 380; // Wider panel for larger text
    int panel_y = 16;
    int line_height = 20; // 16px font + 4px spacing
    int y = panel_y;

    pz_vec4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    pz_vec4 green = { 0.3f, 1.0f, 0.3f, 1.0f };
    pz_vec4 yellow = { 1.0f, 1.0f, 0.3f, 1.0f };
    pz_vec4 red = { 1.0f, 0.3f, 0.3f, 1.0f };
    pz_vec4 cyan = { 0.3f, 1.0f, 1.0f, 1.0f };
    pz_vec4 gray = { 0.6f, 0.6f, 0.6f, 1.0f };
    (void)red;

    // Header
    pz_debug_overlay_text_color(
        g_app.debug_overlay, panel_x, y, cyan, "-- Music Debug --");
    y += line_height + 4;

    // State
    const char *state_str
        = info.is_victory ? "VICTORY" : (info.playing ? "PLAYING" : "STOPPED");
    pz_vec4 state_color = info.playing ? green : gray;
    pz_debug_overlay_text_color(
        g_app.debug_overlay, panel_x, y, state_color, "State: %s", state_str);
    y += line_height;

    // BPM and timing
    pz_debug_overlay_text_color(
        g_app.debug_overlay, panel_x, y, white, "BPM: %.1f", info.bpm);
    y += line_height;

    // Time with beat indicator
    double beat_duration_ms = 60000.0 / info.bpm;
    double beat_progress = info.beat_pos / beat_duration_ms;
    int beat_num = (int)(info.time_ms / beat_duration_ms) % 4 + 1;
    pz_vec4 beat_color = (beat_progress < 0.1) ? yellow : white;
    pz_debug_overlay_text_color(g_app.debug_overlay, panel_x, y, beat_color,
        "Time: %.1fs [%d]", info.time_ms / 1000.0, beat_num);
    y += line_height;

    // Loop length
    pz_debug_overlay_text_color(g_app.debug_overlay, panel_x, y, white,
        "Loop: %.1fs", info.loop_length_ms / 1000.0);
    y += line_height;

    // Master volume
    pz_debug_overlay_text_color(g_app.debug_overlay, panel_x, y, white,
        "Volume: %.0f%%", info.master_volume * 100.0f);
    y += line_height + 4;

    // Intensity layers
    pz_debug_overlay_text_color(
        g_app.debug_overlay, panel_x, y, cyan, "Intensity:");
    y += line_height;

    pz_vec4 i1_color = info.intensity1_active
        ? green
        : (info.intensity1_pending ? yellow : gray);
    const char *i1_status = info.intensity1_active
        ? "ON"
        : (info.intensity1_pending ? "PENDING" : "OFF");
    pz_debug_overlay_text_color(
        g_app.debug_overlay, panel_x, y, i1_color, "  I1: %s", i1_status);
    y += line_height;

    pz_vec4 i2_color = info.intensity2_active
        ? green
        : (info.intensity2_pending ? yellow : gray);
    const char *i2_status = info.intensity2_active
        ? "ON"
        : (info.intensity2_pending ? "PENDING" : "OFF");
    pz_debug_overlay_text_color(
        g_app.debug_overlay, panel_x, y, i2_color, "  I2: %s", i2_status);
    y += line_height + 4;

    // Layer details
    pz_debug_overlay_text_color(g_app.debug_overlay, panel_x, y, cyan,
        "Layers (%d):", info.layer_count);
    y += line_height;

    for (int i = 0; i < info.layer_count && i < 6; i++) {
        pz_music_layer_info layer_info;
        if (pz_game_music_get_layer_info(g_app.game_music, i, &layer_info)) {
            pz_vec4 layer_color = layer_info.active ? green : gray;
            char status
                = layer_info.enabled ? (layer_info.active ? '+' : '~') : '-';
            pz_debug_overlay_text_color(g_app.debug_overlay, panel_x, y,
                layer_color, "[%c] ch%d v%.0f%%", status,
                layer_info.midi_channel, layer_info.volume * 100.0f);
            y += line_height;
        }
    }
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
    g_app.total_time += frame_dt;

    // Determine number of simulation ticks to run this frame
    int sim_ticks = pz_sim_accumulate(g_app.sim, frame_dt);
    float dt = pz_sim_dt(); // Fixed timestep for simulation

    // Gather input (once per frame)
    pz_tank_input player_input = { 0 };
    if (g_app.key_down[SAPP_KEYCODE_W] || g_app.key_down[SAPP_KEYCODE_UP])
        player_input.move_dir.y -= 1.0f;
    if (g_app.key_down[SAPP_KEYCODE_S] || g_app.key_down[SAPP_KEYCODE_DOWN])
        player_input.move_dir.y += 1.0f;
    if (g_app.key_down[SAPP_KEYCODE_A] || g_app.key_down[SAPP_KEYCODE_LEFT])
        player_input.move_dir.x -= 1.0f;
    if (g_app.key_down[SAPP_KEYCODE_D] || g_app.key_down[SAPP_KEYCODE_RIGHT])
        player_input.move_dir.x += 1.0f;

    if (g_app.session.player_tank
        && !(g_app.session.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
        pz_vec3 mouse_world = pz_camera_screen_to_world(
            &g_app.camera, (int)g_app.mouse_x, (int)g_app.mouse_y);
        float aim_dx = mouse_world.x - g_app.session.player_tank->pos.x;
        float aim_dz = mouse_world.z - g_app.session.player_tank->pos.y;
        player_input.target_turret = atan2f(aim_dx, aim_dz);
        player_input.fire = g_app.mouse_left_down || g_app.space_down;
    }

    // Handle weapon cycling (once per frame, not per sim tick)
    if (g_app.session.player_tank
        && !(g_app.session.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
        if (g_app.scroll_accumulator >= 3.0f) {
            pz_tank_cycle_weapon(g_app.session.player_tank, 1);
            g_app.scroll_accumulator = 0.0f;
        } else if (g_app.scroll_accumulator <= -3.0f) {
            pz_tank_cycle_weapon(g_app.session.player_tank, -1);
            g_app.scroll_accumulator = 0.0f;
        }
        if (g_app.key_f_just_pressed) {
            pz_tank_cycle_weapon(g_app.session.player_tank, 1);
        }
    }

    // =========================================================================
    // FIXED TIMESTEP SIMULATION LOOP
    // Run N simulation ticks at fixed dt for deterministic gameplay
    // =========================================================================
    for (int tick = 0; tick < sim_ticks; tick++) {
        pz_sim_begin_tick(g_app.sim);

        // Player tank update
        if (g_app.session.player_tank
            && !(g_app.session.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
            pz_tank_update(g_app.session.tank_mgr, g_app.session.player_tank,
                &player_input, g_app.session.map, dt);

            // Track marks for player
            if (g_app.session.tracks
                && pz_vec2_len(g_app.session.player_tank->vel) > 0.1f) {
                pz_tracks_add_mark(g_app.session.tracks,
                    g_app.session.player_tank->id,
                    g_app.session.player_tank->pos.x,
                    g_app.session.player_tank->pos.y,
                    g_app.session.player_tank->body_angle, 0.45f,
                    track_strength_for_tank(g_app.session.player_tank));
            }

            // Powerup collection
            pz_powerup_type collected
                = pz_powerup_check_collection(g_app.session.powerup_mgr,
                    g_app.session.player_tank->pos, 0.7f);
            if (collected != PZ_POWERUP_NONE) {
                pz_tank_add_weapon(g_app.session.player_tank, (int)collected);
                pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Player collected: %s",
                    pz_powerup_type_name(collected));
            }

            // Player firing
            int current_weapon
                = pz_tank_get_current_weapon(g_app.session.player_tank);
            const pz_weapon_stats *weapon
                = pz_weapon_get_stats((pz_powerup_type)current_weapon);

            bool fire_held = g_app.mouse_left_down || g_app.space_down;
            bool fire_pressed
                = g_app.mouse_left_just_pressed || g_app.space_just_pressed;
            bool should_fire = weapon->auto_fire ? fire_held : fire_pressed;

            int active_projectiles = pz_projectile_count_by_owner(
                g_app.session.projectile_mgr, g_app.session.player_tank->id);
            bool can_fire = active_projectiles < weapon->max_active_projectiles;

            if (should_fire && can_fire
                && g_app.session.player_tank->fire_cooldown <= 0.0f) {
                pz_vec2 spawn_pos = { 0 };
                pz_vec2 fire_dir = { 0 };
                int bounce_cost = 0;
                pz_tank_get_fire_solution(g_app.session.player_tank,
                    g_app.session.map, &spawn_pos, &fire_dir, &bounce_cost);

                pz_projectile_config proj_config = {
                    .speed = weapon->projectile_speed,
                    .max_bounces = weapon->max_bounces,
                    .lifetime = -1.0f,
                    .damage = weapon->damage,
                    .scale = weapon->projectile_scale,
                    .color = weapon->projectile_color,
                };

                int proj_slot = pz_projectile_spawn(
                    g_app.session.projectile_mgr, spawn_pos, fire_dir,
                    &proj_config, g_app.session.player_tank->id);
                if (proj_slot >= 0 && bounce_cost > 0) {
                    pz_projectile *proj
                        = &g_app.session.projectile_mgr->projectiles[proj_slot];
                    if (proj->bounces_remaining > 0) {
                        proj->bounces_remaining -= 1;
                    }
                }

                g_app.session.player_tank->fire_cooldown
                    = weapon->fire_cooldown;

                // Trigger visual recoil
                g_app.session.player_tank->recoil = weapon->recoil_strength;

                // Play gunfire sound
                pz_game_sfx_play_gunfire(g_app.game_sfx);
            }
        }

        // Update all tanks (respawn timers, etc.)
        pz_tank_update_all(g_app.session.tank_mgr, g_app.session.map, dt);

        // Resolve tank-barrier collisions for all tanks
        if (g_app.session.barrier_mgr) {
            for (int i = 0; i < PZ_MAX_TANKS; i++) {
                pz_tank *tank = &g_app.session.tank_mgr->tanks[i];
                if ((tank->flags & PZ_TANK_FLAG_ACTIVE)
                    && !(tank->flags & PZ_TANK_FLAG_DEAD)) {
                    pz_barrier_resolve_collision(g_app.session.barrier_mgr,
                        &tank->pos, g_app.session.tank_mgr->collision_radius);
                }
            }
        }

        // AI update
        if (g_app.session.ai_mgr && g_app.session.player_tank
            && !(g_app.session.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
            pz_ai_update(g_app.session.ai_mgr, g_app.session.player_tank->pos,
                g_app.session.projectile_mgr, dt);
            int ai_shots = pz_ai_fire(
                g_app.session.ai_mgr, g_app.session.projectile_mgr);

            // Play gunfire sounds for AI shots
            for (int shot = 0; shot < ai_shots; shot++) {
                pz_game_sfx_play_gunfire(g_app.game_sfx);
            }

            // Track marks for enemy tanks
            if (g_app.session.tracks) {
                for (int i = 0; i < g_app.session.ai_mgr->controller_count;
                     i++) {
                    pz_ai_controller *ctrl
                        = &g_app.session.ai_mgr->controllers[i];
                    pz_tank *enemy = pz_tank_get_by_id(
                        g_app.session.tank_mgr, ctrl->tank_id);
                    if (enemy && !(enemy->flags & PZ_TANK_FLAG_DEAD)) {
                        if (pz_vec2_len(enemy->vel) > 0.1f) {
                            pz_tracks_add_mark(g_app.session.tracks, enemy->id,
                                enemy->pos.x, enemy->pos.y, enemy->body_angle,
                                0.45f, track_strength_for_tank(enemy));
                        }
                    }
                }
            }
        }

        // Powerup, barrier, and projectile updates
        pz_powerup_update(g_app.session.powerup_mgr, dt);
        if (g_app.session.barrier_mgr) {
            pz_barrier_update(g_app.session.barrier_mgr, dt);
        }
        pz_projectile_update(g_app.session.projectile_mgr, g_app.session.map,
            g_app.session.tank_mgr, dt);

        // Check projectile-barrier collisions
        if (g_app.session.barrier_mgr && g_app.session.projectile_mgr) {
            for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
                pz_projectile *proj
                    = &g_app.session.projectile_mgr->projectiles[i];
                if (!proj->active)
                    continue;

                pz_vec2 hit_pos, hit_normal;
                pz_barrier *barrier = NULL;

                // Check if projectile is inside a barrier
                // Use a small raycast from previous position to current
                pz_vec2 prev_pos
                    = pz_vec2_sub(proj->pos, pz_vec2_scale(proj->velocity, dt));
                if (pz_barrier_raycast(g_app.session.barrier_mgr, prev_pos,
                        proj->pos, &hit_pos, &hit_normal, &barrier)) {

                    // Apply damage to barrier
                    bool destroyed = false;
                    pz_barrier_apply_damage(g_app.session.barrier_mgr, hit_pos,
                        (float)proj->damage, &destroyed);

                    // Record hit for particle effects (reuse existing system)
                    if (g_app.session.projectile_mgr->hit_count
                        < PZ_MAX_PROJECTILE_HITS) {
                        pz_projectile_hit *hit
                            = &g_app.session.projectile_mgr->hits
                                   [g_app.session.projectile_mgr->hit_count++];
                        hit->type = destroyed ? PZ_HIT_WALL : PZ_HIT_WALL;
                        hit->pos = hit_pos;
                    }

                    // Destroy projectile
                    proj->active = false;
                    g_app.session.projectile_mgr->active_count--;

                    // If barrier was destroyed, spawn larger explosion
                    if (destroyed) {
                        pz_vec3 exp_pos
                            = { barrier->pos.x, 0.75f, barrier->pos.y };
                        pz_smoke_config explosion = PZ_SMOKE_TANK_HIT;
                        explosion.position = exp_pos;
                        explosion.count = 12;
                        explosion.spread = 1.0f;
                        explosion.scale_min = 1.5f;
                        explosion.scale_max = 2.5f;
                        pz_particle_spawn_smoke(
                            g_app.session.particle_mgr, &explosion);

                        // Add explosion light for destroyed barrier
                        for (int j = 0; j < MAX_EXPLOSION_LIGHTS; j++) {
                            if (g_app.session.explosion_lights[j].timer
                                <= 0.0f) {
                                g_app.session.explosion_lights[j].pos
                                    = barrier->pos;
                                g_app.session.explosion_lights[j].is_tank
                                    = false;
                                g_app.session.explosion_lights[j].duration
                                    = 0.3f;
                                g_app.session.explosion_lights[j].timer = 0.3f;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Hash game state for determinism verification
        if (g_app.session.player_tank) {
            pz_sim_hash_vec2(g_app.sim, g_app.session.player_tank->pos.x,
                g_app.session.player_tank->pos.y);
            pz_sim_hash_float(g_app.sim, g_app.session.player_tank->body_angle);
        }

        pz_sim_end_tick(g_app.sim);
    }

    fog_marks_update(&g_app.session, frame_dt);
    fog_marks_emit(&g_app.session);

    {
        pz_projectile_hit hits[PZ_MAX_PROJECTILE_HITS];
        int hit_count = pz_projectile_get_hits(
            g_app.session.projectile_mgr, hits, PZ_MAX_PROJECTILE_HITS);

        for (int i = 0; i < hit_count; i++) {
            pz_vec3 hit_pos = { hits[i].pos.x, 1.18f, hits[i].pos.y };

            pz_smoke_config smoke = PZ_SMOKE_BULLET_IMPACT;
            smoke.position = hit_pos;

            if (hits[i].type == PZ_HIT_TANK
                || hits[i].type == PZ_HIT_TANK_NON_FATAL) {
                smoke = PZ_SMOKE_TANK_HIT;
                smoke.position = hit_pos;
            }

            // Play bullet-hits-bullet sound
            if (hits[i].type == PZ_HIT_PROJECTILE) {
                pz_game_sfx_play_bullet_hit(g_app.game_sfx);
            }

            // Play tank hit sound (non-fatal hit)
            if (hits[i].type == PZ_HIT_TANK_NON_FATAL) {
                pz_game_sfx_play_tank_hit(g_app.game_sfx);
            }

            // Play ricochet sound (bullet bounces off wall)
            if (hits[i].type == PZ_HIT_WALL_RICOCHET) {
                pz_game_sfx_play_ricochet(g_app.game_sfx);
            }

            pz_particle_spawn_smoke(g_app.session.particle_mgr, &smoke);

            for (int j = 0; j < MAX_EXPLOSION_LIGHTS; j++) {
                if (g_app.session.explosion_lights[j].timer <= 0.0f) {
                    g_app.session.explosion_lights[j].pos = hits[i].pos;
                    g_app.session.explosion_lights[j].is_tank = false;
                    g_app.session.explosion_lights[j].duration = 0.15f;
                    g_app.session.explosion_lights[j].timer
                        = g_app.session.explosion_lights[j].duration;
                    break;
                }
            }
        }
    }

    // Process tank death events
    {
        pz_tank_death_event death_events[PZ_MAX_DEATH_EVENTS];
        int death_count = pz_tank_get_death_events(
            g_app.session.tank_mgr, death_events, PZ_MAX_DEATH_EVENTS);

        for (int i = 0; i < death_count; i++) {
            pz_vec3 death_pos
                = { death_events[i].pos.x, 0.6f, death_events[i].pos.y };

            // Spawn explosion particles
            pz_smoke_config explosion = PZ_SMOKE_TANK_EXPLOSION;
            explosion.position = death_pos;
            pz_particle_spawn_smoke(g_app.session.particle_mgr, &explosion);

            // Add explosion light
            for (int j = 0; j < MAX_EXPLOSION_LIGHTS; j++) {
                if (g_app.session.explosion_lights[j].timer <= 0.0f) {
                    g_app.session.explosion_lights[j].pos = death_events[i].pos;
                    g_app.session.explosion_lights[j].is_tank = true;
                    g_app.session.explosion_lights[j].duration = 0.4f;
                    g_app.session.explosion_lights[j].timer
                        = g_app.session.explosion_lights[j].duration;
                    break;
                }
            }

            // Check win condition (all enemies defeated)
            if (!death_events[i].is_player && g_app.state == GAME_STATE_PLAYING
                && g_app.session.initial_enemy_count > 0) {
                int enemies_remaining
                    = pz_tank_count_enemies_alive(g_app.session.tank_mgr);
                if (enemies_remaining == 0) {
                    // Last enemy - play big explosion
                    pz_game_sfx_play_tank_explosion(g_app.game_sfx, true);
                    g_app.state = GAME_STATE_LEVEL_COMPLETE;
                    g_app.state_timer = 0.0f;
                    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
                        "Victory! All enemies defeated.");
                } else {
                    // Regular enemy explosion
                    pz_game_sfx_play_tank_explosion(g_app.game_sfx, false);
                }
            } else if (!death_events[i].is_player) {
                // Enemy died but we're not in playing state (or no enemies to
                // track)
                pz_game_sfx_play_tank_explosion(g_app.game_sfx, false);
            } else {
                // Player died
                pz_game_sfx_play_tank_explosion(g_app.game_sfx, false);
            }

            // Handle player death (lives system)
            if (death_events[i].is_player
                && g_app.state == GAME_STATE_PLAYING) {
                if (g_app.campaign_mgr && g_app.campaign_mgr->loaded) {
                    // Campaign mode - use lives
                    if (!pz_campaign_player_died(g_app.campaign_mgr)) {
                        // No lives left - game over
                        g_app.state = GAME_STATE_GAME_OVER;
                        g_app.state_timer = 0.0f;
                    } else {
                        // Still have lives - respawn after delay
                        // (Tank respawn is handled by tank manager)
                    }
                }
                // Single map mode - just respawn (handled by tank manager)
            }
        }

        // Clear events for next frame
        pz_tank_clear_death_events(g_app.session.tank_mgr);
    }

    // =========================================================================
    // VISUAL-ONLY UPDATES (use frame_dt for smooth animation)
    // =========================================================================
    for (int i = 0; i < MAX_EXPLOSION_LIGHTS; i++) {
        if (g_app.session.explosion_lights[i].timer > 0.0f) {
            g_app.session.explosion_lights[i].timer -= frame_dt;
        }
    }

    spawn_tank_fog(
        g_app.session.particle_mgr, g_app.session.tank_mgr, frame_dt);
    spawn_projectile_fog(
        g_app.session.particle_mgr, g_app.session.projectile_mgr, frame_dt);

    pz_particle_update(g_app.session.particle_mgr, frame_dt);

    // Update engine sounds for all tanks
    pz_game_sfx_update_engines(g_app.game_sfx, g_app.session.tank_mgr);

    if (g_app.game_music && g_app.session.tank_mgr) {
        int enemies_alive = pz_tank_count_enemies_alive(g_app.session.tank_mgr);
        bool has_level3 = pz_ai_has_level3_alive(g_app.session.ai_mgr);
        bool level_complete = (g_app.state == GAME_STATE_LEVEL_COMPLETE);
        pz_game_music_update(g_app.game_music, enemies_alive, has_level3,
            level_complete, frame_dt);
    }

    double now = pz_time_now();
    if (now - g_app.last_hot_reload_check > 0.5) {
        pz_texture_check_hot_reload(g_app.tex_manager);
        pz_map_hot_reload_check(g_app.session.hot_reload);
        g_app.last_hot_reload_check = now;
    }

    pz_debug_overlay_begin_frame(g_app.debug_overlay);
    pz_renderer_begin_frame(g_app.renderer);
    pz_renderer_clear(g_app.renderer, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    // Render background (sky gradient) first
    int vp_width, vp_height;
    pz_renderer_get_viewport(g_app.renderer, &vp_width, &vp_height);
    pz_background_render(g_app.background, g_app.renderer, vp_width, vp_height);

    pz_tracks_update(g_app.session.tracks);

    if (g_app.session.lighting && g_app.session.map) {
        pz_lighting_clear_occluders(g_app.session.lighting);
        pz_lighting_add_map_occluders(
            g_app.session.lighting, g_app.session.map);

        // Add barrier occluders
        if (g_app.session.barrier_mgr) {
            pz_barrier_add_occluders(
                g_app.session.barrier_mgr, g_app.session.lighting);
        }

        pz_lighting_clear_lights(g_app.session.lighting);

        for (int i = 0; i < PZ_MAX_TANKS; i++) {
            pz_tank *tank = &g_app.session.tank_mgr->tanks[i];
            if ((tank->flags & PZ_TANK_FLAG_ACTIVE)
                && !(tank->flags & PZ_TANK_FLAG_DEAD)) {

                float light_offset = 0.8f;
                pz_vec2 light_dir
                    = { sinf(tank->turret_angle), cosf(tank->turret_angle) };
                pz_vec2 light_pos = pz_vec2_add(
                    tank->pos, pz_vec2_scale(light_dir, light_offset));

                if (g_app.session.map) {
                    bool hit = false;
                    pz_vec2 hit_pos = pz_map_raycast(g_app.session.map,
                        tank->pos, light_dir, light_offset, &hit);
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

                pz_lighting_add_spotlight(g_app.session.lighting, light_pos,
                    light_dir_2d, light_color, 1.2f, 22.5f, PZ_PI * 0.35f,
                    0.3f);
            }
        }

        for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
            pz_projectile *proj = &g_app.session.projectile_mgr->projectiles[i];
            if (proj->active) {
                pz_vec3 proj_light_color
                    = { proj->color.x, proj->color.y, proj->color.z };

                float backward_angle
                    = atan2f(-proj->velocity.y, -proj->velocity.x);

                pz_lighting_add_spotlight(g_app.session.lighting, proj->pos,
                    backward_angle, proj_light_color, 0.4f, 2.8f,
                    PZ_PI * 0.125f, 0.5f);
            }
        }

        for (int i = 0; i < MAX_EXPLOSION_LIGHTS; i++) {
            if (g_app.session.explosion_lights[i].timer > 0.0f) {
                float t = g_app.session.explosion_lights[i].timer
                    / g_app.session.explosion_lights[i].duration;
                float intensity = t * t;

                if (g_app.session.explosion_lights[i].is_tank) {
                    pz_vec3 exp_color = { 1.0f, 0.3f + 0.5f * t, 0.1f * t };
                    pz_lighting_add_point_light(g_app.session.lighting,
                        g_app.session.explosion_lights[i].pos, exp_color,
                        3.0f * intensity, 6.0f);
                } else {
                    pz_vec3 exp_color = { 0.7f, 0.8f, 1.0f };
                    pz_lighting_add_point_light(g_app.session.lighting,
                        g_app.session.explosion_lights[i].pos, exp_color,
                        2.0f * intensity, 4.0f);
                }
            }
        }

        for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
            pz_powerup *powerup = &g_app.session.powerup_mgr->powerups[i];
            if (!powerup->active || powerup->collected)
                continue;

            const pz_weapon_stats *stats = pz_weapon_get_stats(powerup->type);
            pz_vec3 powerup_color = { stats->projectile_color.x,
                stats->projectile_color.y, stats->projectile_color.z };

            float flicker
                = pz_powerup_get_flicker(g_app.session.powerup_mgr, i);

            pz_lighting_add_point_light(g_app.session.lighting, powerup->pos,
                powerup_color, 1.0f * flicker, 3.5f);
        }

        pz_lighting_render(g_app.session.lighting);
    }

    const pz_mat4 *vp = pz_camera_get_view_projection(&g_app.camera);

    pz_map_render_params render_params = { 0 };
    if (g_app.session.tracks) {
        render_params.track_texture
            = pz_tracks_get_texture(g_app.session.tracks);
        pz_tracks_get_uv_transform(g_app.session.tracks,
            &render_params.track_scale_x, &render_params.track_scale_z,
            &render_params.track_offset_x, &render_params.track_offset_z);
    }
    if (g_app.session.lighting) {
        render_params.light_texture
            = pz_lighting_get_texture(g_app.session.lighting);
        pz_lighting_get_uv_transform(g_app.session.lighting,
            &render_params.light_scale_x, &render_params.light_scale_z,
            &render_params.light_offset_x, &render_params.light_offset_z);
    }
    if (g_app.session.map) {
        const pz_map_lighting *map_light
            = pz_map_get_lighting(g_app.session.map);
        render_params.has_sun = map_light->has_sun;
        render_params.sun_direction = map_light->sun_direction;
        render_params.sun_color = map_light->sun_color;
    }

    render_params.fog_disturb_count = 0;
    render_params.fog_disturb_strength = 1.0f;
    if (g_app.session.map && g_app.session.map->has_fog
        && (g_app.session.map->fog_level == 0
            || g_app.session.map->fog_level == 1)) {
        for (int i = 0; i < MAX_FOG_MARKS; i++) {
            fog_mark *mark = &g_app.session.fog_marks[i];
            if (!mark->active || mark->duration <= 0.0f) {
                continue;
            }

            float t = pz_clampf(mark->timer / mark->duration, 0.0f, 1.0f);
            float strength = mark->strength * t;
            if (render_params.fog_disturb_count < PZ_FOG_DISTURB_MAX) {
                int idx = render_params.fog_disturb_count++;
                render_params.fog_disturb_pos[idx]
                    = (pz_vec3) { mark->pos.x, 0.0f, mark->pos.y };
                render_params.fog_disturb_radius[idx] = mark->radius;
                render_params.fog_disturb_strengths[idx] = strength;
                continue;
            }

            int weakest = 0;
            float weakest_strength = render_params.fog_disturb_strengths[0];
            for (int j = 1; j < render_params.fog_disturb_count; j++) {
                if (render_params.fog_disturb_strengths[j] < weakest_strength) {
                    weakest = j;
                    weakest_strength = render_params.fog_disturb_strengths[j];
                }
            }

            if (strength > weakest_strength) {
                render_params.fog_disturb_pos[weakest]
                    = (pz_vec3) { mark->pos.x, 0.0f, mark->pos.y };
                render_params.fog_disturb_radius[weakest] = mark->radius;
                render_params.fog_disturb_strengths[weakest] = strength;
            }
        }
    }

    // Time for water animation
    render_params.time = g_app.total_time;

    pz_map_renderer_draw(g_app.session.renderer, vp, &render_params);

    // Draw debug texture scale grid if enabled
    pz_map_renderer_draw_debug(g_app.session.renderer, vp);

    // Render barriers (after map, before tanks)
    if (g_app.session.barrier_mgr) {
        pz_barrier_render_params barrier_params = { 0 };
        if (g_app.session.lighting) {
            barrier_params.light_texture
                = pz_lighting_get_texture(g_app.session.lighting);
            pz_lighting_get_uv_transform(g_app.session.lighting,
                &barrier_params.light_scale_x, &barrier_params.light_scale_z,
                &barrier_params.light_offset_x, &barrier_params.light_offset_z);
        }
        if (g_app.session.map) {
            const pz_map_lighting *map_light
                = pz_map_get_lighting(g_app.session.map);
            barrier_params.has_sun = map_light->has_sun;
            barrier_params.sun_direction = map_light->sun_direction;
            barrier_params.sun_color = map_light->sun_color;
            barrier_params.ambient = map_light->ambient_color;
        }
        pz_barrier_render(
            g_app.session.barrier_mgr, g_app.renderer, vp, &barrier_params);
    }

    pz_tank_render_params tank_params = { 0 };
    if (g_app.session.lighting) {
        tank_params.light_texture
            = pz_lighting_get_texture(g_app.session.lighting);
        pz_lighting_get_uv_transform(g_app.session.lighting,
            &tank_params.light_scale_x, &tank_params.light_scale_z,
            &tank_params.light_offset_x, &tank_params.light_offset_z);
    }
    pz_tank_render(g_app.session.tank_mgr, g_app.renderer, vp, &tank_params);

    pz_powerup_render(g_app.session.powerup_mgr, g_app.renderer, vp);

    if (g_app.laser_pipeline != PZ_INVALID_HANDLE && g_app.session.map
        && g_app.session.player_tank
        && !(g_app.session.player_tank->flags & PZ_TANK_FLAG_DEAD)) {
        pz_vec2 laser_start = { 0 };
        pz_vec2 laser_dir = { 0 };
        int bounce_cost = 0;
        pz_tank_get_fire_solution(g_app.session.player_tank, g_app.session.map,
            &laser_start, &laser_dir, &bounce_cost);

        pz_vec2 ray_start = laser_start;
        pz_vec2 ray_end
            = pz_vec2_add(ray_start, pz_vec2_scale(laser_dir, LASER_MAX_DIST));

        pz_raycast_result map_hit
            = pz_map_raycast_ex(g_app.session.map, ray_start, ray_end);
        pz_vec2 laser_end = map_hit.hit ? map_hit.point : ray_end;

        // Also check barrier collision for laser
        if (g_app.session.barrier_mgr) {
            pz_vec2 barrier_hit_pos;
            if (pz_barrier_raycast(g_app.session.barrier_mgr, ray_start,
                    ray_end, &barrier_hit_pos, NULL, NULL)) {
                float barrier_dist = pz_vec2_dist(ray_start, barrier_hit_pos);
                float best_dist
                    = map_hit.hit ? map_hit.distance : LASER_MAX_DIST;
                if (barrier_dist < best_dist) {
                    laser_end = barrier_hit_pos;
                }
            }
        }

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
    if (g_app.session.lighting) {
        proj_params.light_texture
            = pz_lighting_get_texture(g_app.session.lighting);
        pz_lighting_get_uv_transform(g_app.session.lighting,
            &proj_params.light_scale_x, &proj_params.light_scale_z,
            &proj_params.light_offset_x, &proj_params.light_offset_z);
    }
    pz_projectile_render(
        g_app.session.projectile_mgr, g_app.renderer, vp, &proj_params);

    {
        const pz_mat4 *view = pz_camera_get_view(&g_app.camera);
        pz_vec3 cam_right = { view->m[0], view->m[4], view->m[8] };
        pz_vec3 cam_up = { view->m[1], view->m[5], view->m[9] };

        pz_particle_render_params particle_params = { 0 };
        if (g_app.session.lighting) {
            particle_params.light_texture
                = pz_lighting_get_texture(g_app.session.lighting);
            pz_lighting_get_uv_transform(g_app.session.lighting,
                &particle_params.light_scale_x, &particle_params.light_scale_z,
                &particle_params.light_offset_x,
                &particle_params.light_offset_z);
        }

        pz_particle_render(g_app.session.particle_mgr, g_app.renderer, vp,
            cam_right, cam_up, &particle_params);
    }

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
        if (g_app.session.player_tank) {
            pz_font_drawf(g_app.font_mgr, &health_style, vp_width - 20.0f,
                vp_height - 20.0f, "HP: %d", g_app.session.player_tank->health);
        }

        // Lives display (bottom-left) - only in campaign mode
        if (g_app.campaign_mgr && g_app.campaign_mgr->loaded) {
            pz_text_style lives_style
                = PZ_TEXT_STYLE_DEFAULT(g_app.font_russo, 28.0f);
            lives_style.align_h = PZ_FONT_ALIGN_LEFT;
            lives_style.align_v = PZ_FONT_ALIGN_BOTTOM;
            lives_style.color = pz_vec4_new(0.6f, 0.9f, 1.0f, 1.0f);
            lives_style.outline_width = 2.0f;
            lives_style.outline_color = pz_vec4_new(0.0f, 0.0f, 0.0f, 1.0f);

            pz_font_drawf(g_app.font_mgr, &lives_style, 20.0f,
                vp_height - 20.0f, "Lives: %d",
                pz_campaign_get_lives(g_app.campaign_mgr));

            // Level indicator (top-left)
            pz_text_style level_style
                = PZ_TEXT_STYLE_DEFAULT(g_app.font_russo, 24.0f);
            level_style.align_h = PZ_FONT_ALIGN_LEFT;
            level_style.align_v = PZ_FONT_ALIGN_TOP;
            level_style.color = pz_vec4_new(0.8f, 0.8f, 0.8f, 1.0f);
            level_style.outline_width = 2.0f;
            level_style.outline_color = pz_vec4_new(0.0f, 0.0f, 0.0f, 1.0f);

            pz_font_drawf(g_app.font_mgr, &level_style, 20.0f, 20.0f,
                "Level %d/%d", pz_campaign_get_level_number(g_app.campaign_mgr),
                pz_campaign_get_level_count(g_app.campaign_mgr));
        }

        // Enemies remaining (top-right)
        if (g_app.session.initial_enemy_count > 0) {
            int enemies_alive
                = pz_tank_count_enemies_alive(g_app.session.tank_mgr);

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

        // State-based overlays
        pz_text_style title_style
            = PZ_TEXT_STYLE_DEFAULT(g_app.font_russo, 64.0f);
        title_style.align_h = PZ_FONT_ALIGN_CENTER;
        title_style.align_v = PZ_FONT_ALIGN_MIDDLE;
        title_style.outline_width = 4.0f;

        pz_text_style subtitle_style
            = PZ_TEXT_STYLE_DEFAULT(g_app.font_russo, 28.0f);
        subtitle_style.align_h = PZ_FONT_ALIGN_CENTER;
        subtitle_style.align_v = PZ_FONT_ALIGN_MIDDLE;
        subtitle_style.color = pz_vec4_new(0.9f, 0.9f, 0.9f, 1.0f);
        subtitle_style.outline_width = 2.0f;
        subtitle_style.outline_color = pz_vec4_new(0.0f, 0.0f, 0.0f, 1.0f);

        if (g_app.state == GAME_STATE_LEVEL_COMPLETE) {
            g_app.state_timer += frame_dt;

            title_style.color = pz_vec4_new(1.0f, 0.9f, 0.3f, 1.0f);
            title_style.outline_color = pz_vec4_new(0.2f, 0.15f, 0.0f, 1.0f);

            pz_font_draw(g_app.font_mgr, &title_style, vp_width * 0.5f,
                vp_height * 0.4f, "LEVEL COMPLETE!");

            if (g_app.state_timer > 1.5f) {
                // Check if there are more levels
                bool has_next = g_app.campaign_mgr && g_app.campaign_mgr->loaded
                    && (pz_campaign_get_level_number(g_app.campaign_mgr)
                        < pz_campaign_get_level_count(g_app.campaign_mgr));

                if (has_next) {
                    pz_font_draw(g_app.font_mgr, &subtitle_style,
                        vp_width * 0.5f, vp_height * 0.55f,
                        "Press SPACE for next level, R to replay");
                } else if (g_app.campaign_mgr && g_app.campaign_mgr->loaded) {
                    // Last level of campaign - SPACE finishes campaign
                    pz_font_draw(g_app.font_mgr, &subtitle_style,
                        vp_width * 0.5f, vp_height * 0.55f,
                        "Press SPACE to finish, R to replay");
                } else {
                    // Single map mode
                    pz_font_draw(g_app.font_mgr, &subtitle_style,
                        vp_width * 0.5f, vp_height * 0.55f,
                        "Press R to replay");
                }
            }
        } else if (g_app.state == GAME_STATE_CAMPAIGN_COMPLETE) {
            g_app.state_timer += frame_dt;

            title_style.color = pz_vec4_new(1.0f, 0.9f, 0.3f, 1.0f);
            title_style.outline_color = pz_vec4_new(0.2f, 0.15f, 0.0f, 1.0f);

            pz_font_draw(g_app.font_mgr, &title_style, vp_width * 0.5f,
                vp_height * 0.4f, "CAMPAIGN COMPLETE!");

            if (g_app.state_timer > 1.5f) {
                pz_font_draw(g_app.font_mgr, &subtitle_style, vp_width * 0.5f,
                    vp_height * 0.55f, "Congratulations! Press R to restart");
            }
        } else if (g_app.state == GAME_STATE_GAME_OVER) {
            g_app.state_timer += frame_dt;

            title_style.color = pz_vec4_new(1.0f, 0.3f, 0.3f, 1.0f);
            title_style.outline_color = pz_vec4_new(0.3f, 0.0f, 0.0f, 1.0f);

            pz_font_draw(g_app.font_mgr, &title_style, vp_width * 0.5f,
                vp_height * 0.4f, "GAME OVER");

            if (g_app.state_timer > 1.5f) {
                pz_font_draw(g_app.font_mgr, &subtitle_style, vp_width * 0.5f,
                    vp_height * 0.55f, "Press R to restart campaign");
            }
        }

        pz_font_end_frame(g_app.font_mgr);
    }

    // Render debug overlay on top of everything
    render_music_debug_overlay();
    pz_debug_overlay_render(g_app.debug_overlay);
    pz_debug_overlay_end_frame(g_app.debug_overlay);

    bool should_quit = false;
    g_app.frame_count++;
    if (g_app.auto_screenshot && g_app.frame_count >= g_app.screenshot_frames) {
        pz_renderer_save_screenshot(g_app.renderer, g_app.screenshot_path);
        should_quit = true;
    }

    if (g_app.lightmap_debug_path
        && g_app.frame_count >= g_app.screenshot_frames) {
        pz_lighting_save_debug(
            g_app.session.lighting, g_app.lightmap_debug_path);
        g_app.lightmap_debug_path = NULL;
    }

    pz_renderer_end_frame(g_app.renderer);

    if (should_quit) {
        sapp_quit();
    }

    g_app.mouse_left_just_pressed = false;
    g_app.space_just_pressed = false;
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
            } else if (event->key_code == SAPP_KEYCODE_F3) {
                // Toggle texture scale debug visualization
                if (g_app.session.renderer) {
                    bool enabled = pz_map_renderer_get_debug_texture_scale(
                        g_app.session.renderer);
                    pz_map_renderer_set_debug_texture_scale(
                        g_app.session.renderer, !enabled);
                }
            } else if (event->key_code == SAPP_KEYCODE_F11) {
                if (g_app.session.lighting) {
                    pz_lighting_save_debug(g_app.session.lighting,
                        "screenshots/lightmap_debug.png");
                }
            } else if (event->key_code == SAPP_KEYCODE_F12) {
                char *path = generate_screenshot_path();
                if (path) {
                    pz_renderer_save_screenshot(g_app.renderer, path);
                    pz_free(path);
                }
            } else if (event->key_code == SAPP_KEYCODE_F) {
                g_app.key_f_just_pressed = true;
            } else if (event->key_code == SAPP_KEYCODE_SPACE) {
                // SPACE fires during gameplay, advances level when complete
                g_app.space_down = true;
                g_app.space_just_pressed = true;
                if (g_app.state == GAME_STATE_LEVEL_COMPLETE
                    && g_app.state_timer > 1.5f) {
                    // Consume the space press so it doesn't fire on new level
                    g_app.space_just_pressed = false;
                    if (g_app.campaign_mgr && g_app.campaign_mgr->loaded) {
                        if (pz_campaign_advance(g_app.campaign_mgr)) {
                            // Load next map
                            const char *next_map = pz_campaign_get_current_map(
                                g_app.campaign_mgr);
                            if (next_map
                                && map_session_load(&g_app.session, next_map)) {
                                g_app.state = GAME_STATE_PLAYING;
                                g_app.state_timer = 0.0f;
                            } else {
                                pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
                                    "Failed to load next map");
                            }
                        } else {
                            // Campaign complete!
                            g_app.state = GAME_STATE_CAMPAIGN_COMPLETE;
                            g_app.state_timer = 0.0f;
                        }
                    }
                }
            } else if (event->key_code == SAPP_KEYCODE_R) {
                // R key behavior depends on current state
                if (g_app.state == GAME_STATE_LEVEL_COMPLETE
                    && g_app.state_timer > 1.5f) {
                    // Replay current level
                    map_session_reset(&g_app.session);
                    g_app.state = GAME_STATE_PLAYING;
                    g_app.state_timer = 0.0f;
                    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Level restarted");
                } else if (g_app.state == GAME_STATE_GAME_OVER
                    && g_app.state_timer > 1.5f) {
                    // Restart entire campaign
                    if (g_app.campaign_mgr && g_app.campaign_mgr->loaded) {
                        pz_campaign_start(g_app.campaign_mgr, 0);
                        const char *first_map
                            = pz_campaign_get_current_map(g_app.campaign_mgr);
                        if (first_map
                            && map_session_load(&g_app.session, first_map)) {
                            g_app.state = GAME_STATE_PLAYING;
                            g_app.state_timer = 0.0f;
                            pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
                                "Campaign restarted");
                        }
                    } else {
                        // Single map mode - just reset
                        map_session_reset(&g_app.session);
                        g_app.state = GAME_STATE_PLAYING;
                        g_app.state_timer = 0.0f;
                    }
                } else if (g_app.state == GAME_STATE_CAMPAIGN_COMPLETE
                    && g_app.state_timer > 1.5f) {
                    // Restart campaign from beginning
                    if (g_app.campaign_mgr && g_app.campaign_mgr->loaded) {
                        pz_campaign_start(g_app.campaign_mgr, 0);
                        const char *first_map
                            = pz_campaign_get_current_map(g_app.campaign_mgr);
                        if (first_map
                            && map_session_load(&g_app.session, first_map)) {
                            g_app.state = GAME_STATE_PLAYING;
                            g_app.state_timer = 0.0f;
                            pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
                                "Campaign restarted");
                        }
                    }
                }
            }
        }
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        if (event->key_code >= 0 && event->key_code < SAPP_KEYCODE_COUNT) {
            g_app.key_down[event->key_code] = false;
        }
        if (event->key_code == SAPP_KEYCODE_SPACE) {
            g_app.space_down = false;
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
    // Unload map session (all map-dependent state)
    map_session_unload(&g_app.session);

    // Destroy campaign manager
    pz_campaign_destroy(g_app.campaign_mgr);

    // Destroy persistent systems
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

    pz_background_destroy(g_app.background, g_app.renderer);

    pz_sim_destroy(g_app.sim);

    pz_tile_registry_destroy(g_app.tile_registry);
    pz_texture_manager_destroy(g_app.tex_manager);
    pz_renderer_destroy(g_app.renderer);

    if (g_app.audio) {
        pz_audio_set_callback(g_app.audio, NULL, NULL);
        pz_audio_shutdown(g_app.audio);
    }
    pz_game_sfx_destroy(g_app.game_sfx);
    pz_game_music_destroy(g_app.game_music);

    pz_log_shutdown();
    pz_mem_dump_leaks();

    printf("Tank Game - Exiting.\n");
}

static void
audio_callback(float *buffer, int num_frames, int num_channels, void *userdata)
{
    (void)userdata;

    // Render music first (fills buffer)
    pz_game_music_render(g_app.game_music, buffer, num_frames, num_channels);

    // Render SFX on top (adds to buffer)
    pz_game_sfx_render(g_app.game_sfx, buffer, num_frames, num_channels);
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
