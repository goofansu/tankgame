/*
 * Tank Game - Particle System
 *
 * Cel-shaded smoke/explosion effects inspired by Wind Waker.
 * Particles are billboarded quads with stylized cloud textures.
 */

#ifndef PZ_PARTICLE_H
#define PZ_PARTICLE_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"
#include "../engine/render/pz_renderer.h"

// Maximum particles active at once
#define PZ_MAX_PARTICLES 256

// Particle types
typedef enum pz_particle_type {
    PZ_PARTICLE_SMOKE, // Blue-gray smoke puff
    PZ_PARTICLE_IMPACT, // Quick flash on bullet impact
    PZ_PARTICLE_COUNT
} pz_particle_type;

// Individual particle
typedef struct pz_particle {
    bool active;
    pz_particle_type type;

    pz_vec3 pos; // World position
    pz_vec3 velocity; // Movement per second
    float rotation; // Rotation angle (radians)
    float rotation_speed; // Rotation per second

    float scale; // Current scale
    float scale_start; // Initial scale
    float scale_end; // Final scale (at end of life)

    float alpha; // Current alpha
    float alpha_start; // Initial alpha
    float alpha_end; // Final alpha

    pz_vec3 color; // Base color (can vary per particle)

    float lifetime; // Total lifetime
    float age; // Current age

    int variant; // Which sprite variant to use (0-3)
} pz_particle;

// Particle manager
typedef struct pz_particle_manager {
    pz_particle particles[PZ_MAX_PARTICLES];
    int active_count;

    // Rendering resources
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    pz_texture_handle smoke_texture;
    pz_buffer_handle quad_buffer;
    bool render_ready;
} pz_particle_manager;

// Configuration for spawning a group of smoke particles
typedef struct pz_smoke_config {
    pz_vec3 position; // Center of smoke effect
    int count; // Number of particles (4-12 typical)
    float spread; // How far particles spread from center
    float scale_min; // Minimum particle scale
    float scale_max; // Maximum particle scale
    float lifetime_min; // Minimum lifetime
    float lifetime_max; // Maximum lifetime
    float velocity_up; // Upward velocity
    float velocity_spread; // Horizontal velocity randomness
} pz_smoke_config;

// Default smoke configuration for bullet impacts
extern const pz_smoke_config PZ_SMOKE_BULLET_IMPACT;

// Default smoke configuration for tank hits
extern const pz_smoke_config PZ_SMOKE_TANK_HIT;

/* ============================================================================
 * API
 * ============================================================================
 */

// Create/destroy manager
pz_particle_manager *pz_particle_manager_create(pz_renderer *renderer);
void pz_particle_manager_destroy(
    pz_particle_manager *mgr, pz_renderer *renderer);

// Spawn a group of smoke particles
void pz_particle_spawn_smoke(
    pz_particle_manager *mgr, const pz_smoke_config *config);

// Spawn a single particle (for custom effects)
void pz_particle_spawn(pz_particle_manager *mgr, const pz_particle *template);

// Update all particles
void pz_particle_update(pz_particle_manager *mgr, float dt);

// Render all particles (call after opaque geometry, before UI)
// camera_right and camera_up are needed for billboarding
void pz_particle_render(pz_particle_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection, pz_vec3 camera_right, pz_vec3 camera_up);

// Get number of active particles
int pz_particle_count(const pz_particle_manager *mgr);

#endif // PZ_PARTICLE_H
