/*
 * Tank Game - Enemy AI System Implementation
 */

#include "pz_ai.h"

#include <math.h>
#include <stdlib.h>
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
        .body_color = { 0.5f, 0.5f, 0.5f, 1.0f },
        .weapon_type = PZ_POWERUP_NONE,
        .projectile_speed_scale = 1.0f,
        .bounce_shot_range = 30.0f },

    // Level 1: Sentry (stationary turret, fires often, uses bounce shots)
    { .health = 10,
        .max_bounces = 1,
        .fire_cooldown = 0.6f,
        .aim_speed = 1.2f,
        .body_color = { 0.6f, 0.25f, 0.25f, 1.0f }, // Dark red
        .weapon_type = PZ_POWERUP_NONE,
        .projectile_speed_scale = 1.0f,
        .bounce_shot_range = 30.0f },

    // Level 2: Skirmisher (uses cover)
    { .health = 15,
        .max_bounces = 1,
        .fire_cooldown = 0.8f,
        .aim_speed = 1.3f,
        .body_color = { 0.7f, 0.4f, 0.1f, 1.0f }, // Orange-brown
        .weapon_type = PZ_POWERUP_NONE,
        .projectile_speed_scale = 1.0f,
        .bounce_shot_range = 0.0f },

    // Level 3: Hunter (aggressive, machine gun burst)
    { .health = 20,
        .max_bounces = 0,
        .fire_cooldown = 0.2f,
        .aim_speed = 2.0f,
        .body_color = { 0.2f, 0.5f, 0.2f, 1.0f }, // Dark green (hunter)
        .weapon_type = PZ_POWERUP_MACHINE_GUN,
        .projectile_speed_scale = 1.0f,
        .bounce_shot_range = 0.0f },

    // Level 4: Sniper (stationary, long-range ricochet)
    { .health = 12,
        .max_bounces = 3,
        .fire_cooldown = 1.1f,
        .aim_speed = 0.9f,
        .body_color = { 0.35f, 0.4f, 0.7f, 1.0f }, // Steel blue
        .weapon_type = PZ_POWERUP_RICOCHET,
        .projectile_speed_scale = 1.4f,
        .bounce_shot_range = 60.0f },
};

const pz_enemy_stats *
pz_enemy_get_stats(pz_enemy_level level)
{
    if (level < 1 || level > PZ_ENEMY_LEVEL_SNIPER) {
        return &ENEMY_STATS[1]; // Default to level 1
    }
    return &ENEMY_STATS[level];
}

const char *
pz_enemy_level_name(pz_enemy_level level)
{
    switch (level) {
    case PZ_ENEMY_LEVEL_1:
        return "sentry";
    case PZ_ENEMY_LEVEL_2:
        return "skirmisher";
    case PZ_ENEMY_LEVEL_3:
        return "hunter";
    case PZ_ENEMY_LEVEL_SNIPER:
        return "sniper";
    default:
        return "sentry";
    }
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
    if (stats->weapon_type != PZ_POWERUP_NONE) {
        pz_tank_add_weapon(tank, (int)stats->weapon_type);
    }

    // Create AI controller
    pz_ai_controller *ctrl = &ai_mgr->controllers[ai_mgr->controller_count++];
    memset(ctrl, 0, sizeof(pz_ai_controller));

    ctrl->tank_id = tank->id;
    ctrl->level = level;
    ctrl->current_aim_angle = angle;
    ctrl->target_aim_angle = angle;
    // Match player behavior: allow immediate fire, then use weapon cooldown.
    ctrl->fire_timer = 0.0f;
    ctrl->can_see_player = false;
    ctrl->reaction_delay = 0.0f;
    ctrl->last_seen_time = 0.0f;

    // Initialize cover behavior for level 2+
    ctrl->state = PZ_AI_STATE_IDLE;
    ctrl->cover_pos = pos;
    ctrl->peek_pos = pos;
    ctrl->move_target = pos;
    ctrl->state_timer = 0.0f;
    ctrl->cover_search_timer = 0.0f;
    ctrl->has_cover = false;
    ctrl->shots_fired = 0;
    if (level == PZ_ENEMY_LEVEL_2) {
        ctrl->max_shots_per_peek = 2;
    } else if (level == PZ_ENEMY_LEVEL_3) {
        ctrl->max_shots_per_peek = 3;
    } else {
        ctrl->max_shots_per_peek = 1;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
        "Spawned %s enemy at (%.1f, %.1f), tank_id=%d",
        pz_enemy_level_name(level), pos.x, pos.y, tank->id);

    return tank;
}

