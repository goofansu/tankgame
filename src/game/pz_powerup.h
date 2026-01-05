/*
 * Tank Game - Powerup System
 *
 * Handles collectible powerups that modify tank weapons.
 */

#ifndef PZ_POWERUP_H
#define PZ_POWERUP_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"
#include "../engine/render/pz_renderer.h"
#include "pz_mesh.h"

// Forward declarations
typedef struct pz_tank pz_tank;
typedef struct pz_tank_manager pz_tank_manager;

// Maximum number of powerups
#define PZ_MAX_POWERUPS 16

// Powerup types
typedef enum pz_powerup_type {
    PZ_POWERUP_NONE = 0,
    PZ_POWERUP_MACHINE_GUN, // Faster firing, less damage, smaller bullets
    PZ_POWERUP_RICOCHET, // Bullets bounce twice, green, slightly faster
    PZ_POWERUP_BARRIER_PLACER, // Place destructible barriers
    PZ_POWERUP_COUNT
} pz_powerup_type;

// Barrier placer data (for barrier_placer powerups)
typedef struct pz_barrier_placer_data {
    char barrier_tile[32]; // Tile name for barriers
    float barrier_health; // Health for placed barriers
    int barrier_count; // Max barriers that can be placed at once
} pz_barrier_placer_data;

// Powerup structure
typedef struct pz_powerup {
    bool active; // Is this slot in use?
    bool collected; // Has this been picked up (waiting for respawn)?
    pz_powerup_type type; // Type of powerup

    pz_vec2 pos; // Position in world space (X, Z)
    float bob_offset; // For floating animation
    float rotation; // Current rotation angle

    float respawn_timer; // Time until respawn (when collected)
    float respawn_time; // How long until respawn

    // For barrier_placer type only
    pz_barrier_placer_data barrier_data;
} pz_powerup;

// Powerup manager
typedef struct pz_powerup_manager {
    pz_powerup powerups[PZ_MAX_POWERUPS];
    int active_count;

    // Rendering resources
    pz_mesh *mesh;
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    pz_pipeline_handle pipeline_transparent; // For translucent rendering
    bool render_ready;

    // Animation time (for flicker effects)
    float time;
} pz_powerup_manager;

/* ============================================================================
 * Weapon Properties (used by tank/projectile systems)
 * ============================================================================
 */

// Weapon stats for different powerups
typedef struct pz_weapon_stats {
    float fire_cooldown; // Time between shots
    float projectile_speed; // Bullet speed
    int damage; // Damage per hit
    int max_bounces; // Number of bounces
    float projectile_scale; // Visual scale of projectile
    pz_vec4 projectile_color; // Color of the projectile
    bool auto_fire; // If true, holding mouse fires continuously
    int max_active_projectiles; // Max bullets in flight at once
    float recoil_strength; // Velocity impulse applied to tank when firing
} pz_weapon_stats;

// Get weapon stats for a weapon type
const pz_weapon_stats *pz_weapon_get_stats(pz_powerup_type weapon);

// Default weapon type
#define PZ_WEAPON_DEFAULT PZ_POWERUP_NONE

/* ============================================================================
 * Manager API
 * ============================================================================
 */

// Create/destroy manager
pz_powerup_manager *pz_powerup_manager_create(pz_renderer *renderer);
void pz_powerup_manager_destroy(pz_powerup_manager *mgr, pz_renderer *renderer);

// Add a powerup at a position
int pz_powerup_add(pz_powerup_manager *mgr, pz_vec2 pos, pz_powerup_type type,
    float respawn_time);

// Add a barrier placer powerup with barrier configuration
int pz_powerup_add_barrier_placer(pz_powerup_manager *mgr, pz_vec2 pos,
    float respawn_time, const char *barrier_tile, float barrier_health,
    int barrier_count);

// Update all powerups (animation, respawn timers)
void pz_powerup_update(pz_powerup_manager *mgr, float dt);

// Check for tank collision with powerups
// Returns the type of powerup collected (or PZ_POWERUP_NONE)
pz_powerup_type pz_powerup_check_collection(
    pz_powerup_manager *mgr, pz_vec2 tank_pos, float tank_radius);

// Extended collection check that also returns barrier placer data
// barrier_data_out is only filled if the collected powerup is a barrier_placer
pz_powerup_type pz_powerup_check_collection_ex(pz_powerup_manager *mgr,
    pz_vec2 tank_pos, float tank_radius,
    pz_barrier_placer_data *barrier_data_out);

// Render all active powerups
void pz_powerup_render(pz_powerup_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection);

// Get number of active (visible) powerups
int pz_powerup_count(const pz_powerup_manager *mgr);

// Get powerup type name
const char *pz_powerup_type_name(pz_powerup_type type);

// Get powerup type from name string
// Returns PZ_POWERUP_NONE if not found
pz_powerup_type pz_powerup_type_from_name(const char *name);

// Get powerup flicker intensity (0-1) for light effects
float pz_powerup_get_flicker(const pz_powerup_manager *mgr, int index);

// Get powerup alpha (0-1) for rendering transparency
float pz_powerup_get_alpha(const pz_powerup_manager *mgr, int index);

#endif // PZ_POWERUP_H
