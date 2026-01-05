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
typedef struct pz_rng pz_rng;
typedef struct pz_tank_manager pz_tank_manager;
typedef struct pz_projectile_manager pz_projectile_manager;
typedef struct pz_mine_manager pz_mine_manager;
typedef struct pz_toxic_cloud pz_toxic_cloud;

/* ============================================================================
 * Enemy Types
 * ============================================================================
 *
 * Sentry enemy
 *   - 10 HP
 *   - Default weapon (1 bounce)
 *   - Normal fire rate
 *   - Stationary (turret only)
 *
 * Skirmisher enemy
 *   - 15 HP
 *   - Default weapon (1 bounce)
 *   - Faster fire rate
 *   - Uses cover: hides behind walls, peeks out to fire
 *
 * Hunter enemy
 *   - 20 HP
 *   - Machine gun (no bounces)
 *   - Fast fire rate
 *   - Aggressively chases player
 *   - Evades incoming bullets
 *   - Seeks cover when low health or reloading
 *
 * Sniper enemy
 *   - 12 HP
 *   - Ricochet weapon (3 bounces)
 *   - Slow fire rate, long-range shots
 *   - Mostly stationary (turret only)
 */

typedef enum {
    PZ_ENEMY_TYPE_SENTRY = 1,
    PZ_ENEMY_TYPE_SKIRMISHER = 2,
    PZ_ENEMY_TYPE_HUNTER = 3,
    PZ_ENEMY_TYPE_SNIPER = 4,
} pz_enemy_type;

typedef enum {
    PZ_AI_BEHAVIOR_MOVE = 1u << 0,
    PZ_AI_BEHAVIOR_PATHFIND = 1u << 1,
    PZ_AI_BEHAVIOR_USE_COVER = 1u << 2,
    PZ_AI_BEHAVIOR_CHASE = 1u << 3,
    PZ_AI_BEHAVIOR_FLANK = 1u << 4,
    PZ_AI_BEHAVIOR_EVADE = 1u << 5,
    PZ_AI_BEHAVIOR_STRAFE = 1u << 6,
    PZ_AI_BEHAVIOR_BOUNCE_SHOTS = 1u << 7,
    PZ_AI_BEHAVIOR_DEFEND_PROJECTILES = 1u << 8,
    PZ_AI_BEHAVIOR_TARGET_MINES = 1u << 9,
    PZ_AI_BEHAVIOR_TOXIC_ESCAPE = 1u << 10,
    PZ_AI_BEHAVIOR_REQUIRE_BOUNCE_SHOT = 1u << 11,
} pz_ai_behavior_flags;

// Enemy stats based on type
typedef struct pz_enemy_stats {
    int health;
    int max_bounces; // Weapon bounces
    float fire_cooldown_scale; // Multiplier applied to weapon cooldown
    float aim_speed; // Turret rotation speed multiplier
    float aim_tolerance; // Allowed aim error in radians
    float move_speed; // Base movement speed for AI movement
    pz_vec4 body_color; // Tank body color
    pz_powerup_type weapon_type; // Default weapon for this enemy
    float projectile_speed_scale; // Multiplier applied to weapon speed
    float bounce_shot_range; // Bounce-search range for stationary types
    float projectile_defense_chance; // 0.0 disables incoming-shot defense
    int bounce_shot_samples; // Angular samples for ricochet search
    int max_shots_per_peek; // Cover peek firing limit
    int max_projectiles_direct; // Max active shots for direct LOS
    int max_projectiles_bounce; // Max active shots for bounce shots
    uint32_t behavior_flags; // pz_ai_behavior_flags
} pz_enemy_stats;