/* ============================================================================
 * AI Helpers
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

// Check if a position is valid (not inside a wall)
static bool
is_position_valid(const pz_map *map, pz_vec2 pos, float radius)
{
    if (!map) {
        return true;
    }

    // Check center and multiple points around the tank for robust collision
    if (pz_map_is_solid(map, pos)) {
        return false;
    }
    // Cardinal directions
    if (pz_map_is_solid(map, (pz_vec2) { pos.x + radius, pos.y })) {
        return false;
    }
    if (pz_map_is_solid(map, (pz_vec2) { pos.x - radius, pos.y })) {
        return false;
    }
    if (pz_map_is_solid(map, (pz_vec2) { pos.x, pos.y + radius })) {
        return false;
    }
    if (pz_map_is_solid(map, (pz_vec2) { pos.x, pos.y - radius })) {
        return false;
    }
    // Diagonal directions (corners)
    float diag = radius * 0.707f; // radius / sqrt(2)
    if (pz_map_is_solid(map, (pz_vec2) { pos.x + diag, pos.y + diag })) {
        return false;
    }
    if (pz_map_is_solid(map, (pz_vec2) { pos.x + diag, pos.y - diag })) {
        return false;
    }
    if (pz_map_is_solid(map, (pz_vec2) { pos.x - diag, pos.y + diag })) {
        return false;
    }
    if (pz_map_is_solid(map, (pz_vec2) { pos.x - diag, pos.y - diag })) {
        return false;
    }

    return true;
}

// Try to find a bounce shot angle to hit the player
// Returns true if a valid bounce shot was found, sets bounce_angle
// This simulates firing a projectile and checking if it would hit the player
// after bouncing off a wall.
static bool
find_bounce_shot(const pz_map *map, pz_vec2 ai_pos, pz_vec2 player_pos,
    float max_ray_dist, float *bounce_angle)
{
    if (!map) {
        return false;
    }

    const float player_hit_radius = 0.9f; // Tank collision radius
    const int num_angles = 36; // Sample 36 angles (every 10 degrees)

    float best_score = -1.0f;
    float best_angle = 0.0f;

    // Try firing in multiple directions and simulate one bounce
    for (int i = 0; i < num_angles; i++) {
        float angle = (float)i * (2.0f * PZ_PI / (float)num_angles);
        pz_vec2 dir = { sinf(angle), cosf(angle) };

        // Cast ray to find wall
        pz_vec2 end = pz_vec2_add(ai_pos, pz_vec2_scale(dir, max_ray_dist));
        pz_raycast_result ray = pz_map_raycast_ex(map, ai_pos, end);

        if (!ray.hit) {
            continue; // No wall hit, can't bounce
        }

        // Calculate reflected direction
        pz_vec2 reflected = pz_vec2_reflect(dir, ray.normal);

        // Push slightly off the wall
        pz_vec2 bounce_pos
            = pz_vec2_add(ray.point, pz_vec2_scale(ray.normal, 0.05f));

        // Cast ray from bounce point in reflected direction toward player
        float dist_to_player = pz_vec2_dist(bounce_pos, player_pos);
        pz_vec2 reflected_end
            = pz_vec2_add(bounce_pos, pz_vec2_scale(reflected, max_ray_dist));

        // Check if reflected ray passes close to player
        // Find closest approach of the reflected ray to the player

        // Vector from bounce point to player
        pz_vec2 to_player = pz_vec2_sub(player_pos, bounce_pos);

        // Project player position onto reflected ray
        float dot = pz_vec2_dot(to_player, reflected);

        if (dot < 0) {
            continue; // Player is behind the bounce direction
        }

        // Closest point on reflected ray to player
        pz_vec2 closest
            = pz_vec2_add(bounce_pos, pz_vec2_scale(reflected, dot));
        float miss_dist = pz_vec2_dist(closest, player_pos);

        // Check if the ray would hit a wall before reaching the player
        pz_vec2 check_end = pz_vec2_add(
            bounce_pos, pz_vec2_scale(reflected, dot + player_hit_radius));
        pz_raycast_result check = pz_map_raycast_ex(map, bounce_pos, check_end);

        if (check.hit && check.distance < dot - player_hit_radius) {
            continue; // Wall blocks the path to player
        }

        // Score this shot (lower miss distance is better)
        if (miss_dist < player_hit_radius * 1.5f) {
            // Calculate a score based on how close we'd get
            float score = player_hit_radius * 2.0f - miss_dist;

            // Prefer shorter total path
            float total_dist = ray.distance + dot;
            score -= total_dist * 0.01f;

            if (score > best_score) {
                best_score = score;
                best_angle = angle;
            }
        }
    }

    if (best_score > 0) {
        *bounce_angle = best_angle;
        return true;
    }

    return false;
}

// Find a cover position relative to the player
// Returns true if cover was found, sets cover_pos and peek_pos
// The AI will hide behind cover, then move out to peek and fire
static bool
find_cover_position(const pz_map *map, pz_vec2 ai_pos, pz_vec2 player_pos,
    pz_vec2 *cover_pos, pz_vec2 *peek_pos)
{
    if (!map) {
        return false;
    }

    const float tank_radius = 0.9f;
    const float standoff = 1.2f; // Distance from wall

    // Direction to player
    pz_vec2 to_player = pz_vec2_sub(player_pos, ai_pos);
    float dist_to_player = pz_vec2_len(to_player);
    if (dist_to_player < 0.1f) {
        return false;
    }
    pz_vec2 dir_to_player = pz_vec2_scale(to_player, 1.0f / dist_to_player);

    float best_score = -1.0f;
    pz_vec2 best_cover = ai_pos;
    pz_vec2 best_peek = ai_pos;

    // Sample positions in a grid around the AI to find good cover spots
    const float search_range = 10.0f;
    const float step = 1.0f;

    for (float dx = -search_range; dx <= search_range; dx += step) {
        for (float dy = -search_range; dy <= search_range; dy += step) {
            pz_vec2 test_cover = { ai_pos.x + dx, ai_pos.y + dy };

            // Skip if position is in a wall
            if (!is_position_valid(map, test_cover, tank_radius)) {
                continue;
            }

            // Skip if player can see this position (not good cover)
            if (check_line_of_sight(map, test_cover, player_pos)) {
                continue;
            }

            // This is a potential cover position - now find a peek position
            // The peek position should be closer to player and have LOS

            // Try stepping toward the player from cover
            for (float peek_step = 1.0f; peek_step <= 4.0f; peek_step += 0.5f) {
                pz_vec2 test_peek = pz_vec2_add(
                    test_cover, pz_vec2_scale(dir_to_player, peek_step));

                // Peek position must be valid
                if (!is_position_valid(map, test_peek, tank_radius)) {
                    continue;
                }

                // Peek position must have LOS to player
                if (!check_line_of_sight(map, test_peek, player_pos)) {
                    continue;
                }

                // Found a valid cover/peek pair!
                float score = 10.0f;

                // Prefer cover closer to AI
                float cover_dist = pz_vec2_dist(ai_pos, test_cover);
                score -= cover_dist * 0.3f;

                // Prefer shorter peek distance (less time exposed)
                score -= peek_step * 0.5f;

                // Prefer cover that's somewhat toward the player
                float toward
                    = pz_vec2_dot(pz_vec2_scale((pz_vec2) { dx, dy },
                                      1.0f / sqrtf(dx * dx + dy * dy + 0.01f)),
                        dir_to_player);
                if (toward > 0) {
                    score += toward * 3.0f;
                }

                if (score > best_score) {
                    best_score = score;
                    best_cover = test_cover;
                    best_peek = test_peek;
                }

                break; // Found a peek for this cover, try next cover
            }
        }
    }

    if (best_score > 0) {
        *cover_pos = best_cover;
        *peek_pos = best_peek;
        return true;
    }

    return false;
}

/* ============================================================================
 * Level 2 Cover AI Update
 * ============================================================================
 */

