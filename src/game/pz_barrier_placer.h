/*
 * Tank Game - Barrier Placer System
 *
 * Handles ghost preview rendering and barrier placement logic
 * for tanks with the barrier_placer weapon.
 */

#ifndef PZ_BARRIER_PLACER_H
#define PZ_BARRIER_PLACER_H

#include <stdbool.h>

#include "../core/pz_math.h"
#include "../engine/render/pz_renderer.h"

// Forward declarations
typedef struct pz_tank pz_tank;
typedef struct pz_map pz_map;
typedef struct pz_barrier_manager pz_barrier_manager;
typedef struct pz_tile_registry pz_tile_registry;

// Ghost preview state
typedef struct pz_barrier_ghost {
    pz_vec2 pos; // World position (grid-snapped)
    bool valid; // True if placement is allowed here
    bool visible; // True if ghost should be shown
} pz_barrier_ghost;

// Barrier placer renderer (for ghost preview)
typedef struct pz_barrier_placer_renderer {
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    pz_buffer_handle mesh_buffer;
    int mesh_vertex_count;
    bool render_ready;
    float tile_size;
} pz_barrier_placer_renderer;

/* ============================================================================
 * Ghost Position Calculation
 * ============================================================================
 */

// Calculate ghost position based on mouse cursor position
// Returns grid-snapped position clamped to max 3 tiles from tank
// cursor_world: mouse cursor position in world coordinates
pz_vec2 pz_barrier_placer_calc_ghost_pos(
    const pz_tank *tank, pz_vec2 cursor_world, float tile_size);

// Update ghost state (position, validity)
// cursor_world: mouse cursor position in world coordinates
void pz_barrier_placer_update_ghost(pz_barrier_ghost *ghost,
    const pz_tank *tank, const pz_map *map,
    const pz_barrier_manager *barrier_mgr, float tile_size,
    pz_vec2 cursor_world);

/* ============================================================================
 * Renderer
 * ============================================================================
 */

// Create ghost renderer
pz_barrier_placer_renderer *pz_barrier_placer_renderer_create(
    pz_renderer *renderer, float tile_size);

// Destroy ghost renderer
void pz_barrier_placer_renderer_destroy(
    pz_barrier_placer_renderer *bpr, pz_renderer *renderer);

// Render ghost preview
// tank_color: base color of the tank (for tinting)
void pz_barrier_placer_render_ghost(pz_barrier_placer_renderer *bpr,
    pz_renderer *renderer, const pz_mat4 *view_projection,
    const pz_barrier_ghost *ghost, pz_vec4 tank_color,
    const pz_tile_registry *tile_registry, const char *tile_name);

/* ============================================================================
 * Placement
 * ============================================================================
 */

// Attempt to place a barrier
// Returns barrier index if placed, -1 if failed
int pz_barrier_placer_place(pz_tank *tank, pz_barrier_manager *barrier_mgr,
    const pz_map *map, const pz_barrier_ghost *ghost, float tile_size);

#endif // PZ_BARRIER_PLACER_H
