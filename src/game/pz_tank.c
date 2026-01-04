/*
 * Tank Game - Tank Entity System Implementation
 */

#include "pz_tank.h"
#include "pz_powerup.h"

#include <math.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

/* ============================================================================
 * Constants
 * ============================================================================
 */

// Barrel length from turret center to tip (must match turret mesh)
static const float BARREL_LENGTH = 1.65f;
// Allow tiny overlap before treating the barrel as blocked
static const float BARREL_CLEAR_EPSILON = 0.02f;
// Small offset to push a deflected projectile off the wall
static const float BARREL_DEFLECT_EPSILON = 0.01f;

// Turret height offset above ground
static const float TURRET_Y_OFFSET = 0.65f;

// Shadow dimensions and offset
static const float SHADOW_WIDTH = 1.7f;
static const float SHADOW_LENGTH = 2.5f;
static const float SHADOW_Y_OFFSET = 0.02f;
static const float SHADOW_ALPHA = 0.35f;
static const float SHADOW_SOFTNESS = 0.09f;

// Time before respawn after death
static const float RESPAWN_DELAY = 3.0f;

// Duration of damage flash effect
static const float DAMAGE_FLASH_DURATION = 0.15f;

// Duration of invulnerability after respawn
static const float INVULN_DURATION = 1.5f;

// Default health
static const int DEFAULT_HEALTH = 10;