// Get stats for an enemy type
const pz_enemy_stats *pz_enemy_get_stats(pz_enemy_type type);
const char *pz_enemy_type_name(pz_enemy_type type);

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
    pz_enemy_type type; // Enemy type (determines behavior)
    uint32_t behavior_flags; // pz_ai_behavior_flags

    // Aiming state
    float current_aim_angle; // Current turret aim (for smoothing)
    float target_aim_angle; // Target angle towards player

    // Firing state
    float fire_timer; // Countdown to next shot
    bool can_see_player; // Line-of-sight to player?

    // Behavior timers
    float reaction_delay; // Delay before reacting to player movement
    float last_seen_time; // When we last saw the player

    // Cover behavior
    pz_ai_state state; // Current behavior state
    pz_vec2 cover_pos; // Position behind cover
    pz_vec2 peek_pos; // Position when peeking out to fire
    pz_vec2 move_target; // Current movement target
    float state_timer; // Timer for current state
    float cover_search_timer; // Cooldown for searching new cover
    bool has_cover; // Whether we have a valid cover position
    int shots_fired; // Number of shots fired while peeking
    int max_shots_per_peek; // How many shots to fire before retreating

    // Aggressive behavior
    pz_vec2 evade_dir; // Direction to evade when dodging
    float evade_timer; // Duration of current evade
    float aggression_timer; // Time until switching to aggressive behavior
    float last_player_pos_x; // For tracking player movement
    float last_player_pos_y;
    pz_vec2 flank_target; // Target position for flanking
    bool wants_to_fire; // Request to fire (checked in pz_ai_fire)

    // Bounce shot targeting
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

    // Mine awareness
    bool targeting_mine; // Whether we're aiming at a mine instead of player
    pz_vec2 mine_target_pos; // Mine position we're targeting
    float mine_target_dist; // Distance to targeted mine

    // A* Pathfinding
    pz_path path; // Current path being followed
    pz_vec2 path_goal; // Goal position for current path
    float path_update_timer; // Time until next repath

    // Toxic cloud escape behavior
    bool toxic_escaping; // Currently in toxic escape mode
    pz_vec2 toxic_escape_target; // Safe destination to reach
    pz_path toxic_escape_path; // Path to escape destination
    float toxic_check_timer; // Timer for rechecking toxic threat
    float toxic_urgency; // 0.0 = safe, 1.0 = critical (in toxic now)

    // Temporary detour when blocked by other tanks
    bool detour_active; // Using a temporary detour target
    pz_vec2 detour_target; // Temporary target to make space
    float detour_timer; // Time remaining for detour attempt
    float detour_blocked_timer; // Time spent blocked by another tank
    pz_vec2 detour_last_pos; // Last position used for stall detection
    bool detour_has_last_pos; // Whether detour_last_pos is initialized
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
    pz_ai_manager *ai_mgr, pz_vec2 pos, float angle, pz_enemy_type type);

// Update all AI controllers
// This generates pz_tank_input for each AI-controlled tank
// player_pos is the position of the player tank (for aiming)
// proj_mgr is optional - used by Level 3 to detect incoming projectiles
// rng is used for AI decision-making randomness (deterministic)
void pz_ai_update(pz_ai_manager *ai_mgr, pz_vec2 player_pos,
    pz_projectile_manager *proj_mgr, pz_mine_manager *mine_mgr, pz_rng *rng,
    const pz_toxic_cloud *toxic_cloud, float dt);

// Fire projectiles for AI tanks that want to fire
// This should be called after pz_ai_update
// Returns the number of projectiles fired
int pz_ai_fire(pz_ai_manager *ai_mgr, pz_projectile_manager *proj_mgr);

// Get count of alive AI-controlled enemies
int pz_ai_count_alive(const pz_ai_manager *ai_mgr);
bool pz_ai_has_elite_alive(const pz_ai_manager *ai_mgr);

// Check if a tank is AI-controlled
bool pz_ai_is_controlled(const pz_ai_manager *ai_mgr, int tank_id);

// Get the AI controller for a tank (NULL if not AI-controlled)
pz_ai_controller *pz_ai_get_controller(pz_ai_manager *ai_mgr, int tank_id);

#endif // PZ_AI_H
