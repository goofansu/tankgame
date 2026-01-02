/*
 * Tank Game - Enemy AI System Implementation
 */

#include "pz_ai.h"

#include <math.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "pz_map.h"
#include "pz_powerup.h"
#include "pz_projectile.h"

/* ============================================================================
 * Enemy Stats
 * ============================================================================
 */

static const pz_enemy_stats ENEMY_STATS[] = {
    // Level 0 (unused, placeholder)
    { .health = 10,
        .max_bounces = 1,
        .fire_cooldown = 1.0f,
        .aim_speed = 1.0f,
        .body_color = { 0.5f, 0.5f, 0.5f, 1.0f } },

    // Level 1: Basic enemy
    { .health = 10,
        .max_bounces = 1,
        .fire_cooldown = 1.2f,
        .aim_speed = 1.0f,
        .body_color = { 0.6f, 0.25f, 0.25f, 1.0f } }, // Dark red

    // Level 2: Intermediate enemy
    { .health = 15,
        .max_bounces = 1,
        .fire_cooldown = 0.8f,
        .aim_speed = 1.3f,
        .body_color = { 0.7f, 0.4f, 0.1f, 1.0f } }, // Orange-brown

    // Level 3: Advanced enemy
    { .health = 20,
        .max_bounces = 2,
        .fire_cooldown = 0.6f,
        .aim_speed = 1.6f,
        .body_color = { 0.4f, 0.1f, 0.4f, 1.0f } }, // Purple
};

const pz_enemy_stats *
pz_enemy_get_stats(pz_enemy_level level)
{
    if (level < 1 || level > 3) {
        return &ENEMY_STATS[1]; // Default to level 1
    }
    return &ENEMY_STATS[level];
}

/* ============================================================================
 * AI Manager
 * ============================================================================
 */

pz_ai_manager *
pz_ai_manager_create(pz_tank_manager *tank_mgr, const pz_map *map)
{
    pz_ai_manager *ai = pz_calloc(1, sizeof(pz_ai_manager));
    if (!ai) {
        return NULL;
    }

    ai->tank_mgr = tank_mgr;
    ai->map = map;
    ai->controller_count = 0;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "AI manager created");
    return ai;
}

void
pz_ai_manager_destroy(pz_ai_manager *ai_mgr)
{
    if (!ai_mgr) {
        return;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "AI manager destroyed");
    pz_free(ai_mgr);
}

void
pz_ai_manager_set_map(pz_ai_manager *ai_mgr, const pz_map *map)
{
    if (ai_mgr) {
        ai_mgr->map = map;
    }
}

/* ============================================================================
 * Enemy Spawning
 * ============================================================================
 */

pz_tank *
pz_ai_spawn_enemy(
    pz_ai_manager *ai_mgr, pz_vec2 pos, float angle, pz_enemy_level level)
{
    if (!ai_mgr || !ai_mgr->tank_mgr) {
        return NULL;
    }

    // Check if we have room for another AI controller
    if (ai_mgr->controller_count >= PZ_MAX_AI_CONTROLLERS) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Cannot spawn enemy: max AI controllers reached (%d)",
            PZ_MAX_AI_CONTROLLERS);
        return NULL;
    }

    // Get enemy stats
    const pz_enemy_stats *stats = pz_enemy_get_stats(level);

    // Spawn the tank (not a player)
    pz_tank *tank
        = pz_tank_spawn(ai_mgr->tank_mgr, pos, stats->body_color, false);
    if (!tank) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Cannot spawn enemy: no tank slots available");
        return NULL;
    }

    // Set tank properties based on enemy level
    tank->health = stats->health;
    tank->max_health = stats->health;
    tank->body_angle = angle;
    tank->turret_angle = angle;

    // Create AI controller
    pz_ai_controller *ctrl = &ai_mgr->controllers[ai_mgr->controller_count++];
    memset(ctrl, 0, sizeof(pz_ai_controller));

    ctrl->tank_id = tank->id;
    ctrl->level = level;
    ctrl->current_aim_angle = angle;
    ctrl->target_aim_angle = angle;
    ctrl->fire_timer
        = stats->fire_cooldown; // Start with a delay before first shot
    ctrl->can_see_player = false;
    ctrl->reaction_delay = 0.0f;
    ctrl->last_seen_time = 0.0f;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
        "Spawned Level %d enemy at (%.1f, %.1f), tank_id=%d", level, pos.x,
        pos.y, tank->id);

    return tank;
}