static pz_mesh *
create_shadow_mesh(void)
{
    float half_w = SHADOW_WIDTH * 0.5f;
    float half_l = SHADOW_LENGTH * 0.5f;
    pz_mesh_vertex verts[6] = {
        { -half_w, 0.0f, -half_l, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
        { half_w, 0.0f, -half_l, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f },
        { half_w, 0.0f, half_l, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
        { -half_w, 0.0f, -half_l, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
        { half_w, 0.0f, half_l, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
        { -half_w, 0.0f, half_l, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f },
    };

    return pz_mesh_create_from_data(verts, 6);
}

static bool
circle_overlaps_tile(pz_vec2 center, float radius, float min_x, float min_y,
    float max_x, float max_y, pz_vec2 *push_out)
{
    float nearest_x = pz_clampf(center.x, min_x, max_x);
    float nearest_y = pz_clampf(center.y, min_y, max_y);
    float dx = center.x - nearest_x;
    float dy = center.y - nearest_y;
    float dist_sq = dx * dx + dy * dy;
    float radius_sq = radius * radius;

    if (dist_sq >= radius_sq) {
        return false;
    }

    if (!push_out) {
        return true;
    }

    if (dist_sq > 0.000001f) {
        float dist = sqrtf(dist_sq);
        float push = radius - dist;
        push_out->x = (dx / dist) * push;
        push_out->y = (dy / dist) * push;
        return true;
    }

    float left = center.x - min_x;
    float right = max_x - center.x;
    float bottom = center.y - min_y;
    float top = max_y - center.y;

    float min_dist = left;
    pz_vec2 normal = { -1.0f, 0.0f };
    if (right < min_dist) {
        min_dist = right;
        normal = (pz_vec2) { 1.0f, 0.0f };
    }
    if (bottom < min_dist) {
        min_dist = bottom;
        normal = (pz_vec2) { 0.0f, -1.0f };
    }
    if (top < min_dist) {
        min_dist = top;
        normal = (pz_vec2) { 0.0f, 1.0f };
    }

    push_out->x = normal.x * (radius + min_dist);
    push_out->y = normal.y * (radius + min_dist);
    return true;
}

static bool
tank_circle_hits_map(const pz_map *map, pz_vec2 center, float radius)
{
    if (!map)
        return false;

    float half_w = map->world_width / 2.0f;
    float half_h = map->world_height / 2.0f;
    float ts = map->tile_size;

    int min_tx = (int)floorf((center.x - radius + half_w) / ts);
    int max_tx = (int)floorf((center.x + radius + half_w) / ts);
    int min_ty = (int)floorf((center.y - radius + half_h) / ts);
    int max_ty = (int)floorf((center.y + radius + half_h) / ts);

    for (int ty = min_ty; ty <= max_ty; ty++) {
        for (int tx = min_tx; tx <= max_tx; tx++) {
            if (!pz_map_in_bounds(map, tx, ty)) {
                return true;
            }
            if (pz_map_get_height(map, tx, ty) == 0) {
                continue;
            }

            float min_x = tx * ts - half_w;
            float min_y = ty * ts - half_h;
            float max_x = min_x + ts;
            float max_y = min_y + ts;

            if (circle_overlaps_tile(
                    center, radius, min_x, min_y, max_x, max_y, NULL)) {
                return true;
            }
        }
    }

    return false;
}

static void
resolve_tank_circle_map(const pz_map *map, pz_vec2 *center, float radius)
{
    if (!map || !center)
        return;

    float half_w = map->world_width / 2.0f;
    float half_h = map->world_height / 2.0f;
    float ts = map->tile_size;

    for (int iter = 0; iter < 4; iter++) {
        bool any = false;

        float min_x = -half_w + radius;
        float max_x = half_w - radius;
        float min_y = -half_h + radius;
        float max_y = half_h - radius;

        if (center->x < min_x) {
            center->x = min_x;
            any = true;
        } else if (center->x > max_x) {
            center->x = max_x;
            any = true;
        }
        if (center->y < min_y) {
            center->y = min_y;
            any = true;
        } else if (center->y > max_y) {
            center->y = max_y;
            any = true;
        }

        int min_tx = (int)floorf((center->x - radius + half_w) / ts);
        int max_tx = (int)floorf((center->x + radius + half_w) / ts);
        int min_ty = (int)floorf((center->y - radius + half_h) / ts);
        int max_ty = (int)floorf((center->y + radius + half_h) / ts);

        for (int ty = min_ty; ty <= max_ty; ty++) {
            for (int tx = min_tx; tx <= max_tx; tx++) {
                if (!pz_map_in_bounds(map, tx, ty)) {
                    continue;
                }
                if (pz_map_get_height(map, tx, ty) == 0) {
                    continue;
                }

                float tile_min_x = tx * ts - half_w;
                float tile_min_y = ty * ts - half_h;
                float tile_max_x = tile_min_x + ts;
                float tile_max_y = tile_min_y + ts;
                pz_vec2 push_out = { 0.0f, 0.0f };

                if (circle_overlaps_tile(*center, radius, tile_min_x,
                        tile_min_y, tile_max_x, tile_max_y, &push_out)) {
                    center->x += push_out.x;
                    center->y += push_out.y;
                    any = true;
                }
            }
        }

        if (!any) {
            break;
        }
    }
}

// Update turret color to match current weapon's projectile color
static void
update_turret_color(pz_tank *tank)
{
    if (!tank)
        return;

    int weapon_type = pz_tank_get_current_weapon(tank);
    const pz_weapon_stats *stats
        = pz_weapon_get_stats((pz_powerup_type)weapon_type);

    // Use the projectile color for the turret
    tank->turret_color = stats->projectile_color;
}

/* ============================================================================
 * Default Configuration
 * ============================================================================
 */

const pz_tank_manager_config PZ_TANK_DEFAULT_CONFIG = {
    .accel = 40.0f,
    .friction = 25.0f,
    .max_speed = 3.5f,
    .body_turn_speed = 3.5f,
    .turret_turn_speed = 5.6f,
    .collision_radius = 0.9f,
};

/* ============================================================================
 * Manager Lifecycle
 * ============================================================================
 */

pz_tank_manager *
pz_tank_manager_create(
    pz_renderer *renderer, const pz_tank_manager_config *config)
{
    pz_tank_manager *mgr = pz_calloc(1, sizeof(pz_tank_manager));

    // Use default config if none provided
    if (!config) {
        config = &PZ_TANK_DEFAULT_CONFIG;
    }

    mgr->accel = config->accel;
    mgr->friction = config->friction;
    mgr->max_speed = config->max_speed;
    mgr->body_turn_speed = config->body_turn_speed;
    mgr->turret_turn_speed = config->turret_turn_speed;
    mgr->collision_radius = config->collision_radius;
    mgr->next_id = 1;

    // Create meshes
    mgr->body_mesh = pz_mesh_create_tank_body();
    mgr->turret_mesh = pz_mesh_create_tank_turret();
    mgr->shadow_mesh = create_shadow_mesh();

    if (mgr->body_mesh) {
        pz_mesh_upload(mgr->body_mesh, renderer);
    }
    if (mgr->turret_mesh) {
        pz_mesh_upload(mgr->turret_mesh, renderer);
    }
    if (mgr->shadow_mesh) {
        pz_mesh_upload(mgr->shadow_mesh, renderer);
    }

    // Load shader
    mgr->shader = pz_renderer_load_shader(
        renderer, "shaders/entity.vert", "shaders/entity.frag", "tank");

    if (mgr->shader != PZ_INVALID_HANDLE) {
        pz_pipeline_desc desc = {
            .shader = mgr->shader,
            .vertex_layout = pz_mesh_get_vertex_layout(),
            .blend = PZ_BLEND_NONE,
            .depth = PZ_DEPTH_READ_WRITE,
            .cull = PZ_CULL_BACK,
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        mgr->pipeline = pz_renderer_create_pipeline(renderer, &desc);

        pz_pipeline_desc shadow_desc = desc;
        shadow_desc.blend = PZ_BLEND_ALPHA;
        shadow_desc.depth = PZ_DEPTH_READ;
        shadow_desc.cull = PZ_CULL_NONE;
        mgr->shadow_pipeline
            = pz_renderer_create_pipeline(renderer, &shadow_desc);

        mgr->render_ready = (mgr->pipeline != PZ_INVALID_HANDLE);
    }

    if (!mgr->render_ready) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Tank rendering not available (shader/pipeline failed)");
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Tank manager created");
    return mgr;
}

void
pz_tank_manager_destroy(pz_tank_manager *mgr, pz_renderer *renderer)
{
    if (!mgr)
        return;

    if (mgr->pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(renderer, mgr->pipeline);
    }
    if (mgr->shadow_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(renderer, mgr->shadow_pipeline);
    }
    if (mgr->shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(renderer, mgr->shader);
    }
    if (mgr->body_mesh) {
        pz_mesh_destroy(mgr->body_mesh, renderer);
    }
    if (mgr->turret_mesh) {
        pz_mesh_destroy(mgr->turret_mesh, renderer);
    }
    if (mgr->shadow_mesh) {
        pz_mesh_destroy(mgr->shadow_mesh, renderer);
    }

    pz_free(mgr);
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Tank manager destroyed");
}

/* ============================================================================
 * Tank Spawning
 * ============================================================================
 */

pz_tank *
pz_tank_spawn(pz_tank_manager *mgr, pz_vec2 pos, pz_vec4 color, bool is_player)
{
    if (!mgr)
        return NULL;

    // Find free slot
    int slot = -1;
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        if (!(mgr->tanks[i].flags & PZ_TANK_FLAG_ACTIVE)) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "No free tank slots (max=%d)",
            PZ_MAX_TANKS);
        return NULL;
    }

    pz_tank *tank = &mgr->tanks[slot];
    memset(tank, 0, sizeof(pz_tank));

    tank->flags = PZ_TANK_FLAG_ACTIVE;
    if (is_player) {
        tank->flags |= PZ_TANK_FLAG_PLAYER;
    }

    tank->id = mgr->next_id++;
    tank->pos = pos;
    tank->spawn_pos = pos;
    tank->vel = (pz_vec2) { 0.0f, 0.0f };
    tank->body_angle = 0.0f;
    tank->turret_angle = 0.0f;

    tank->health = DEFAULT_HEALTH;
    tank->max_health = DEFAULT_HEALTH;
    tank->fire_cooldown = 0.0f;

    // Initialize loadout with default weapon
    tank->loadout[0] = PZ_POWERUP_NONE; // Default cannon
    tank->loadout_count = 1;
    tank->loadout_index = 0;

    tank->body_color = color;
    // Turret color matches current weapon's projectile color
    update_turret_color(tank);

    mgr->tank_count++;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
        "Tank spawned at (%.2f, %.2f), id=%d, player=%d", pos.x, pos.y,
        tank->id, is_player);

    return tank;
}

pz_tank *
pz_tank_get_by_id(pz_tank_manager *mgr, int id)
{
    if (!mgr)
        return NULL;

    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &mgr->tanks[i];
        if ((tank->flags & PZ_TANK_FLAG_ACTIVE) && tank->id == id) {
            return tank;
        }
    }
    return NULL;
}

pz_tank *
pz_tank_get_player(pz_tank_manager *mgr)
{
    if (!mgr)
        return NULL;

    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &mgr->tanks[i];
        if ((tank->flags & PZ_TANK_FLAG_ACTIVE)
            && (tank->flags & PZ_TANK_FLAG_PLAYER)) {
            return tank;
        }
    }
    return NULL;
}

