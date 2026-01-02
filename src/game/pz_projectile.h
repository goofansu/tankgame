/*
 * Tank Game - Projectile System
 *
 * Handles bullets that can bounce off walls.
 */

#ifndef PZ_PROJECTILE_H
#define PZ_PROJECTILE_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"
#include "../engine/render/pz_renderer.h"
#include "pz_map.h"
#include "pz_mesh.h"

// Forward declaration
typedef struct pz_tank_manager pz_tank_manager;

// Maximum number of active projectiles
#define PZ_MAX_PROJECTILES 64

// Projectile structure
typedef struct pz_projectile {
    bool active; // Is this slot in use?

    pz_vec2 pos; // Position in world space (X, Z)
    pz_vec2 velocity; // Velocity vector
    float speed; // Movement speed (units/sec)

    int bounces_remaining; // How many more bounces before destruction
    float lifetime; // Time remaining before auto-destruct
    float age; // Time since spawned (for self-damage grace period)

    int owner_id; // Who fired this (for friendly fire checks)
    int damage; // Damage on hit
} pz_projectile;

// Projectile manager
typedef struct pz_projectile_manager {
    pz_projectile projectiles[PZ_MAX_PROJECTILES];
    int active_count;

    // Rendering resources
    pz_mesh *mesh;
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    bool render_ready;
} pz_projectile_manager;

// Configuration for projectile spawning
typedef struct pz_projectile_config {
    float speed; // Default: 15.0
    int max_bounces; // Default: 1
    float lifetime; // Default: 5.0 seconds
    int damage; // Default: 1
} pz_projectile_config;

// Default configuration
extern const pz_projectile_config PZ_PROJECTILE_DEFAULT;

/* ============================================================================
 * API
 * ============================================================================
 */

// Create/destroy manager
pz_projectile_manager *pz_projectile_manager_create(pz_renderer *renderer);
void pz_projectile_manager_destroy(
    pz_projectile_manager *mgr, pz_renderer *renderer);

// Spawn a new projectile
// Returns the projectile index, or -1 if no slots available
int pz_projectile_spawn(pz_projectile_manager *mgr, pz_vec2 pos,
    pz_vec2 direction, const pz_projectile_config *config, int owner_id);

// Update all projectiles (movement, collision, bouncing)
// tank_mgr can be NULL if no tank collision is desired
void pz_projectile_update(pz_projectile_manager *mgr, const pz_map *map,
    pz_tank_manager *tank_mgr, float dt);

// Render all active projectiles
void pz_projectile_render(pz_projectile_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection);

// Get number of active projectiles
int pz_projectile_count(const pz_projectile_manager *mgr);

#endif // PZ_PROJECTILE_H
