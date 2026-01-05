/*
 * Tank Game - Custom Cursor Rendering
 *
 * Renders custom cursors (crosshair for gameplay, arrow for menus)
 * with black outlines for visibility.
 */

#ifndef PZ_CURSOR_H
#define PZ_CURSOR_H

#include "../core/pz_math.h"
#include "render/pz_renderer.h"
#include <stdbool.h>

/* ============================================================================
 * Cursor Types
 * ============================================================================
 */

typedef enum pz_cursor_type {
    PZ_CURSOR_CROSSHAIR, // Gameplay cursor (circle + cross)
    PZ_CURSOR_ARROW, // Menu cursor (pointer arrow)
} pz_cursor_type;

/* ============================================================================
 * Cursor Context
 * ============================================================================
 */

typedef struct pz_cursor pz_cursor;

// Create cursor renderer (call after renderer is created)
pz_cursor *pz_cursor_create(pz_renderer *renderer);

// Destroy cursor renderer
void pz_cursor_destroy(pz_cursor *cursor);

/* ============================================================================
 * Cursor Control
 * ============================================================================
 */

// Set cursor type
void pz_cursor_set_type(pz_cursor *cursor, pz_cursor_type type);

// Set cursor position (screen coordinates)
void pz_cursor_set_position(pz_cursor *cursor, float x, float y);

// Set cursor visibility
void pz_cursor_set_visible(pz_cursor *cursor, bool visible);

// Check if cursor is visible
bool pz_cursor_is_visible(pz_cursor *cursor);

/* ============================================================================
 * Rendering
 * ============================================================================
 */

// Render the cursor (call last, after all other rendering)
void pz_cursor_render(pz_cursor *cursor);

#endif // PZ_CURSOR_H