static void
update_level2_ai(pz_ai_controller *ctrl, pz_tank *tank,
    pz_tank_manager *tank_mgr, const pz_map *map, pz_vec2 player_pos, float dt)
{
    const float move_speed = 3.0f; // Movement speed for AI
    const float arrive_threshold = 0.5f; // How close to target to stop
    const float cover_wait_time = 1.5f; // Time to wait in cover before peeking
    const float firing_time = 2.0f; // Max time exposed while firing
    const float cover_search_cooldown = 3.0f; // Time between cover searches

    // Update cover search timer
    if (ctrl->cover_search_timer > 0.0f) {
        ctrl->cover_search_timer -= dt;
    }

    // State machine for cover behavior
    pz_vec2 move_dir = { 0.0f, 0.0f };

    switch (ctrl->state) {
    case PZ_AI_STATE_IDLE:
        // Initial state - look for cover
        if (!ctrl->has_cover && ctrl->cover_search_timer <= 0.0f) {
            if (find_cover_position(map, tank->pos, player_pos,
                    &ctrl->cover_pos, &ctrl->peek_pos)) {
                ctrl->has_cover = true;
                ctrl->state = PZ_AI_STATE_SEEKING_COVER;
                ctrl->move_target = ctrl->cover_pos;
                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "AI %d found cover at (%.1f, %.1f)", tank->id,
                    ctrl->cover_pos.x, ctrl->cover_pos.y);
            } else {
                ctrl->cover_search_timer = cover_search_cooldown;
            }
        }
        break;

    case PZ_AI_STATE_SEEKING_COVER: {
        // Move toward cover position
        pz_vec2 to_cover = pz_vec2_sub(ctrl->cover_pos, tank->pos);
        float dist = pz_vec2_len(to_cover);

        if (dist < arrive_threshold) {
            // Arrived at cover
            ctrl->state = PZ_AI_STATE_IN_COVER;
            ctrl->state_timer = cover_wait_time
                * (0.5f + 0.5f * ((float)(rand() % 100) / 100.0f));
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "AI %d arrived at cover",
                tank->id);
        } else {
            // Move toward cover
            move_dir = pz_vec2_scale(to_cover, 1.0f / dist);
        }
        break;
    }

    case PZ_AI_STATE_IN_COVER:
        if (check_line_of_sight(map, tank->pos, player_pos)) {
            ctrl->has_cover = false;
            ctrl->state = PZ_AI_STATE_IDLE;
            ctrl->state_timer = 0.0f;
            break;
        }
        // Wait in cover, then peek out
        ctrl->state_timer -= dt;
        if (ctrl->state_timer <= 0.0f) {
            if ((rand() % 100) < 25) {
                ctrl->has_cover = false;
                ctrl->state = PZ_AI_STATE_IDLE;
                ctrl->state_timer = 0.0f;
            } else {
                ctrl->state = PZ_AI_STATE_PEEKING;
                ctrl->move_target = ctrl->peek_pos;
                ctrl->shots_fired = 0;
                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "AI %d peeking from cover", tank->id);
            }
        }
        break;

    case PZ_AI_STATE_PEEKING: {
        // Move to peek position
        pz_vec2 to_peek = pz_vec2_sub(ctrl->peek_pos, tank->pos);
        float dist = pz_vec2_len(to_peek);

        if (dist < arrive_threshold) {
            // Arrived at peek position, start firing
            ctrl->state = PZ_AI_STATE_FIRING;
            ctrl->state_timer = firing_time;
        } else {
            move_dir = pz_vec2_scale(to_peek, 1.0f / dist);
        }
        break;
    }

    case PZ_AI_STATE_FIRING:
        // Stay exposed and fire (firing happens in pz_ai_fire)
        // Retreat after timer expires or we've fired enough shots
        ctrl->state_timer -= dt;
        if (ctrl->state_timer <= 0.0f
            || ctrl->shots_fired >= ctrl->max_shots_per_peek) {
            ctrl->state = PZ_AI_STATE_RETREATING;
            ctrl->move_target = ctrl->cover_pos;
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                "AI %d retreating to cover after %d shots", tank->id,
                ctrl->shots_fired);
        }
        break;

    case PZ_AI_STATE_RETREATING: {
        // Move back to cover
        pz_vec2 to_cover = pz_vec2_sub(ctrl->cover_pos, tank->pos);
        float dist = pz_vec2_len(to_cover);

        if (dist < arrive_threshold) {
            // Back in cover
            ctrl->state = PZ_AI_STATE_IN_COVER;
            ctrl->state_timer = cover_wait_time
                * (0.5f + 0.5f * ((float)(rand() % 100) / 100.0f));

            // Occasionally search for new cover
            if ((rand() % 100) < 50) {
                ctrl->has_cover = false;
                ctrl->state = PZ_AI_STATE_IDLE;
                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "AI %d looking for new cover", tank->id);
            }
        } else {
            move_dir = pz_vec2_scale(to_cover, 1.0f / dist);
        }
        break;
    }
    }

    // Apply movement
    pz_tank_input input = {
        .move_dir = pz_vec2_scale(move_dir, move_speed),
        .target_turret = ctrl->current_aim_angle,
        .fire = false,
    };

    pz_tank_update(tank_mgr, tank, &input, map, dt);
}