void
pz_tank_foreach(pz_tank_manager *mgr, pz_tank_iter_fn fn, void *user_data)
{
    if (!mgr || !fn)
        return;

    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &mgr->tanks[i];
        if (tank->flags & PZ_TANK_FLAG_ACTIVE) {
            fn(tank, user_data);
        }
    }
}

/* ============================================================================
 * Tank Update
 * ============================================================================
 */

// Normalize angle to [-PI, PI]
static float
normalize_angle(float angle)
{
    while (angle > PZ_PI)
        angle -= 2.0f * PZ_PI;
    while (angle < -PZ_PI)
        angle += 2.0f * PZ_PI;
    return angle;
}

void
pz_tank_update(pz_tank_manager *mgr, pz_tank *tank, const pz_tank_input *input,
    const pz_map *map, float dt)
{
    if (!mgr || !tank || !input)
        return;

    // Skip dead tanks (they have their own update logic)
    if (tank->flags & PZ_TANK_FLAG_DEAD)
        return;

    // Update damage flash
    if (tank->damage_flash > 0.0f) {
        tank->damage_flash -= dt;
        if (tank->damage_flash < 0.0f)
            tank->damage_flash = 0.0f;
    }

    // Update fire cooldown
    if (tank->fire_cooldown > 0.0f) {
        tank->fire_cooldown -= dt;
        if (tank->fire_cooldown < 0.0f)
            tank->fire_cooldown = 0.0f;
    }

    // Get terrain properties at tank position
    float terrain_speed_mult = 1.0f;
    float terrain_friction = 1.0f;
    if (map) {
        terrain_speed_mult = pz_map_get_speed_multiplier(map, tank->pos);
        terrain_friction = pz_map_get_friction(map, tank->pos);
    }

    // Apply acceleration in input direction
    if (pz_vec2_len_sq(input->move_dir) > 0.0f) {
        pz_vec2 dir = pz_vec2_normalize(input->move_dir);
        tank->vel = pz_vec2_add(tank->vel, pz_vec2_scale(dir, mgr->accel * dt));

        // Rotate body towards movement direction
        float target_angle = atan2f(dir.x, dir.y);
        float angle_diff = normalize_angle(target_angle - tank->body_angle);
        tank->body_angle
            += angle_diff * pz_minf(1.0f, mgr->body_turn_speed * dt);
    }

    // Apply friction (scaled by terrain friction - higher friction = faster
    // stop)
    float speed = pz_vec2_len(tank->vel);
    if (speed > 0.0f) {
        float friction_amount = mgr->friction * terrain_friction * dt;
        if (friction_amount > speed)
            friction_amount = speed;
        tank->vel = pz_vec2_sub(tank->vel,
            pz_vec2_scale(pz_vec2_normalize(tank->vel), friction_amount));
    }

    // Clamp to max speed (scaled by terrain speed multiplier)
    float effective_max_speed = mgr->max_speed * terrain_speed_mult;
    speed = pz_vec2_len(tank->vel);
    if (speed > effective_max_speed) {
        tank->vel
            = pz_vec2_scale(pz_vec2_normalize(tank->vel), effective_max_speed);
    }

    // Update position
    pz_vec2 new_pos = pz_vec2_add(tank->pos, pz_vec2_scale(tank->vel, dt));

    // Wall collision (separate axis) using circle-vs-tile checks
    if (map) {
        float r = mgr->collision_radius;
        pz_vec2 pos = tank->pos;

        pz_vec2 test_x = { new_pos.x, pos.y };
        if (!tank_circle_hits_map(map, test_x, r)) {
            pos.x = new_pos.x;
        } else {
            tank->vel.x = 0;
        }

        pz_vec2 test_y = { pos.x, new_pos.y };
        if (!tank_circle_hits_map(map, test_y, r)) {
            pos.y = new_pos.y;
        } else {
            tank->vel.y = 0;
        }

        resolve_tank_circle_map(map, &pos, r);
        tank->pos = pos;
    } else {
        tank->pos = new_pos;
    }

    // Turret rotation (smooth interpolation toward target)
    float turret_diff
        = normalize_angle(input->target_turret - tank->turret_angle);
    tank->turret_angle
        += turret_diff * pz_minf(1.0f, mgr->turret_turn_speed * dt);
}

