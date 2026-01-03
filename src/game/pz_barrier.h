/*
 * Tank Game - Destructible Barrier System
 *
 * Barriers are tile-sized objects that block movement, projectiles, and light
 * until destroyed. They render using tile textures (like walls) but are placed
 * via map tags like powerups or spawn points.
 *
 * When destroyed, barriers play an explosion effect and become passable.
 */

#ifndef PZ_BARRIER_H
#define PZ_BARRIER_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"
#include "../engine/render/pz_renderer.h"
#include "../engine/render/pz_texture.h"
#include "pz_tile_registry.h"

// Maximum number of barriers per map
#define PZ_MAX_BARRIERS 32

// Forward declarations
typedef struct pz_map pz_map;
typedef struct pz_particle_manager pz_particle_manager;
typedef struct pz_lighting pz_lighting;

// Barrier structure
typedef struct pz_barrier {
    bool active; // Is this slot in use?
    bool destroyed; // Has this barrier been destroyed?

    pz_vec2 pos; // World position (center of tile)
    float health; // Current health
    float max_health; // Starting health

    char tile_name[32]; // Tile name for texture lookup

    // Destruction animation state
    float destroy_timer; // Counts down during destruction effect
} pz_barrier;

// Barrier manager
typedef struct pz_barrier_manager {
    pz_barrier barriers[PZ_MAX_BARRIERS];
    int active_count;

    // Tile registry for texture lookups
    const pz_tile_registry *tile_registry;

    // Rendering resources
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    pz_buffer_handle mesh_buffer;
    int mesh_vertex_count;
    bool render_ready;

    // Cached tile size from map (for mesh generation)
    float tile_size;
} pz_barrier_manager;

/* ============================================================================
 * Manager API
 * ============================================================================
 */

// Create/destroy manager
pz_barrier_manager *pz_barrier_manager_create(pz_renderer *renderer,
    const pz_tile_registry *tile_registry, float tile_size);
void pz_barrier_manager_destroy(pz_barrier_manager *mgr, pz_renderer *renderer);

// Add a barrier at a position
// Returns barrier index, or -1 if full
int pz_barrier_add(
    pz_barrier_manager *mgr, pz_vec2 pos, const char *tile_name, float health);

// Update all barriers (destruction timers, etc.)
void pz_barrier_update(pz_barrier_manager *mgr, float dt);

// Apply damage to a barrier at a position
// Returns true if a barrier was hit
// If destroyed is not NULL, sets *destroyed = true if the barrier was destroyed
bool pz_barrier_apply_damage(
    pz_barrier_manager *mgr, pz_vec2 pos, float damage, bool *destroyed);

// Check if a world position is blocked by a barrier
// Returns the barrier if blocked, NULL otherwise
pz_barrier *pz_barrier_check_collision(
    pz_barrier_manager *mgr, pz_vec2 pos, float radius);

// Resolve tank-barrier collision by pushing the tank out
// Modifies pos in place, returns true if collision occurred
bool pz_barrier_resolve_collision(
    pz_barrier_manager *mgr, pz_vec2 *pos, float radius);

// Check if a line segment hits a barrier (for projectile raycast)
// Returns true if hit, fills out hit_pos and hit_normal
// barrier_out receives the hit barrier (can be NULL if not needed)
bool pz_barrier_raycast(pz_barrier_manager *mgr, pz_vec2 start, pz_vec2 end,
    pz_vec2 *hit_pos, pz_vec2 *hit_normal, pz_barrier **barrier_out);

// Add barrier occluders to lighting system
void pz_barrier_add_occluders(pz_barrier_manager *mgr, pz_lighting *lighting);

// Render all active barriers
// Lighting parameters for lit rendering
typedef struct pz_barrier_render_params {
    pz_texture_handle light_texture;
    float light_scale_x, light_scale_z;
    float light_offset_x, light_offset_z;
    pz_vec3 ambient;
    bool has_sun;
    pz_vec3 sun_direction;
    pz_vec3 sun_color;
} pz_barrier_render_params;

void pz_barrier_render(pz_barrier_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection, const pz_barrier_render_params *params);

// Get number of active (not destroyed) barriers
int pz_barrier_count(const pz_barrier_manager *mgr);

// Clear all barriers (for map reload)
void pz_barrier_clear(pz_barrier_manager *mgr);

#endif // PZ_BARRIER_H
