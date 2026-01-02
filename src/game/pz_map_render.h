/*
 * Map Rendering System
 *
 * Handles rendering the map terrain with appropriate textures
 * for each tile type, including 3D wall geometry.
 */

#ifndef PZ_MAP_RENDER_H
#define PZ_MAP_RENDER_H

#include "../engine/render/pz_renderer.h"
#include "../engine/render/pz_texture.h"
#include "pz_map.h"

// Forward declaration
typedef struct pz_map_renderer pz_map_renderer;

// Create a map renderer
pz_map_renderer *pz_map_renderer_create(
    pz_renderer *renderer, pz_texture_manager *tex_manager);

// Destroy the map renderer
void pz_map_renderer_destroy(pz_map_renderer *mr);

// Set the map to render (generates mesh from map data)
void pz_map_renderer_set_map(pz_map_renderer *mr, const pz_map *map);

// Render the map ground layer
// track_texture: optional track accumulation texture (0 = no tracks)
// track_scale/offset: transform from world XZ to track texture UV
void pz_map_renderer_draw_ground(pz_map_renderer *mr,
    const pz_mat4 *view_projection, pz_texture_handle track_texture,
    float track_scale_x, float track_scale_z, float track_offset_x,
    float track_offset_z);

// Render the 3D wall geometry
void pz_map_renderer_draw_walls(
    pz_map_renderer *mr, const pz_mat4 *view_projection);

// Render everything (ground + walls)
// track_texture: optional track accumulation texture (0 = no tracks)
// track_scale/offset: transform from world XZ to track texture UV
void pz_map_renderer_draw(pz_map_renderer *mr, const pz_mat4 *view_projection,
    pz_texture_handle track_texture, float track_scale_x, float track_scale_z,
    float track_offset_x, float track_offset_z);

// Check for texture hot-reload
void pz_map_renderer_check_hot_reload(pz_map_renderer *mr);

// ============================================================================
// Map Hot-Reload Support
// ============================================================================

// Opaque map hot-reload context
typedef struct pz_map_hot_reload pz_map_hot_reload;

// Create a hot-reload watcher for a map file
// The renderer's mesh will be updated when the file changes
pz_map_hot_reload *pz_map_hot_reload_create(
    const char *path, pz_map **map_ptr, pz_map_renderer *renderer);

// Destroy the hot-reload watcher
void pz_map_hot_reload_destroy(pz_map_hot_reload *hr);

// Check for changes and reload if needed
// Returns true if the map was reloaded
bool pz_map_hot_reload_check(pz_map_hot_reload *hr);

// Get the map file path being watched
const char *pz_map_hot_reload_get_path(const pz_map_hot_reload *hr);

#endif // PZ_MAP_RENDER_H