void
pz_tank_update_all(pz_tank_manager *mgr, const pz_map *map, float dt)
{
    if (!mgr)
        return;

    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &mgr->tanks[i];
        if (!(tank->flags & PZ_TANK_FLAG_ACTIVE))
            continue;

        // Handle dead tanks (respawn timer for players only)
        if (tank->flags & PZ_TANK_FLAG_DEAD) {
            // Only player tanks respawn
            if (tank->flags & PZ_TANK_FLAG_PLAYER) {
                tank->respawn_timer -= dt;
                if (tank->respawn_timer <= 0.0f) {
                    pz_tank_respawn(tank);
                }
            }
            // Non-player tanks stay dead (but remain active for cleanup)
            continue;
        }

        // Update invulnerability timer
        if (tank->invuln_timer > 0.0f) {
            tank->invuln_timer -= dt;
            if (tank->invuln_timer <= 0.0f) {
                tank->invuln_timer = 0.0f;
                tank->flags &= ~PZ_TANK_FLAG_INVULNERABLE;
            }
        }

        // Update damage flash for all tanks
        if (tank->damage_flash > 0.0f) {
            tank->damage_flash -= dt;
            if (tank->damage_flash < 0.0f)
                tank->damage_flash = 0.0f;
        }

        // Decay visual recoil (spring-like decay)
        if (tank->recoil > 0.001f) {
            tank->recoil *= expf(-8.0f * dt);
        } else {
            tank->recoil = 0.0f;
        }

        // Fire cooldown
        if (tank->fire_cooldown > 0.0f) {
            tank->fire_cooldown -= dt;
            if (tank->fire_cooldown < 0.0f)
                tank->fire_cooldown = 0.0f;
        }

        // TODO: AI input for non-player tanks would go here
    }
}

