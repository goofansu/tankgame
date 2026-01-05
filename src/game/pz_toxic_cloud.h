/*
 * Tank Game - Toxic Cloud System
 *
 * Battle royale-style closing toxic cloud with a rounded-rectangle boundary.
 */

#ifndef PZ_TOXIC_CLOUD_H
#define PZ_TOXIC_CLOUD_H

#include <stdbool.h>

#include "../core/pz_math.h"

// Default config values
#define PZ_TOXIC_DEFAULT_DELAY 10.0f
#define PZ_TOXIC_DEFAULT_DURATION 90.0f
#define PZ_TOXIC_DEFAULT_SAFE_ZONE_RATIO 0.20f
#define PZ_TOXIC_DEFAULT_DAMAGE 1
#define PZ_TOXIC_DEFAULT_DAMAGE_INTERVAL 5.0f
#define PZ_TOXIC_DEFAULT_SLOWDOWN 0.70f
#define PZ_TOXIC_DEFAULT_GRACE_PERIOD 3.0f

// Configuration (from map file)
typedef struct pz_toxic_cloud_config {
    bool enabled;
    float delay; // Seconds before closing starts
    float duration; // Seconds to reach final size
    float safe_zone_ratio; // Final safe zone as ratio of map (0-1)
    int damage; // Damage per tick
    float damage_interval; // Seconds between damage ticks
    float slowdown; // Speed multiplier when inside
    pz_vec3 color; // Cloud color RGB (0-1)
    pz_vec2 center; // World-space center point
    float grace_period; // Respawn invulnerability
} pz_toxic_cloud_config;

// Runtime state
typedef struct pz_toxic_cloud {
    pz_toxic_cloud_config config;
    float elapsed; // Total time since map start
    float closing_progress; // 0.0 = full map safe, 1.0 = at safe zone
    bool closing_started; // True after delay has passed
    float spawn_timer; // Particle spawn accumulator

    // Cached boundary (updated each frame)
    float boundary_left;
    float boundary_right;
    float boundary_top;
    float boundary_bottom;
    float corner_radius;

    // Map reference for bounds
    float map_width;
    float map_height;
    pz_vec2 map_center;
} pz_toxic_cloud;

// Default config for a map center (enabled = false).
pz_toxic_cloud_config pz_toxic_cloud_config_default(pz_vec2 map_center);

// Lifecycle
pz_toxic_cloud *pz_toxic_cloud_create(
    const pz_toxic_cloud_config *config, float map_width, float map_height);
void pz_toxic_cloud_destroy(pz_toxic_cloud *cloud);

// Update (call each frame)
void pz_toxic_cloud_update(pz_toxic_cloud *cloud, float dt);

// Queries
// Returns true when the position is in the toxic zone (outside the safe area).
bool pz_toxic_cloud_is_inside(const pz_toxic_cloud *cloud, pz_vec2 pos);
bool pz_toxic_cloud_is_damaging(const pz_toxic_cloud *cloud, pz_vec2 pos);
float pz_toxic_cloud_get_progress(const pz_toxic_cloud *cloud);

// Get boundary info for rendering
void pz_toxic_cloud_get_boundary(const pz_toxic_cloud *cloud, float *left,
    float *right, float *top, float *bottom, float *corner_radius);

// Direction to nearest safe zone (for AI escape)
pz_vec2 pz_toxic_cloud_escape_direction(
    const pz_toxic_cloud *cloud, pz_vec2 pos);

// Distance to the safe zone boundary (negative if inside safe zone, positive
// if in toxic zone). For AI to anticipate incoming cloud.
float pz_toxic_cloud_distance_to_boundary(
    const pz_toxic_cloud *cloud, pz_vec2 pos);

// Get a safe position inside the safe zone given a starting position.
// Returns a position that is safely inside the current safe zone.
// margin: extra distance from the boundary edge to stay safe.
pz_vec2 pz_toxic_cloud_get_safe_position(
    const pz_toxic_cloud *cloud, pz_vec2 from, float margin);

// Check if a position will be inside the toxic zone at a future progress level.
// Used for AI to predict where the cloud will be.
bool pz_toxic_cloud_will_be_inside(
    const pz_toxic_cloud *cloud, pz_vec2 pos, float future_progress);

// Particle rendering helper
typedef struct pz_particle_manager pz_particle_manager;
void pz_toxic_cloud_spawn_particles(
    pz_toxic_cloud *cloud, pz_particle_manager *particles, float dt);

#endif // PZ_TOXIC_CLOUD_H
