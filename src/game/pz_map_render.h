/*
 * Map Rendering System
 *
 * Handles rendering the map terrain with appropriate textures
 * for each tile type, including 3D wall geometry.
 *
 * Wall textures are now sourced from tile definitions via the tile registry,
 * allowing different wall appearances per tile type.
 */

#ifndef PZ_MAP_RENDER_H
#define PZ_MAP_RENDER_H

#include "../engine/render/pz_renderer.h"
#include "../engine/render/pz_texture.h"
#include "pz_map.h"
#include "pz_tile_registry.h"

// Forward declaration
typedef struct pz_map_renderer pz_map_renderer;

// Create a map renderer with tile registry for texture lookups
pz_map_renderer *pz_map_renderer_create(pz_renderer *renderer,
    pz_texture_manager *tex_manager, const pz_tile_registry *tile_registry);

// Destroy the map renderer
void pz_map_renderer_destroy(pz_map_renderer *mr);

// Set the map to render (generates mesh from map data)
void pz_map_renderer_set_map(pz_map_renderer *mr, const pz_map *map);

// Lighting parameters for rendering
typedef struct pz_map_render_params {
    // Track texture (optional)
    pz_texture_handle track_texture;
    float track_scale_x, track_scale_z;
    float track_offset_x, track_offset_z;

    // Light map texture (optional)
    pz_texture_handle light_texture;
    float light_scale_x, light_scale_z;
    float light_offset_x, light_offset_z;

    // Sun lighting (from map)
    bool has_sun;
    pz_vec3 sun_direction;
    pz_vec3 sun_color;

    // Animation time (for water)
    float time;
} pz_map_render_params;

// Render the map ground layer
// track_texture: optional track accumulation texture (0 = no tracks)
// track_scale/offset: transform from world XZ to track texture UV
void pz_map_renderer_draw_ground(pz_map_renderer *mr,
    const pz_mat4 *view_projection, const pz_map_render_params *params);

// Render the 3D wall geometry
void pz_map_renderer_draw_walls(pz_map_renderer *mr,
    const pz_mat4 *view_projection, const pz_map_render_params *params);

// Render everything (ground + walls)
void pz_map_renderer_draw(pz_map_renderer *mr, const pz_mat4 *view_projection,
    const pz_map_render_params *params);

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