/* ============================================================================
 * Combat
 * ============================================================================
 */

bool
pz_tank_damage(pz_tank *tank, int amount)
{
    if (!tank)
        return false;

    // Ignore if dead or invulnerable
    if ((tank->flags & PZ_TANK_FLAG_DEAD)
        || (tank->flags & PZ_TANK_FLAG_INVULNERABLE)) {
        return false;
    }

    tank->health -= amount;
    tank->damage_flash = DAMAGE_FLASH_DURATION;

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Tank %d took %d damage, health=%d",
        tank->id, amount, tank->health);

    if (tank->health <= 0) {
        tank->health = 0;
        tank->flags |= PZ_TANK_FLAG_DEAD;
        tank->respawn_timer = RESPAWN_DELAY;

        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Tank %d destroyed!", tank->id);
        return true;
    }

    return false;
}

bool
pz_tank_apply_damage(pz_tank_manager *mgr, pz_tank *tank, int amount)
{
    if (!mgr || !tank)
        return false;

    bool killed = pz_tank_damage(tank, amount);

    if (killed) {
        // Record death event
        if (mgr->death_event_count < PZ_MAX_DEATH_EVENTS) {
            pz_tank_death_event *event
                = &mgr->death_events[mgr->death_event_count++];
            event->tank_id = tank->id;
            event->pos = tank->pos;
            event->is_player = (tank->flags & PZ_TANK_FLAG_PLAYER) != 0;
        }
    }

    return killed;
}

pz_tank *
pz_tank_check_collision(
    pz_tank_manager *mgr, pz_vec2 pos, float radius, int exclude_id)
{
    if (!mgr)
        return NULL;

    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &mgr->tanks[i];

        // Skip inactive or dead tanks
        if (!(tank->flags & PZ_TANK_FLAG_ACTIVE))
            continue;
        if (tank->flags & PZ_TANK_FLAG_DEAD)
            continue;

        // Skip excluded tank (projectile owner)
        if (tank->id == exclude_id)
            continue;

        // Circle-circle collision
        float dist = pz_vec2_dist(pos, tank->pos);
        float combined_radius = radius + mgr->collision_radius;

        if (dist < combined_radius) {
            return tank;
        }
    }

    return NULL;
}

void
pz_tank_respawn(pz_tank *tank)
{
    if (!tank)
        return;

    tank->flags &= ~PZ_TANK_FLAG_DEAD;
    tank->flags |= PZ_TANK_FLAG_INVULNERABLE;

    tank->pos = tank->spawn_pos;
    tank->vel = (pz_vec2) { 0.0f, 0.0f };
    tank->health = tank->max_health;
    tank->respawn_timer = 0.0f;
    tank->invuln_timer = INVULN_DURATION;
    tank->damage_flash = 0.0f;
    tank->recoil = 0.0f;
    tank->fog_timer = 0.0f;
    tank->idle_time = 0.0f;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Tank %d respawned at (%.2f, %.2f)",
        tank->id, tank->spawn_pos.x, tank->spawn_pos.y);
}

/* ============================================================================
 * Weapon Loadout
 * ============================================================================
 */