/* ============================================================================
 * AI Update
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

// Check line-of-sight from AI to player using map raycast
static bool
check_line_of_sight(const pz_map *map, pz_vec2 from, pz_vec2 to)
{
    if (!map) {
        return true; // No map = assume can see
    }

    pz_vec2 dir = pz_vec2_sub(to, from);
    float dist = pz_vec2_len(dir);

    if (dist < 0.1f) {
        return true; // Very close = can see
    }

    dir = pz_vec2_scale(dir, 1.0f / dist); // Normalize

    // Raycast from AI to player
    bool hit_wall = false;
    pz_vec2 hit_pos = pz_map_raycast(map, from, dir, dist, &hit_wall);
    (void)hit_pos;

    // If we hit a wall before reaching player distance, no LOS
    return !hit_wall;
}

void
pz_ai_update(pz_ai_manager *ai_mgr, pz_vec2 player_pos, float dt)
{
    if (!ai_mgr || !ai_mgr->tank_mgr) {
        return;
    }

    for (int i = 0; i < ai_mgr->controller_count; i++) {
        pz_ai_controller *ctrl = &ai_mgr->controllers[i];

        // Get the tank this AI controls
        pz_tank *tank = pz_tank_get_by_id(ai_mgr->tank_mgr, ctrl->tank_id);
        if (!tank) {
            continue; // Tank doesn't exist (destroyed)
        }

        // Skip dead tanks
        if (tank->flags & PZ_TANK_FLAG_DEAD) {
            continue;
        }

        // Get enemy stats for behavior parameters
        const pz_enemy_stats *stats = pz_enemy_get_stats(ctrl->level);

        // Calculate angle to player
        float dx = player_pos.x - tank->pos.x;
        float dy = player_pos.y - tank->pos.y;
        float target_angle = atan2f(dx, dy);

        // AI has full information (top-down view) - always tracks and aims at
        // player, but only fires when there's clear line-of-sight
        ctrl->target_aim_angle = target_angle;
        ctrl->can_see_player
            = check_line_of_sight(ai_mgr->map, tank->pos, player_pos);

        // Smoothly rotate turret towards target
        // This simulates the AI having to physically turn the turret like a
        // player would
        float angle_diff
            = normalize_angle(ctrl->target_aim_angle - ctrl->current_aim_angle);
        float turret_speed
            = 5.0f * stats->aim_speed; // Base turret speed * level multiplier
        float max_rotation = turret_speed * dt;

        if (fabsf(angle_diff) <= max_rotation) {
            ctrl->current_aim_angle = ctrl->target_aim_angle;
        } else if (angle_diff > 0) {
            ctrl->current_aim_angle += max_rotation;
        } else {
            ctrl->current_aim_angle -= max_rotation;
        }

        ctrl->current_aim_angle = normalize_angle(ctrl->current_aim_angle);

        // Build input for this tank
        // AI tanks are currently stationary (turret only)
        pz_tank_input input = {
            .move_dir = { 0.0f, 0.0f }, // No movement
            .target_turret = ctrl->current_aim_angle,
            .fire = false, // Firing handled separately
        };

        // Update the tank with this input
        pz_tank_update(ai_mgr->tank_mgr, tank, &input, ai_mgr->map, dt);

        // Update fire timer
        if (ctrl->fire_timer > 0.0f) {
            ctrl->fire_timer -= dt;
        }
    }
}

/* ============================================================================
 * AI Firing
 * ============================================================================
 */