/* ============================================================================
 * Level 3 Aggressive Hunter AI
 * ============================================================================
 */

// Check if there's an incoming projectile that threatens the tank
// Returns true if evasion is needed, sets evade_dir
static bool
check_incoming_projectiles(pz_ai_controller *ctrl, pz_tank *tank,
    pz_projectile_manager *proj_mgr, pz_vec2 *evade_dir)
{
    if (!proj_mgr) {
        return false;
    }

    const float threat_radius = 3.0f; // How close before we evade
    const float threat_time = 0.5f; // How many seconds ahead to predict

    bool threat_found = false;
    pz_vec2 best_evade = { 0.0f, 0.0f };
    float closest_threat = threat_radius;

    // Check all active projectiles
    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
        const pz_projectile *proj = &proj_mgr->projectiles[i];
        if (!proj->active) {
            continue;
        }

        // Ignore our own projectiles
        if (proj->owner_id == tank->id) {
            continue;
        }

        // Predict where projectile will be
        pz_vec2 proj_future = pz_vec2_add(
            proj->pos, pz_vec2_scale(proj->velocity, threat_time));

        // Check distance to predicted position
        pz_vec2 to_proj = pz_vec2_sub(proj->pos, tank->pos);
        pz_vec2 to_future = pz_vec2_sub(proj_future, tank->pos);

        // Check if projectile is heading toward us
        float proj_speed = pz_vec2_len(proj->velocity);
        if (proj_speed < 0.1f) {
            continue;
        }

        pz_vec2 proj_dir = pz_vec2_scale(proj->velocity, 1.0f / proj_speed);

        // Calculate closest approach distance
        float dot = pz_vec2_dot(to_proj, proj_dir);
        if (dot > 0) {
            continue; // Projectile moving away from us
        }

        // Point of closest approach
        pz_vec2 closest_point
            = pz_vec2_sub(proj->pos, pz_vec2_scale(proj_dir, dot));
        float closest_dist = pz_vec2_dist(closest_point, tank->pos);

        if (closest_dist < closest_threat) {
            closest_threat = closest_dist;
            threat_found = true;

            // Calculate perpendicular evasion direction
            pz_vec2 perp = { -proj_dir.y, proj_dir.x };

            // Choose which side to evade to (away from projectile)
            if (pz_vec2_dot(perp, to_proj) > 0) {
                best_evade = perp;
            } else {
                best_evade = pz_vec2_scale(perp, -1.0f);
            }
        }
    }

    if (threat_found) {
        *evade_dir = best_evade;
    }

    return threat_found;
}