bool
pz_tank_add_weapon(pz_tank *tank, int weapon_type)
{
    if (!tank)
        return false;

    // Check if already in loadout
    for (int i = 0; i < tank->loadout_count; i++) {
        if (tank->loadout[i] == weapon_type) {
            // Already have this weapon, switch to it
            tank->loadout_index = i;
            update_turret_color(tank);
            return false;
        }
    }

    // Add to loadout if there's room
    if (tank->loadout_count >= PZ_MAX_LOADOUT_WEAPONS) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "Loadout full, cannot add weapon");
        return false;
    }

    tank->loadout[tank->loadout_count] = weapon_type;
    tank->loadout_index = tank->loadout_count; // Switch to new weapon
    tank->loadout_count++;

    update_turret_color(tank);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
        "Added weapon to loadout, now have %d weapons", tank->loadout_count);

    return true;
}

void
pz_tank_cycle_weapon(pz_tank *tank, int scroll_delta)
{
    if (!tank || tank->loadout_count <= 1)
        return;

    // Cycle through loadout
    tank->loadout_index += scroll_delta;

    // Wrap around
    if (tank->loadout_index < 0) {
        tank->loadout_index = tank->loadout_count - 1;
    } else if (tank->loadout_index >= tank->loadout_count) {
        tank->loadout_index = 0;
    }

    update_turret_color(tank);

    int weapon = tank->loadout[tank->loadout_index];
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Switched to weapon %d (%s)",
        tank->loadout_index, pz_powerup_type_name((pz_powerup_type)weapon));
}

int
pz_tank_get_current_weapon(const pz_tank *tank)
{
    if (!tank || tank->loadout_count == 0)
        return PZ_POWERUP_NONE;

    return tank->loadout[tank->loadout_index];
}

void
pz_tank_reset_loadout(pz_tank *tank)
{
    if (!tank)
        return;

    // Reset to default weapon only
    tank->loadout[0] = PZ_POWERUP_NONE;
    tank->loadout_count = 1;
    tank->loadout_index = 0;

    update_turret_color(tank);

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Tank %d loadout reset to default",
        tank->id);
}

/* ============================================================================
 * Rendering
 * ============================================================================
 */

