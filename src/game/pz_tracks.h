/*
 * Tank Track Accumulation System
 *
 * Renders persistent tank tracks on the ground using an FBO-based
 * accumulation texture. Tracks are rendered as textured quads that
 * blend (darken) into the accumulation texture.
 *
 * Usage:
 *   1. Create with pz_tracks_create()
 *   2. Each frame, call pz_tracks_add_mark() when tank moves
 *   3. Before rendering ground, get texture with pz_tracks_get_texture()
 *   4. Ground shader samples this texture using world coordinates
 */

#ifndef PZ_TRACKS_H
#define PZ_TRACKS_H

#include "../engine/render/pz_renderer.h"
#include "../engine/render/pz_texture.h"

// Forward declaration
typedef struct pz_tracks pz_tracks;

// Configuration for track system
typedef struct pz_tracks_config {
    float world_width; // Width of the map in world units
    float world_height; // Height (depth) of the map in world units
    int texture_size; // Resolution of accumulation texture (e.g., 1024)
} pz_tracks_config;

// Create the track accumulation system
pz_tracks *pz_tracks_create(pz_renderer *renderer,
    pz_texture_manager *tex_manager, const pz_tracks_config *config);

// Destroy the track system
void pz_tracks_destroy(pz_tracks *tracks);

// Add a track mark at the given position for a specific entity
// - entity_id: unique ID for this entity (e.g., tank ID)
// - pos_x, pos_z: world position of the tank center
// - angle: tank body angle in radians (direction it's facing)
// - tread_offset: distance from center to each tread (typically tank_width/2)
// Call this when the tank has moved enough distance
void pz_tracks_add_mark(pz_tracks *tracks, int entity_id, float pos_x,
    float pos_z, float angle, float tread_offset);

// Clear track state for a specific entity (e.g., when entity dies/respawns)
void pz_tracks_clear_entity(pz_tracks *tracks, int entity_id);

// Update track rendering - call once per frame before ground rendering
// Renders any pending track marks into the accumulation texture
void pz_tracks_update(pz_tracks *tracks);

// Get the accumulation texture for ground rendering
// This texture contains darkened areas where tracks have been laid
pz_texture_handle pz_tracks_get_texture(pz_tracks *tracks);

// Get world-to-UV transformation info for the ground shader
// Returns the scale and offset to convert world XZ to texture UV
void pz_tracks_get_uv_transform(pz_tracks *tracks, float *out_scale_x,
    float *out_scale_z, float *out_offset_x, float *out_offset_z);

// Clear all tracks (e.g., on map reload)
void pz_tracks_clear(pz_tracks *tracks);

#endif // PZ_TRACKS_H
