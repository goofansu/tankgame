/*
 * Tank Game - Background Renderer
 *
 * Renders the sky/background behind the map.
 * Supports solid color, vertical gradient, and radial gradient.
 */

#ifndef PZ_BACKGROUND_H
#define PZ_BACKGROUND_H

#include <stdbool.h>

#include "../core/pz_math.h"
#include "../engine/render/pz_renderer.h"
#include "pz_map.h"

// Opaque background renderer
typedef struct pz_background pz_background;

/* ============================================================================
 * API
 * ============================================================================
 */

// Create/destroy
pz_background *pz_background_create(pz_renderer *renderer);
void pz_background_destroy(pz_background *bg, pz_renderer *renderer);

// Configure from map settings
void pz_background_set_from_map(pz_background *bg, const pz_map *map);

// Configure manually
void pz_background_set_color(pz_background *bg, pz_vec3 color);
void pz_background_set_gradient(pz_background *bg, pz_vec3 color_start,
    pz_vec3 color_end, pz_gradient_direction direction);

// Render (call before any other rendering, after clear)
// viewport_width/height needed for aspect ratio correction in radial mode
void pz_background_render(pz_background *bg, pz_renderer *renderer,
    int viewport_width, int viewport_height);

#endif // PZ_BACKGROUND_H