void
pz_tank_render(pz_tank_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection, const pz_tank_render_params *params)
{
    if (!mgr || !renderer || !view_projection)
        return;

    if (!mgr->render_ready)
        return;

    // Light parameters for directional shading
    pz_vec3 light_dir = { 0.5f, 1.0f, 0.3f };
    pz_vec3 light_color = { 0.6f, 0.55f, 0.5f };
    pz_vec3 ambient = { 0.15f, 0.18f, 0.2f };
    pz_vec4 shadow_color = { 0.0f, 0.0f, 0.0f, SHADOW_ALPHA };

    // Set shared uniforms
    pz_renderer_set_uniform_vec3(
        renderer, mgr->shader, "u_light_dir", light_dir);
    pz_renderer_set_uniform_vec3(
        renderer, mgr->shader, "u_light_color", light_color);
    pz_renderer_set_uniform_vec3(renderer, mgr->shader, "u_ambient", ambient);
    pz_renderer_set_uniform_vec2(
        renderer, mgr->shader, "u_shadow_params", (pz_vec2) { 0.0f, 0.0f });

    // Set light map uniforms
    if (params && params->light_texture != PZ_INVALID_HANDLE
        && params->light_texture != 0) {
        pz_renderer_bind_texture(renderer, 0, params->light_texture);
        pz_renderer_set_uniform_int(
            renderer, mgr->shader, "u_light_texture", 0);
        pz_renderer_set_uniform_int(renderer, mgr->shader, "u_use_lighting", 1);
        pz_renderer_set_uniform_vec2(renderer, mgr->shader, "u_light_scale",
            (pz_vec2) { params->light_scale_x, params->light_scale_z });
        pz_renderer_set_uniform_vec2(renderer, mgr->shader, "u_light_offset",
            (pz_vec2) { params->light_offset_x, params->light_offset_z });
    } else {
        pz_renderer_set_uniform_int(renderer, mgr->shader, "u_use_lighting", 0);
    }

    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &mgr->tanks[i];

        // Skip inactive or dead tanks
        if (!(tank->flags & PZ_TANK_FLAG_ACTIVE))
            continue;
        if (tank->flags & PZ_TANK_FLAG_DEAD)
            continue;

        // Calculate colors (with damage flash)
        pz_vec4 body_color = tank->body_color;
        pz_vec4 turret_color = tank->turret_color;

        if (tank->damage_flash > 0.0f) {
            // Flash to white when damaged
            float flash_t = tank->damage_flash / DAMAGE_FLASH_DURATION;
            body_color.x = pz_lerpf(body_color.x, 1.0f, flash_t);
            body_color.y = pz_lerpf(body_color.y, 1.0f, flash_t);
            body_color.z = pz_lerpf(body_color.z, 1.0f, flash_t);
            turret_color.x = pz_lerpf(turret_color.x, 1.0f, flash_t);
            turret_color.y = pz_lerpf(turret_color.y, 1.0f, flash_t);
            turret_color.z = pz_lerpf(turret_color.z, 1.0f, flash_t);
        }

        if (mgr->shadow_pipeline != PZ_INVALID_HANDLE && mgr->shadow_mesh
            && mgr->shadow_mesh->uploaded) {
            pz_renderer_set_uniform_vec2(renderer, mgr->shader,
                "u_shadow_params", (pz_vec2) { SHADOW_SOFTNESS, 1.0f });
            pz_mat4 shadow_model = pz_mat4_identity();
            shadow_model = pz_mat4_mul(shadow_model,
                pz_mat4_translate(
                    (pz_vec3) { tank->pos.x, SHADOW_Y_OFFSET, tank->pos.y }));
            shadow_model
                = pz_mat4_mul(shadow_model, pz_mat4_rotate_y(tank->body_angle));

            pz_mat4 shadow_mvp = pz_mat4_mul(*view_projection, shadow_model);

            pz_renderer_set_uniform_mat4(
                renderer, mgr->shader, "u_mvp", &shadow_mvp);
            pz_renderer_set_uniform_mat4(
                renderer, mgr->shader, "u_model", &shadow_model);
            pz_renderer_set_uniform_vec4(
                renderer, mgr->shader, "u_color", shadow_color);

            pz_draw_cmd shadow_cmd = {
                .pipeline = mgr->shadow_pipeline,
                .vertex_buffer = mgr->shadow_mesh->buffer,
                .index_buffer = PZ_INVALID_HANDLE,
                .vertex_count = mgr->shadow_mesh->vertex_count,
                .index_count = 0,
                .vertex_offset = 0,
                .index_offset = 0,
            };
            pz_renderer_draw(renderer, &shadow_cmd);
        }

        pz_renderer_set_uniform_vec2(
            renderer, mgr->shader, "u_shadow_params", (pz_vec2) { 0.0f, 0.0f });

        // Calculate visual recoil offset (pushes backward from turret
        // direction)
        float recoil_scale = 0.25f; // How far the tank slides back visually
        float recoil_x
            = -sinf(tank->turret_angle) * tank->recoil * recoil_scale;
        float recoil_z
            = -cosf(tank->turret_angle) * tank->recoil * recoil_scale;

        // Draw body (with recoil offset)
        float body_recoil = 0.4f; // Body moves less than turret
        pz_mat4 body_model = pz_mat4_identity();
        body_model = pz_mat4_mul(body_model,
            pz_mat4_translate((pz_vec3) { tank->pos.x + recoil_x * body_recoil,
                0.0f, tank->pos.y + recoil_z * body_recoil }));
        body_model
            = pz_mat4_mul(body_model, pz_mat4_rotate_y(tank->body_angle));

        pz_mat4 body_mvp = pz_mat4_mul(*view_projection, body_model);

        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_mvp", &body_mvp);
        pz_renderer_set_uniform_mat4(
            renderer, mgr->shader, "u_model", &body_model);
        pz_renderer_set_uniform_vec4(
            renderer, mgr->shader, "u_color", body_color);

        pz_draw_cmd body_cmd = {
            .pipeline = mgr->pipeline,
            .vertex_buffer = mgr->body_mesh->buffer,
            .index_buffer = PZ_INVALID_HANDLE,
            .vertex_count = mgr->body_mesh->vertex_count,
            .index_count = 0,
            .vertex_offset = 0,
            .index_offset = 0,
        };
        pz_renderer_draw(renderer, &body_cmd);

        // Draw turret (with full recoil offset)
        pz_mat4 turret_model = pz_mat4_identity();
        turret_model = pz_mat4_mul(turret_model,
            pz_mat4_translate((pz_vec3) { tank->pos.x + recoil_x,
                TURRET_Y_OFFSET, tank->pos.y + recoil_z }));
        turret_model
            = pz_mat4_mul(turret_model, pz_mat4_rotate_y(tank->turret_angle));

        pz_mat4 turret_mvp = pz_mat4_mul(*view_projection, turret_model);

        pz_renderer_set_uniform_mat4(
            renderer, mgr->shader, "u_mvp", &turret_mvp);
        pz_renderer_set_uniform_mat4(
            renderer, mgr->shader, "u_model", &turret_model);
        pz_renderer_set_uniform_vec4(
            renderer, mgr->shader, "u_color", turret_color);

        pz_draw_cmd turret_cmd = {
            .pipeline = mgr->pipeline,
            .vertex_buffer = mgr->turret_mesh->buffer,
            .index_buffer = PZ_INVALID_HANDLE,
            .vertex_count = mgr->turret_mesh->vertex_count,
            .index_count = 0,
            .vertex_offset = 0,
            .index_offset = 0,
        };
        pz_renderer_draw(renderer, &turret_cmd);
    }
}

