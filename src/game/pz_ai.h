/*
 * Tank Game - Enemy AI System
 *
 * AI controllers that generate pz_tank_input for CPU-controlled tanks.
 * AI tanks have the same constraints as players - they can only "send inputs"
 * as if they were human players (no cheating like teleporting or instant aim).
 */

#ifndef PZ_AI_H
#define PZ_AI_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"
#include "pz_pathfinding.h"
#include "pz_powerup.h"
#include "pz_tank.h"

// Forward declarations
typedef struct pz_map pz_map;
typedef struct pz_tank_manager pz_tank_manager;
typedef struct pz_projectile_manager pz_projectile_manager;

/* ============================================================================
 * Enemy Types
 * ============================================================================
 *
 * Level 1: Sentry enemy
 *   - 10 HP
 *   - Default weapon (1 bounce)
 *   - Normal fire rate
 *   - Stationary (turret only)
 *
 * Level 2: Skirmisher enemy
 *   - 15 HP
 *   - Default weapon (1 bounce)
 *   - Faster fire rate
 *   - Uses cover: hides behind walls, peeks out to fire
 *
 * Level 3: Hunter enemy
 *   - 20 HP
 *   - Machine gun (no bounces)
 *   - Fast fire rate
 *   - Aggressively chases player
 *   - Evades incoming bullets
 *   - Seeks cover when low health or reloading
 *
 * Level 4: Sniper enemy
 *   - 12 HP
 *   - Ricochet weapon (3 bounces)
 *   - Slow fire rate, long-range shots
 *   - Mostly stationary (turret only)
 */

typedef enum {
    PZ_ENEMY_LEVEL_1 = 1,
    PZ_ENEMY_LEVEL_2 = 2,
    PZ_ENEMY_LEVEL_3 = 3,
    PZ_ENEMY_LEVEL_SNIPER = 4,
} pz_enemy_level;

// Enemy stats based on level
typedef struct pz_enemy_stats {
    int health;
    int max_bounces; // Weapon bounces
    float fire_cooldown; // Seconds between shots
    float aim_speed; // Turret rotation speed multiplier
    pz_vec4 body_color; // Tank body color
    pz_powerup_type weapon_type; // Default weapon for this enemy
    float projectile_speed_scale; // Multiplier applied to weapon speed
    float bounce_shot_range; // Bounce-search range for stationary types
    float projectile_defense_chance; // 0.0 disables incoming-shot defense
} pz_enemy_stats;

// Get stats for an enemy level
const pz_enemy_stats *pz_enemy_get_stats(pz_enemy_level level);
const char *pz_enemy_level_name(pz_enemy_level level);

/* ============================================================================
 * AI Controller
 * ============================================================================
 */

// AI behavior states for cover-using enemies (Level 2+)
typedef enum {
    PZ_AI_STATE_IDLE, // Not moving, just aiming
    PZ_AI_STATE_SEEKING_COVER, // Moving toward a cover position
    PZ_AI_STATE_IN_COVER, // Hidden behind cover
    PZ_AI_STATE_PEEKING, // Moving out from cover to fire
    PZ_AI_STATE_FIRING, // Exposed, aiming and firing
    PZ_AI_STATE_RETREATING, // Moving back to cover after firing
    // Level 3 aggressive states
    PZ_AI_STATE_CHASING, // Actively pursuing the player
    PZ_AI_STATE_FLANKING, // Moving to a flanking position
    PZ_AI_STATE_EVADING, // Dodging incoming projectiles
    PZ_AI_STATE_ENGAGING, // In combat range, shooting while moving
} pz_ai_state;

