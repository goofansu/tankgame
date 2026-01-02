/*
 * Tank Game - Tank Entity System
 *
 * Handles tank entities with health, movement, and rendering.
 */

#ifndef PZ_TANK_H
#define PZ_TANK_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"
#include "../engine/pz_camera.h"
#include "../engine/render/pz_renderer.h"
#include "pz_map.h"
#include "pz_mesh.h"

// Maximum number of tanks
#define PZ_MAX_TANKS 8

// Tank state flags
typedef enum {
    PZ_TANK_FLAG_ACTIVE = (1 << 0),
    PZ_TANK_FLAG_DEAD = (1 << 1),
    PZ_TANK_FLAG_INVULNERABLE = (1 << 2),
    PZ_TANK_FLAG_PLAYER = (1 << 3), // Is this a player-controlled tank?
} pz_tank_flags;

// Forward declaration for weapon type
typedef enum pz_powerup_type pz_powerup_type;

// Tank structure
typedef struct pz_tank {
    uint32_t flags;
    int id; // Unique tank ID (for projectile ownership)

    // Position and movement
    pz_vec2 pos; // Position in world space (X, Z)
    pz_vec2 vel; // Velocity
    float body_angle; // Body rotation (radians)
    float turret_angle; // Turret rotation in world space (radians)

    // Combat
    int health;
    int max_health;
    float fire_cooldown;

    // Current weapon (from powerups)
    int current_weapon; // pz_powerup_type (stored as int to avoid circular dep)

    // Respawn
    float respawn_timer; // Countdown when dead
    float invuln_timer; // Invulnerability time remaining
    pz_vec2 spawn_pos; // Where to respawn

    // Visual feedback
    float damage_flash; // Timer for damage flash effect (0 = no flash)
    pz_vec4 body_color;
    pz_vec4 turret_color;
} pz_tank;

// Input for a tank (per-tick input state)
typedef struct pz_tank_input {
    pz_vec2 move_dir; // Normalized movement direction (0,0 = no input)
    float target_turret; // Target turret angle (world space radians)
    bool fire; // Fire button pressed
} pz_tank_input;

// Tank manager
typedef struct pz_tank_manager {
    pz_tank tanks[PZ_MAX_TANKS];
    int tank_count;
    int next_id;

    // Shared rendering resources
    pz_mesh *body_mesh;
    pz_mesh *turret_mesh;
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    bool render_ready;

    // Movement parameters (shared by all tanks)
    float accel;
    float friction;
    float max_speed;
    float body_turn_speed;
    float turret_turn_speed;
    float collision_radius;
} pz_tank_manager;

// Configuration for tank manager
typedef struct pz_tank_manager_config {
    float accel; // Default: 40.0
    float friction; // Default: 25.0
    float max_speed; // Default: 5.0
    float body_turn_speed; // Default: 5.0 rad/s
    float turret_turn_speed; // Default: 8.0 rad/s
    float collision_radius; // Default: 0.7
} pz_tank_manager_config;

// Default configuration
extern const pz_tank_manager_config PZ_TANK_DEFAULT_CONFIG;

/* ============================================================================
 * Manager API
 * ============================================================================
 */

// Create/destroy manager
pz_tank_manager *pz_tank_manager_create(
    pz_renderer *renderer, const pz_tank_manager_config *config);
void pz_tank_manager_destroy(pz_tank_manager *mgr, pz_renderer *renderer);

// Spawn a new tank (returns tank pointer, or NULL if no slots)
pz_tank *pz_tank_spawn(
    pz_tank_manager *mgr, pz_vec2 pos, pz_vec4 color, bool is_player);

// Get tank by ID (returns NULL if not found or inactive)
pz_tank *pz_tank_get_by_id(pz_tank_manager *mgr, int id);

// Get player tank (first tank with PLAYER flag, or NULL)
pz_tank *pz_tank_get_player(pz_tank_manager *mgr);

// Iterate active tanks
typedef void (*pz_tank_iter_fn)(pz_tank *tank, void *user_data);
void pz_tank_foreach(pz_tank_manager *mgr, pz_tank_iter_fn fn, void *user_data);

/* ============================================================================
 * Tank Update
 * ============================================================================
 */

// Update a single tank with input
void pz_tank_update(pz_tank_manager *mgr, pz_tank *tank,
    const pz_tank_input *input, const pz_map *map, float dt);

// Update all tanks (for AI/respawn)
void pz_tank_update_all(pz_tank_manager *mgr, const pz_map *map, float dt);

/* ============================================================================
 * Combat
 * ============================================================================
 */

// Apply damage to a tank (returns true if tank was killed)
bool pz_tank_damage(pz_tank *tank, int amount);

// Check if a circle at pos with radius hits any tank (except exclude_id)
// Returns the tank that was hit, or NULL
pz_tank *pz_tank_check_collision(
    pz_tank_manager *mgr, pz_vec2 pos, float radius, int exclude_id);

// Respawn a dead tank at its spawn point
void pz_tank_respawn(pz_tank *tank);

/* ============================================================================
 * Rendering
 * ============================================================================
 */

// Render all active tanks
void pz_tank_render(pz_tank_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection);

/* ============================================================================
 * Utility
 * ============================================================================
 */

// Get barrel tip position for a tank (for projectile spawning)
pz_vec2 pz_tank_get_barrel_tip(const pz_tank *tank);

// Get fire direction (normalized)
pz_vec2 pz_tank_get_fire_direction(const pz_tank *tank);

// Get count of active (non-dead) tanks
int pz_tank_count_active(const pz_tank_manager *mgr);

#endif // PZ_TANK_H