int
pz_ai_fire(pz_ai_manager *ai_mgr, pz_projectile_manager *proj_mgr)
{
    if (!ai_mgr || !ai_mgr->tank_mgr || !proj_mgr) {
        return 0;
    }

    int fired = 0;

    for (int i = 0; i < ai_mgr->controller_count; i++) {
        pz_ai_controller *ctrl = &ai_mgr->controllers[i];

        // Get the tank
        pz_tank *tank = pz_tank_get_by_id(ai_mgr->tank_mgr, ctrl->tank_id);
        if (!tank || (tank->flags & PZ_TANK_FLAG_DEAD)) {
            continue;
        }

        // Only fire if:
        // 1. We can see the player
        // 2. Fire timer has elapsed
        // 3. Turret is roughly aimed at target (within ~15 degrees)
        if (!ctrl->can_see_player) {
            continue;
        }

        if (ctrl->fire_timer > 0.0f) {
            continue;
        }

        float aim_error = fabsf(
            normalize_angle(ctrl->target_aim_angle - ctrl->current_aim_angle));
        if (aim_error > 0.26f) { // ~15 degrees
            continue;
        }

        // Get enemy stats for weapon properties
        const pz_enemy_stats *stats = pz_enemy_get_stats(ctrl->level);

        // Get weapon stats (default weapon for now)
        const pz_weapon_stats *weapon = pz_weapon_get_stats(PZ_POWERUP_NONE);

        // Check active projectile count
        int active = pz_projectile_count_by_owner(proj_mgr, tank->id);
        if (active >= weapon->max_active_projectiles) {
            continue;
        }

        // Fire!
        pz_vec2 spawn_pos = pz_tank_get_barrel_tip(tank);
        pz_vec2 fire_dir = pz_tank_get_fire_direction(tank);

        pz_projectile_config proj_config = {
            .speed = weapon->projectile_speed,
            .max_bounces = stats->max_bounces, // Use enemy's bounce count
            .lifetime = -1.0f,
            .damage = weapon->damage,
            .scale = weapon->projectile_scale,
            .color = weapon->projectile_color,
        };

        pz_projectile_spawn(
            proj_mgr, spawn_pos, fire_dir, &proj_config, tank->id);

        // Reset fire timer
        ctrl->fire_timer = stats->fire_cooldown;
        fired++;

        pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "AI tank %d fired (level %d)",
            tank->id, ctrl->level);
    }

    return fired;
}

/* ============================================================================
 * AI Queries
 * ============================================================================
 */

int
pz_ai_count_alive(const pz_ai_manager *ai_mgr)
{
    if (!ai_mgr || !ai_mgr->tank_mgr) {
        return 0;
    }

    int count = 0;

    for (int i = 0; i < ai_mgr->controller_count; i++) {
        const pz_ai_controller *ctrl = &ai_mgr->controllers[i];
        pz_tank *tank = pz_tank_get_by_id(ai_mgr->tank_mgr, ctrl->tank_id);

        if (tank && !(tank->flags & PZ_TANK_FLAG_DEAD)) {
            count++;
        }
    }

    return count;
}

bool
pz_ai_is_controlled(const pz_ai_manager *ai_mgr, int tank_id)
{
    if (!ai_mgr) {
        return false;
    }

    for (int i = 0; i < ai_mgr->controller_count; i++) {
        if (ai_mgr->controllers[i].tank_id == tank_id) {
            return true;
        }
    }

    return false;
}

pz_ai_controller *
pz_ai_get_controller(pz_ai_manager *ai_mgr, int tank_id)
{
    if (!ai_mgr) {
        return NULL;
    }

    for (int i = 0; i < ai_mgr->controller_count; i++) {
        if (ai_mgr->controllers[i].tank_id == tank_id) {
            return &ai_mgr->controllers[i];
        }
    }

    return NULL;
}