// AI state for a single enemy
typedef struct pz_ai_controller {
    int tank_id; // Which tank this AI controls
    pz_enemy_level level; // Enemy level (determines behavior)

    // Aiming state
    float current_aim_angle; // Current turret aim (for smoothing)
    float target_aim_angle; // Target angle towards player

    // Firing state
    float fire_timer; // Countdown to next shot
    bool can_see_player; // Line-of-sight to player?

    // Behavior timers
    float reaction_delay; // Delay before reacting to player movement
    float last_seen_time; // When we last saw the player

    // Cover behavior (Level 2+)
    pz_ai_state state; // Current behavior state
    pz_vec2 cover_pos; // Position behind cover
    pz_vec2 peek_pos; // Position when peeking out to fire
    pz_vec2 move_target; // Current movement target
    float state_timer; // Timer for current state
    float cover_search_timer; // Cooldown for searching new cover
    bool has_cover; // Whether we have a valid cover position
    int shots_fired; // Number of shots fired while peeking
    int max_shots_per_peek; // How many shots to fire before retreating

    // Level 3 aggressive behavior
    pz_vec2 evade_dir; // Direction to evade when dodging
    float evade_timer; // Duration of current evade
    float aggression_timer; // Time until switching to aggressive behavior
    float last_player_pos_x; // For tracking player movement
    float last_player_pos_y;
    pz_vec2 flank_target; // Target position for flanking
    bool wants_to_fire; // Request to fire (checked in pz_ai_fire)

    // Bounce shot targeting (Level 1)
    bool has_bounce_shot; // Whether we found a valid bounce shot
    float bounce_shot_angle; // Angle to aim for bounce shot
    float bounce_shot_search_timer; // Cooldown for searching new bounce shots

    // Fire rate limiting (reduces bullet spam)
    float fire_confidence; // 0.0-1.0: how confident AI is about the shot
    float hesitation_timer; // Delay before first shot after acquiring target
    bool had_target_last_frame; // Track target acquisition for hesitation
    bool defending_projectile; // Whether we're aiming to shoot a projectile
    float defense_aim_angle; // Aim angle for projectile defense
    float defense_check_timer; // Timer for reevaluating defense targets

    // A* Pathfinding (Level 2/3)
    pz_path path; // Current path being followed
    pz_vec2 path_goal; // Goal position for current path
    float path_update_timer; // Time until next repath
    bool use_pathfinding; // Whether this AI uses pathfinding
} pz_ai_controller;

/* ============================================================================
 * AI Manager
 * ============================================================================
 */

#define PZ_MAX_AI_CONTROLLERS 16

typedef struct pz_ai_manager {
    pz_ai_controller controllers[PZ_MAX_AI_CONTROLLERS];
    int controller_count;

    // References to game systems (not owned)
    pz_tank_manager *tank_mgr;
    const pz_map *map;
} pz_ai_manager;

// Create/destroy AI manager
pz_ai_manager *pz_ai_manager_create(
    pz_tank_manager *tank_mgr, const pz_map *map);
void pz_ai_manager_destroy(pz_ai_manager *ai_mgr);

// Update map reference (for hot-reload)
void pz_ai_manager_set_map(pz_ai_manager *ai_mgr, const pz_map *map);

// Spawn an AI-controlled enemy tank
// Returns the tank pointer, or NULL if failed
pz_tank *pz_ai_spawn_enemy(
    pz_ai_manager *ai_mgr, pz_vec2 pos, float angle, pz_enemy_level level);

// Update all AI controllers
// This generates pz_tank_input for each AI-controlled tank
// player_pos is the position of the player tank (for aiming)
// proj_mgr is optional - used by Level 3 to detect incoming projectiles
void pz_ai_update(pz_ai_manager *ai_mgr, pz_vec2 player_pos,
    pz_projectile_manager *proj_mgr, float dt);

// Fire projectiles for AI tanks that want to fire
// This should be called after pz_ai_update
// Returns the number of projectiles fired
int pz_ai_fire(pz_ai_manager *ai_mgr, pz_projectile_manager *proj_mgr);

// Get count of alive AI-controlled enemies
int pz_ai_count_alive(const pz_ai_manager *ai_mgr);
bool pz_ai_has_level3_alive(const pz_ai_manager *ai_mgr);

// Check if a tank is AI-controlled
bool pz_ai_is_controlled(const pz_ai_manager *ai_mgr, int tank_id);

// Get the AI controller for a tank (NULL if not AI-controlled)
pz_ai_controller *pz_ai_get_controller(pz_ai_manager *ai_mgr, int tank_id);

#endif // PZ_AI_H
