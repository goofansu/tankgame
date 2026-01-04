/*
 * Tank Game - Mine System
 *
 * Mines that can be placed by tanks and triggered by proximity or shooting.
 */

#ifndef PZ_MINE_H
#define PZ_MINE_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"
#include "../engine/render/pz_renderer.h"
#include "pz_mesh.h"

// Forward declarations
typedef struct pz_map pz_map;
typedef struct pz_tank_manager pz_tank_manager;
typedef struct pz_projectile_manager pz_projectile_manager;

// Maximum number of active mines
#define PZ_MAX_MINES 32

// Maximum mine explosion events per frame
#define PZ_MAX_MINE_EXPLOSIONS 8

// Mine explosion event (for particle spawning)
typedef struct pz_mine_explosion {
    pz_vec2 pos; // Position of explosion
    int owner_id; // Who placed the mine (-1 for map-placed)
} pz_mine_explosion;

// Mine structure
typedef struct pz_mine {
    bool active; // Is this slot in use?

    pz_vec2 pos; // Position in world space (X, Z)
    int owner_id; // Who placed this mine (-1 for map-placed)

    float arm_timer; // Time until armed (0 = armed)
    float bob_offset; // For floating animation (random offset)
    float rotation; // Current rotation angle
} pz_mine;

// Mine manager
typedef struct pz_mine_manager {
    pz_mine mines[PZ_MAX_MINES];
    int active_count;

    // Explosion events from last update (for particle spawning)
    pz_mine_explosion explosions[PZ_MAX_MINE_EXPLOSIONS];
    int explosion_count;

    // Rendering resources
    pz_mesh *mesh;
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    bool render_ready;

    // Animation time
    float time;
} pz_mine_manager;

// Mine constants
#define PZ_MINE_ARM_TIME 0.5f // Time before mine becomes active
#define PZ_MINE_TRIGGER_RADIUS 0.8f // Radius for proximity trigger
#define PZ_MINE_DAMAGE_RADIUS 2.0f // Radius for explosion damage
#define PZ_MINE_DAMAGE 10 // Damage dealt by mine explosion
#define PZ_MINE_MAX_PER_TANK 2 // Maximum mines a tank can carry

/* ============================================================================
 * Manager API
 * ============================================================================
 */

// Create/destroy manager
pz_mine_manager *pz_mine_manager_create(pz_renderer *renderer);
void pz_mine_manager_destroy(pz_mine_manager *mgr, pz_renderer *renderer);

// Place a mine at a position
// Returns mine index, or -1 if no slots available
int pz_mine_place(pz_mine_manager *mgr, pz_vec2 pos, int owner_id);

// Update all mines (animation, proximity checks, explosions)
// Returns number of explosions this frame
int pz_mine_update(pz_mine_manager *mgr, pz_tank_manager *tank_mgr,
    pz_projectile_manager *projectile_mgr, float dt);

// Check if a projectile at pos hits any mine
// Returns true if a mine was hit (and triggers explosion)
bool pz_mine_check_projectile_hit(
    pz_mine_manager *mgr, pz_vec2 pos, float radius, pz_tank_manager *tank_mgr);

// Lighting parameters for mine rendering
typedef struct pz_mine_render_params {
    pz_texture_handle light_texture;
    float light_scale_x, light_scale_z;
    float light_offset_x, light_offset_z;
} pz_mine_render_params;

// Render all active mines
void pz_mine_render(pz_mine_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection, const pz_mine_render_params *params);

// Get number of active mines
int pz_mine_count(const pz_mine_manager *mgr);

// Get explosion events from the last update (for particle spawning)
int pz_mine_get_explosions(const pz_mine_manager *mgr,
    pz_mine_explosion *explosions, int max_explosions);

// Clear all mines (for level reset)
void pz_mine_clear_all(pz_mine_manager *mgr);

#endif // PZ_MINE_H