// Find a flanking position to approach player from the side
static bool
find_flank_position(
    const pz_map *map, pz_vec2 ai_pos, pz_vec2 player_pos, pz_vec2 *flank_pos)
{
    const float tank_radius = 0.9f;
    const float flank_distance = 8.0f; // How far to the side
    const float approach_distance = 6.0f; // How close to get

    // Direction from AI to player
    pz_vec2 to_player = pz_vec2_sub(player_pos, ai_pos);
    float dist = pz_vec2_len(to_player);
    if (dist < 0.1f) {
        return false;
    }

    pz_vec2 dir_to_player = pz_vec2_scale(to_player, 1.0f / dist);

    // Perpendicular directions for flanking
    pz_vec2 perp_left = { -dir_to_player.y, dir_to_player.x };
    pz_vec2 perp_right = { dir_to_player.y, -dir_to_player.x };

    // Try both flanking directions
    pz_vec2 candidates[2];
    candidates[0] = pz_vec2_add(player_pos,
        pz_vec2_add(pz_vec2_scale(perp_left, flank_distance),
            pz_vec2_scale(dir_to_player, -approach_distance)));
    candidates[1] = pz_vec2_add(player_pos,
        pz_vec2_add(pz_vec2_scale(perp_right, flank_distance),
            pz_vec2_scale(dir_to_player, -approach_distance)));

    // Choose the closest valid flanking position
    float best_dist = 1e10f;
    bool found = false;

    for (int i = 0; i < 2; i++) {
        if (!is_position_valid(map, candidates[i], tank_radius)) {
            continue;
        }

        float d = pz_vec2_dist(ai_pos, candidates[i]);
        if (d < best_dist) {
            best_dist = d;
            *flank_pos = candidates[i];
            found = true;
        }
    }

    return found;
}

