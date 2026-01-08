/*
 * Tank Game - Spawn Indicator System
 *
 * Renders a visual indicator above tanks when they spawn/respawn.
 * Shows a colored circle with "P1", "P2", etc. and a line pointing to the tank.
 */

#ifndef PZ_SPAWN_INDICATOR_H
#define PZ_SPAWN_INDICATOR_H

#include "../core/pz_math.h"
#include "../engine/pz_camera.h"
#include "../engine/pz_font.h"
#include "../engine/render/pz_renderer.h"
#include "pz_tank.h"
#include <stdbool.h>

/* ============================================================================
 * Configuration
 * ============================================================================
 */

#define PZ_SPAWN_INDICATOR_DURATION 1.5f // Duration in seconds
#define PZ_SPAWN_INDICATOR_CIRCLE_RADIUS 28.0f // Circle radius in pixels
#define PZ_SPAWN_INDICATOR_LINE_WIDTH 4.0f // Line thickness
#define PZ_SPAWN_INDICATOR_HEIGHT 80.0f // Height above tank in screen pixels
#define PZ_SPAWN_INDICATOR_POINTER_SIZE 12.0f // Size of pointer triangle

/* ============================================================================
 * Types
 * ============================================================================
 */

typedef struct pz_spawn_indicator_renderer pz_spawn_indicator_renderer;

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

// Create spawn indicator renderer
pz_spawn_indicator_renderer *pz_spawn_indicator_create(pz_renderer *renderer);

// Destroy spawn indicator renderer
void pz_spawn_indicator_destroy(
    pz_spawn_indicator_renderer *sir, pz_renderer *renderer);

/* ============================================================================
 * Rendering
 * ============================================================================
 */

// Render spawn indicators for all active tanks
// Call this during the HUD pass (after font_begin_frame, before font_end_frame)
void pz_spawn_indicator_render(pz_spawn_indicator_renderer *sir,
    pz_renderer *renderer, pz_font_manager *font_mgr, pz_font *font,
    pz_tank_manager *tank_mgr, const pz_camera *camera, int screen_width,
    int screen_height);

#endif // PZ_SPAWN_INDICATOR_H
