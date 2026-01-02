/*
 * Tank Game - Debug Overlay
 *
 * Immediate-mode debug overlay for FPS, frame time graph, and debug text.
 * Toggle with F2.
 */

#ifndef PZ_DEBUG_OVERLAY_H
#define PZ_DEBUG_OVERLAY_H

#include "../core/pz_math.h"
#include "render/pz_renderer.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Debug Overlay Context
 * ============================================================================
 */

typedef struct pz_debug_overlay pz_debug_overlay;

// Create debug overlay (call after renderer is created)
pz_debug_overlay *pz_debug_overlay_create(pz_renderer *renderer);

// Destroy debug overlay
void pz_debug_overlay_destroy(pz_debug_overlay *overlay);

/* ============================================================================
 * Overlay Control
 * ============================================================================
 */

// Toggle overlay visibility
void pz_debug_overlay_toggle(pz_debug_overlay *overlay);

// Set overlay visibility
void pz_debug_overlay_set_visible(pz_debug_overlay *overlay, bool visible);

// Check if overlay is visible
bool pz_debug_overlay_is_visible(pz_debug_overlay *overlay);

/* ============================================================================
 * Frame Timing
 * ============================================================================
 */

// Call at the start of each frame to update timing
void pz_debug_overlay_begin_frame(pz_debug_overlay *overlay);

// Call at the end of each frame (before swap)
void pz_debug_overlay_end_frame(pz_debug_overlay *overlay);

/* ============================================================================
 * Immediate-Mode Text (valid only between begin_frame and render)
 * ============================================================================
 */

// Draw text at screen position (in pixels, origin top-left)
void pz_debug_overlay_text(
    pz_debug_overlay *overlay, int x, int y, const char *fmt, ...);

// Draw text with color
void pz_debug_overlay_text_color(pz_debug_overlay *overlay, int x, int y,
    pz_vec4 color, const char *fmt, ...);

/* ============================================================================
 * Rendering
 * ============================================================================
 */

// Render the overlay (call after all other rendering, before swap)
void pz_debug_overlay_render(pz_debug_overlay *overlay);

/* ============================================================================
 * Stats Access
 * ============================================================================
 */

// Get current FPS
float pz_debug_overlay_get_fps(pz_debug_overlay *overlay);

// Get current frame time in milliseconds
float pz_debug_overlay_get_frame_time_ms(pz_debug_overlay *overlay);

#endif // PZ_DEBUG_OVERLAY_H