static void
update_level3_ai(pz_ai_controller *ctrl, pz_tank *tank,
    pz_tank_manager *tank_mgr, const pz_map *map, pz_vec2 player_pos,
    pz_projectile_manager *proj_mgr, float dt)
{
    const float move_speed = 6.0f; // Fast and aggressive
    const float arrive_threshold = 0.5f;
    const float engage_distance = 12.0f; // Distance to start shooting
    const float chase_distance = 20.0f; // Distance to start chasing
    const float too_close_distance = 3.0f; // Back off if too close
    const float evade_duration = 0.3f;
    const float health_retreat_threshold = 0.2f; // Retreat when below 20% HP

    // Update timers
    if (ctrl->evade_timer > 0.0f) {
        ctrl->evade_timer -= dt;
    }
    if (ctrl->aggression_timer > 0.0f) {
        ctrl->aggression_timer -= dt;
    }
    if (ctrl->cover_search_timer > 0.0f) {
        ctrl->cover_search_timer -= dt;
    }

    float dist_to_player = pz_vec2_dist(tank->pos, player_pos);
    float health_ratio = (float)tank->health / (float)tank->max_health;

    // Check for incoming projectiles - highest priority
    pz_vec2 evade_dir = { 0.0f, 0.0f };
    bool should_evade
        = check_incoming_projectiles(ctrl, tank, proj_mgr, &evade_dir);

    if (should_evade && ctrl->evade_timer <= 0.0f) {
        ctrl->state = PZ_AI_STATE_EVADING;
        ctrl->evade_dir = evade_dir;
        ctrl->evade_timer = evade_duration;
        ctrl->wants_to_fire = false;
    }

    // State machine
    pz_vec2 move_dir = { 0.0f, 0.0f };

    switch (ctrl->state) {
    case PZ_AI_STATE_IDLE:
        // Start chasing the player immediately
        ctrl->state = PZ_AI_STATE_CHASING;
        ctrl->aggression_timer = 1.0f + (float)(rand() % 100) / 100.0f;
        break;

    case PZ_AI_STATE_EVADING:
        // Dodge incoming projectile
        move_dir = ctrl->evade_dir;
        ctrl->wants_to_fire = false;

        if (ctrl->evade_timer <= 0.0f) {
            // Return to previous behavior based on situation
            if (health_ratio < health_retreat_threshold) {
                ctrl->state = PZ_AI_STATE_SEEKING_COVER;
                ctrl->has_cover = false;
            } else if (dist_to_player < engage_distance) {
                ctrl->state = PZ_AI_STATE_ENGAGING;
            } else {
                ctrl->state = PZ_AI_STATE_CHASING;
            }
        }
        break;

    case PZ_AI_STATE_CHASING: {
        // Move toward player aggressively
        pz_vec2 to_player = pz_vec2_sub(player_pos, tank->pos);

        if (dist_to_player > 0.1f) {
            move_dir = pz_vec2_scale(to_player, 1.0f / dist_to_player);
        }

        // Transition to engaging when close enough
        if (dist_to_player < engage_distance && ctrl->can_see_player) {
            ctrl->state = PZ_AI_STATE_ENGAGING;
            ctrl->state_timer = 4.0f + (float)(rand() % 200) / 100.0f;
        }

        // Try to flank frequently
        if (ctrl->aggression_timer <= 0.0f && dist_to_player < chase_distance) {
            if (find_flank_position(
                    map, tank->pos, player_pos, &ctrl->flank_target)) {
                ctrl->state = PZ_AI_STATE_FLANKING;
                ctrl->aggression_timer = 2.0f;
            } else {
                ctrl->aggression_timer = 1.0f;
            }
        }

        // Only retreat if very low health
        if (health_ratio < health_retreat_threshold) {
            ctrl->state = PZ_AI_STATE_SEEKING_COVER;
            ctrl->has_cover = false;
        }

        // Fire while chasing if we can see the player
        ctrl->wants_to_fire = ctrl->can_see_player;
        break;
    }

    case PZ_AI_STATE_FLANKING: {
        // Move to flanking position
        pz_vec2 to_flank = pz_vec2_sub(ctrl->flank_target, tank->pos);
        float flank_dist = pz_vec2_len(to_flank);

        if (flank_dist < arrive_threshold) {
            // Reached flanking position, engage
            ctrl->state = PZ_AI_STATE_ENGAGING;
            ctrl->state_timer = 2.0f;
        } else {
            move_dir = pz_vec2_scale(to_flank, 1.0f / flank_dist);
        }

        // Fire while flanking if we have line of sight
        ctrl->wants_to_fire = ctrl->can_see_player;

        // Retreat if low health
        if (health_ratio < health_retreat_threshold) {
            ctrl->state = PZ_AI_STATE_SEEKING_COVER;
            ctrl->has_cover = false;
        }
        break;
    }

    case PZ_AI_STATE_ENGAGING: {
        // Strafe and shoot
        ctrl->state_timer -= dt;

        // Calculate strafing direction (perpendicular to player)
        pz_vec2 to_player = pz_vec2_sub(player_pos, tank->pos);
        if (dist_to_player > 0.1f) {
            pz_vec2 dir_to_player
                = pz_vec2_scale(to_player, 1.0f / dist_to_player);

            // Strafe perpendicular, occasionally switching direction
            pz_vec2 strafe = { -dir_to_player.y, dir_to_player.x };
            if (((int)(ctrl->state_timer * 2.0f)) % 2 == 0) {
                strafe = pz_vec2_scale(strafe, -1.0f);
            }

            // Also move closer or farther based on distance
            if (dist_to_player > engage_distance * 0.7f) {
                // Move closer
                move_dir
                    = pz_vec2_add(strafe, pz_vec2_scale(dir_to_player, 0.5f));
            } else if (dist_to_player < too_close_distance) {
                // Back off
                move_dir
                    = pz_vec2_add(strafe, pz_vec2_scale(dir_to_player, -0.8f));
            } else {
                // Just strafe
                move_dir = strafe;
            }

            // Normalize
            float len = pz_vec2_len(move_dir);
            if (len > 0.1f) {
                move_dir = pz_vec2_scale(move_dir, 1.0f / len);
            }
        }

        ctrl->wants_to_fire = ctrl->can_see_player;

        // Transition out of engaging
        if (ctrl->state_timer <= 0.0f) {
            if (health_ratio < health_retreat_threshold) {
                ctrl->state = PZ_AI_STATE_SEEKING_COVER;
                ctrl->has_cover = false;
            } else if ((rand() % 100) < 40) {
                ctrl->state = PZ_AI_STATE_CHASING;
                ctrl->aggression_timer = 1.5f;
            } else {
                ctrl->state_timer = 2.0f + (float)(rand() % 200) / 100.0f;
            }
        }

        // Lost line of sight - chase
        if (!ctrl->can_see_player) {
            ctrl->state = PZ_AI_STATE_CHASING;
        }

        // Retreat if low health
        if (health_ratio < health_retreat_threshold) {
            ctrl->state = PZ_AI_STATE_SEEKING_COVER;
            ctrl->has_cover = false;
        }
        break;
    }

    case PZ_AI_STATE_SEEKING_COVER: {
        // Find cover if we don't have it
        if (!ctrl->has_cover && ctrl->cover_search_timer <= 0.0f) {
            if (find_cover_position(map, tank->pos, player_pos,
                    &ctrl->cover_pos, &ctrl->peek_pos)) {
                ctrl->has_cover = true;
                ctrl->move_target = ctrl->cover_pos;
            } else {
                ctrl->cover_search_timer = 1.0f;
                // Can't find cover, just run away
                pz_vec2 away = pz_vec2_sub(tank->pos, player_pos);
                float len = pz_vec2_len(away);
                if (len > 0.1f) {
                    move_dir = pz_vec2_scale(away, 1.0f / len);
                }
            }
        }

        if (ctrl->has_cover) {
            pz_vec2 to_cover = pz_vec2_sub(ctrl->cover_pos, tank->pos);
            float cover_dist = pz_vec2_len(to_cover);

            if (cover_dist < arrive_threshold) {
                ctrl->state = PZ_AI_STATE_IN_COVER;
                ctrl->state_timer = 1.5f + (float)(rand() % 150) / 100.0f;
            } else {
                move_dir = pz_vec2_scale(to_cover, 1.0f / cover_dist);
            }
        }

        ctrl->wants_to_fire = false;
        break;
    }

    case PZ_AI_STATE_IN_COVER:
        // Wait and recover
        ctrl->state_timer -= dt;

        if (ctrl->state_timer <= 0.0f) {
            // Health recovered enough or timeout - go aggressive again
            if (health_ratio > 0.5f || ctrl->state_timer < -3.0f) {
                ctrl->state = PZ_AI_STATE_CHASING;
                ctrl->has_cover = false;
                ctrl->aggression_timer = 1.0f;
            } else {
                // Peek out to fire
                ctrl->state = PZ_AI_STATE_PEEKING;
                ctrl->move_target = ctrl->peek_pos;
                ctrl->shots_fired = 0;
            }
        }

        ctrl->wants_to_fire = false;
        break;

    case PZ_AI_STATE_PEEKING: {
        pz_vec2 to_peek = pz_vec2_sub(ctrl->peek_pos, tank->pos);
        float peek_dist = pz_vec2_len(to_peek);

        if (peek_dist < arrive_threshold) {
            ctrl->state = PZ_AI_STATE_FIRING;
            ctrl->state_timer = 1.5f;
        } else {
            move_dir = pz_vec2_scale(to_peek, 1.0f / peek_dist);
        }

        ctrl->wants_to_fire = ctrl->can_see_player;
        break;
    }

    case PZ_AI_STATE_FIRING:
        ctrl->state_timer -= dt;
        ctrl->wants_to_fire = ctrl->can_see_player;

        if (ctrl->state_timer <= 0.0f
            || ctrl->shots_fired >= ctrl->max_shots_per_peek) {
            // Go back to aggressive if health is ok
            if (health_ratio > 0.5f) {
                ctrl->state = PZ_AI_STATE_CHASING;
                ctrl->has_cover = false;
            } else {
                ctrl->state = PZ_AI_STATE_RETREATING;
                ctrl->move_target = ctrl->cover_pos;
            }
        }
        break;

    case PZ_AI_STATE_RETREATING: {
        pz_vec2 to_cover = pz_vec2_sub(ctrl->cover_pos, tank->pos);
        float cover_dist = pz_vec2_len(to_cover);

        if (cover_dist < arrive_threshold) {
            ctrl->state = PZ_AI_STATE_IN_COVER;
            ctrl->state_timer = 2.0f;
        } else {
            move_dir = pz_vec2_scale(to_cover, 1.0f / cover_dist);
        }

        ctrl->wants_to_fire = false;
        break;
    }

    default:
        ctrl->state = PZ_AI_STATE_CHASING;
        break;
    }

    // Apply movement
    pz_tank_input input = {
        .move_dir = pz_vec2_scale(move_dir, move_speed),
        .target_turret = ctrl->current_aim_angle,
        .fire = false,
    };

    pz_tank_update(tank_mgr, tank, &input, map, dt);
}