/* ============================================================================
 * Utility
 * ============================================================================
 */

pz_vec2
pz_tank_get_barrel_tip(const pz_tank *tank)
{
    if (!tank)
        return (pz_vec2) { 0.0f, 0.0f };

    float dx = sinf(tank->turret_angle) * BARREL_LENGTH;
    float dz = cosf(tank->turret_angle) * BARREL_LENGTH;

    return (pz_vec2) {
        tank->pos.x + dx,
        tank->pos.y + dz,
    };
}

pz_vec2
pz_tank_get_fire_direction(const pz_tank *tank)
{
    if (!tank)
        return (pz_vec2) { 0.0f, 1.0f };

    return (pz_vec2) {
        sinf(tank->turret_angle),
        cosf(tank->turret_angle),
    };
}

bool
pz_tank_get_fire_solution(const pz_tank *tank, const pz_map *map,
    pz_vec2 *out_pos, pz_vec2 *out_dir, int *out_bounce_cost)
{
    if (!tank) {
        return false;
    }

    pz_vec2 fire_dir = pz_tank_get_fire_direction(tank);
    pz_vec2 tip = pz_tank_get_barrel_tip(tank);

    if (out_pos) {
        *out_pos = tip;
    }
    if (out_dir) {
        *out_dir = fire_dir;
    }
    if (out_bounce_cost) {
        *out_bounce_cost = 0;
    }

    if (!map) {
        return true;
    }

    float barrel_len = pz_vec2_dist(tank->pos, tip);
    if (barrel_len < 0.0001f) {
        return true;
    }

    pz_raycast_result hit = pz_map_raycast_ex(map, tank->pos, tip);
    if (hit.hit && hit.distance < (barrel_len - BARREL_CLEAR_EPSILON)) {
        if (out_pos) {
            *out_pos = pz_vec2_add(
                hit.point, pz_vec2_scale(hit.normal, BARREL_DEFLECT_EPSILON));
        }
        if (out_dir) {
            *out_dir = pz_vec2_reflect(fire_dir, hit.normal);
        }
        if (out_bounce_cost) {
            *out_bounce_cost = 1;
        }
    }

    return true;
}

bool
pz_tank_barrel_is_clear(const pz_tank *tank, const pz_map *map)
{
    if (!tank || !map) {
        return true;
    }

    pz_vec2 tip = pz_tank_get_barrel_tip(tank);
    float barrel_len = pz_vec2_dist(tank->pos, tip);
    if (barrel_len < 0.0001f) {
        return true;
    }

    pz_raycast_result hit = pz_map_raycast_ex(map, tank->pos, tip);
    if (hit.hit && hit.distance < (barrel_len - BARREL_CLEAR_EPSILON)) {
        return false;
    }

    return true;
}

int
pz_tank_count_active(const pz_tank_manager *mgr)
{
    if (!mgr)
        return 0;

    int count = 0;
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        if ((mgr->tanks[i].flags & PZ_TANK_FLAG_ACTIVE)
            && !(mgr->tanks[i].flags & PZ_TANK_FLAG_DEAD)) {
            count++;
        }
    }
    return count;
}

int
pz_tank_count_enemies_alive(const pz_tank_manager *mgr)
{
    if (!mgr)
        return 0;

    int count = 0;
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        const pz_tank *tank = &mgr->tanks[i];
        if ((tank->flags & PZ_TANK_FLAG_ACTIVE)
            && !(tank->flags & PZ_TANK_FLAG_DEAD)
            && !(tank->flags & PZ_TANK_FLAG_PLAYER)) {
            count++;
        }
    }
    return count;
}

int
pz_tank_get_death_events(
    const pz_tank_manager *mgr, pz_tank_death_event *events, int max_events)
{
    if (!mgr || !events || max_events <= 0)
        return 0;

    int count = mgr->death_event_count;
    if (count > max_events)
        count = max_events;

    for (int i = 0; i < count; i++) {
        events[i] = mgr->death_events[i];
    }

    return count;
}

void
pz_tank_clear_death_events(pz_tank_manager *mgr)
{
    if (mgr) {
        mgr->death_event_count = 0;
    }
}
