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
void pz_map_renderer_draw_ground(
    pz_map_renderer *mr, const pz_mat4 *view_projection);

// Render the 3D wall geometry
void pz_map_renderer_draw_walls(
    pz_map_renderer *mr, const pz_mat4 *view_projection);

// Render everything (ground + walls)
void pz_map_renderer_draw(pz_map_renderer *mr, const pz_mat4 *view_projection);

// Check for texture hot-reload
void pz_map_renderer_check_hot_reload(pz_map_renderer *mr);

#endif // PZ_MAP_RENDER_H