/* ============================================================================
 * AI Update
 * ============================================================================
 */

void
pz_ai_update(pz_ai_manager *ai_mgr, pz_vec2 player_pos,
    pz_projectile_manager *proj_mgr, float dt)
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
        float direct_angle = atan2f(dx, dy);

        // Check line of sight
        ctrl->can_see_player
            = check_line_of_sight(ai_mgr->map, tank->pos, player_pos);

        // Level 1: Consider bounce shots when player is not visible
        bool uses_bounce_shots = (ctrl->level == PZ_ENEMY_LEVEL_1
            || ctrl->level == PZ_ENEMY_LEVEL_2
            || ctrl->level == PZ_ENEMY_LEVEL_SNIPER);
        if (uses_bounce_shots) {
            // Update bounce shot search timer
            if (ctrl->bounce_shot_search_timer > 0.0f) {
                ctrl->bounce_shot_search_timer -= dt;
            }

            if (ctrl->can_see_player) {
                // Direct LOS - aim straight at player
                ctrl->target_aim_angle = direct_angle;
                ctrl->has_bounce_shot = false;
            } else {
                // No LOS - try to find a bounce shot
                if (!ctrl->has_bounce_shot
                    && ctrl->bounce_shot_search_timer <= 0.0f) {
                    float bounce_angle;
                    float bounce_range = stats->bounce_shot_range;
                    if (bounce_range <= 0.0f) {
                        bounce_range = 30.0f;
                    }
                    if (find_bounce_shot(ai_mgr->map, tank->pos, player_pos,
                            bounce_range, &bounce_angle)) {
                        ctrl->has_bounce_shot = true;
                        ctrl->bounce_shot_angle = bounce_angle;
                        pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                            "AI %d found bounce shot at angle %.1f deg",
                            tank->id, bounce_angle * 180.0f / PZ_PI);
                    } else {
                        // No bounce shot found, try again later
                        ctrl->bounce_shot_search_timer = 0.5f;
                    }
                }

                if (ctrl->has_bounce_shot) {
                    ctrl->target_aim_angle = ctrl->bounce_shot_angle;
                } else {
                    // Fallback: aim at player anyway (for when they become
                    // visible)
                    ctrl->target_aim_angle = direct_angle;
                }
            }
        } else {
            // Level 2/3: Always aim directly at player
            ctrl->target_aim_angle = direct_angle;
        }

        // Smoothly rotate turret towards target
        float angle_diff
            = normalize_angle(ctrl->target_aim_angle - ctrl->current_aim_angle);
        float turret_speed = 5.0f * stats->aim_speed;
        float max_rotation = turret_speed * dt;

        if (fabsf(angle_diff) <= max_rotation) {
            ctrl->current_aim_angle = ctrl->target_aim_angle;
        } else if (angle_diff > 0) {
            ctrl->current_aim_angle += max_rotation;
        } else {
            ctrl->current_aim_angle -= max_rotation;
        }

        ctrl->current_aim_angle = normalize_angle(ctrl->current_aim_angle);

        // Level-specific behavior
        if (ctrl->level == PZ_ENEMY_LEVEL_3) {
            // Level 3: Aggressive hunter
            update_level3_ai(ctrl, tank, ai_mgr->tank_mgr, ai_mgr->map,
                player_pos, proj_mgr, dt);
        } else if (ctrl->level == PZ_ENEMY_LEVEL_2) {
            // Level 2: Cover-based
            update_level2_ai(
                ctrl, tank, ai_mgr->tank_mgr, ai_mgr->map, player_pos, dt);
        } else {
            // Stationary turret only (sentry, sniper)
            pz_tank_input input = {
                .move_dir = { 0.0f, 0.0f },
                .target_turret = ctrl->current_aim_angle,
                .fire = false,
            };
            pz_tank_update(ai_mgr->tank_mgr, tank, &input, ai_mgr->map, dt);
        }

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

        // Level-specific firing conditions
        if (ctrl->level == PZ_ENEMY_LEVEL_3) {
            // Level 3: Use wants_to_fire flag set by state machine
            if (!ctrl->wants_to_fire) {
                continue;
            }
        } else if (ctrl->level == PZ_ENEMY_LEVEL_2) {
            // Level 2: Fire whenever not behind cover
            if (ctrl->state == PZ_AI_STATE_IN_COVER) {
                continue;
            }
        }
        // Level 1: Always try to fire (stationary turret, including bounce
        // shots)

        // Check if we can fire
        // Level 1: Can fire if we see player OR have a bounce shot
        // Others: Need to see the player
        bool can_attempt_fire = ctrl->can_see_player;
        bool uses_bounce_shots = (ctrl->level == PZ_ENEMY_LEVEL_1
            || ctrl->level == PZ_ENEMY_LEVEL_2
            || ctrl->level == PZ_ENEMY_LEVEL_SNIPER);
        if (uses_bounce_shots && ctrl->has_bounce_shot) {
            can_attempt_fire = true;
        }

        if (!can_attempt_fire) {
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

        // Get weapon stats for this enemy
        const pz_weapon_stats *weapon = pz_weapon_get_stats(stats->weapon_type);

        // Check active projectile count
        int active = pz_projectile_count_by_owner(proj_mgr, tank->id);
        if (active >= weapon->max_active_projectiles) {
            continue;
        }

        // Fire!
        pz_vec2 spawn_pos = { 0 };
        pz_vec2 fire_dir = { 0 };
        int bounce_cost = 0;
        pz_tank_get_fire_solution(
            tank, ai_mgr->map, &spawn_pos, &fire_dir, &bounce_cost);

        float projectile_speed
            = weapon->projectile_speed * stats->projectile_speed_scale;
        pz_projectile_config proj_config = {
            .speed = projectile_speed,
            .max_bounces = stats->max_bounces,
            .lifetime = -1.0f,
            .damage = weapon->damage,
            .scale = weapon->projectile_scale,
            .color = weapon->projectile_color,
        };

        int proj_slot = pz_projectile_spawn(
            proj_mgr, spawn_pos, fire_dir, &proj_config, tank->id);
        if (proj_slot >= 0 && bounce_cost > 0) {
            pz_projectile *proj = &proj_mgr->projectiles[proj_slot];
            if (proj->bounces_remaining > 0) {
                proj->bounces_remaining -= 1;
            }
        }

        // Reset fire timer to the weapon's max fire rate (same as player).
        ctrl->fire_timer = weapon->fire_cooldown;
        fired++;

        // Track shots for cover behavior
        ctrl->shots_fired++;

        // Level 1: After firing a bounce shot, search for a new one next time
        if (uses_bounce_shots && ctrl->has_bounce_shot) {
            ctrl->has_bounce_shot = false;
            ctrl->bounce_shot_search_timer = 0.3f; // Brief delay before
                                                   // searching again
        }

        pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "AI tank %d fired (%s)", tank->id,
            pz_enemy_level_name(ctrl->level));
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
pz_ai_has_level3_alive(const pz_ai_manager *ai_mgr)
{
    if (!ai_mgr || !ai_mgr->tank_mgr) {
        return false;
    }

    for (int i = 0; i < ai_mgr->controller_count; i++) {
        const pz_ai_controller *ctrl = &ai_mgr->controllers[i];
        if (ctrl->level == PZ_ENEMY_LEVEL_3
            || ctrl->level == PZ_ENEMY_LEVEL_SNIPER) {
            pz_tank *tank = pz_tank_get_by_id(ai_mgr->tank_mgr, ctrl->tank_id);
            if (tank && !(tank->flags & PZ_TANK_FLAG_DEAD)) {
                return true;
            }
        }
    }

    return false;
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
