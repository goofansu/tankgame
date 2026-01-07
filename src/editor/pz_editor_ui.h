/*
 * Tank Game - Editor UI System
 *
 * Immediate-mode style UI for the map editor.
 * Call ui functions between pz_editor_ui_begin() and pz_editor_ui_end().
 */

#ifndef PZ_EDITOR_UI_H
#define PZ_EDITOR_UI_H

#include "../core/pz_math.h"
#include "../engine/pz_font.h"
#include "../engine/render/pz_renderer.h"
#include <stdbool.h>

/* ============================================================================
 * Configuration
 * ============================================================================
 */

#define PZ_UI_MAX_QUADS 1024
#define PZ_UI_MAX_PANELS 16
#define PZ_UI_PANEL_TITLE_HEIGHT 24.0f
#define PZ_UI_BUTTON_PADDING 8.0f
#define PZ_UI_PANEL_PADDING 8.0f

/* ============================================================================
 * Colors (can be customized)
 * ============================================================================
 */

typedef struct pz_ui_colors {
    pz_vec4 panel_bg;
    pz_vec4 panel_title_bg;
    pz_vec4 panel_border;
    pz_vec4 button_bg;
    pz_vec4 button_hover;
    pz_vec4 button_active;
    pz_vec4 button_border;
    pz_vec4 text;
    pz_vec4 text_dim;
    pz_vec4 slot_empty;
    pz_vec4 slot_filled;
    pz_vec4 slot_selected;
} pz_ui_colors;

/* ============================================================================
 * Types
 * ============================================================================
 */

typedef struct pz_editor_ui pz_editor_ui;

// Mouse state for UI interaction
typedef struct pz_ui_mouse {
    float x, y; // Current position
    bool left_down; // Left button currently held
    bool left_clicked; // Left button just clicked this frame
    bool left_released; // Left button just released this frame
    bool right_clicked; // Right button just clicked
    float scroll; // Scroll wheel delta
} pz_ui_mouse;

// UI result flags
typedef enum pz_ui_result {
    PZ_UI_NONE = 0,
    PZ_UI_HOVERED = 1 << 0,
    PZ_UI_CLICKED = 1 << 1,
    PZ_UI_ACTIVE = 1 << 2,
    PZ_UI_CHANGED = 1 << 3,
} pz_ui_result;

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

// Create UI context
pz_editor_ui *pz_editor_ui_create(
    pz_renderer *renderer, pz_font_manager *font_mgr);

// Destroy UI context
void pz_editor_ui_destroy(pz_editor_ui *ui);

/* ============================================================================
 * Frame Management
 * ============================================================================
 */

// Begin UI frame - call before any UI functions
void pz_editor_ui_begin(
    pz_editor_ui *ui, int screen_width, int screen_height, pz_ui_mouse mouse);

// End UI frame - flushes all rendering
void pz_editor_ui_end(pz_editor_ui *ui);

// Check if UI consumed input this frame (for blocking game input)
bool pz_editor_ui_wants_mouse(pz_editor_ui *ui);
bool pz_editor_ui_wants_keyboard(pz_editor_ui *ui);

// Enable/disable widget input handling (visuals still render)
void pz_editor_ui_set_input_enabled(pz_editor_ui *ui, bool enabled);

// Manually consume mouse input for custom hit regions
void pz_ui_consume_mouse(pz_editor_ui *ui);

/* ============================================================================
 * Widgets
 * ============================================================================
 */

// Simple button with text label
// Returns true if clicked
bool pz_ui_button(
    pz_editor_ui *ui, float x, float y, float w, float h, const char *label);

// Text label (no interaction)
void pz_ui_label(
    pz_editor_ui *ui, float x, float y, const char *text, pz_vec4 color);

// Text label that truncates to fit max width (adds "..." when needed)
void pz_ui_label_fit(pz_editor_ui *ui, float x, float y, float max_w,
    const char *text, pz_vec4 color);

// Centered text label
void pz_ui_label_centered(pz_editor_ui *ui, float x, float y, float w, float h,
    const char *text, pz_vec4 color);

// Filled rectangle
void pz_ui_rect(
    pz_editor_ui *ui, float x, float y, float w, float h, pz_vec4 color);

// Rectangle outline
void pz_ui_rect_outline(pz_editor_ui *ui, float x, float y, float w, float h,
    pz_vec4 color, float thickness);

// Shortcut bar slot (for tile/tag display)
// Returns PZ_UI_CLICKED if clicked, PZ_UI_HOVERED if hovered
// label: slot number (e.g., "1"), content_label: tile/tag name (can be NULL)
int pz_ui_slot(pz_editor_ui *ui, float x, float y, float size, bool selected,
    bool filled, const char *label, const char *content_label,
    pz_vec4 preview_color);

// Textured tile slot - shows wall/ground textures in diagonal split pattern
// Returns PZ_UI_CLICKED if clicked, PZ_UI_HOVERED if hovered
int pz_ui_slot_textured(pz_editor_ui *ui, float x, float y, float size,
    bool selected, const char *label, pz_texture_handle wall_texture,
    pz_texture_handle ground_texture);

/* ============================================================================
 * Clipping
 * ============================================================================
 */

// Clip subsequent UI rendering to the given rect (screen space).
// Returns true if the clip area is non-empty.
bool pz_ui_clip_begin(pz_editor_ui *ui, float x, float y, float w, float h);
void pz_ui_clip_end(pz_editor_ui *ui);

/* ============================================================================
 * Panels (Modal Dialogs)
 * ============================================================================
 */

// Begin a panel/dialog
// Returns true if panel is visible (continue adding content)
bool pz_ui_panel_begin(pz_editor_ui *ui, const char *title, float x, float y,
    float w, float h, bool *open);

// End panel
void pz_ui_panel_end(pz_editor_ui *ui);

// Get content area of current panel (after title bar)
void pz_ui_panel_content_area(
    pz_editor_ui *ui, float *x, float *y, float *w, float *h);

/* ============================================================================
 * Windows (Draggable Dialogs)
 * ============================================================================
 */

// Window state - store this in your editor struct
typedef struct pz_ui_window_state {
    float x, y; // Window position (0,0 = auto-center)
    bool dragging;
    float drag_offset_x, drag_offset_y;
    int z_order;
} pz_ui_window_state;

// Window result with content area info
typedef struct pz_ui_window_result {
    bool visible; // Window is open and should render content
    float content_x, content_y; // Top-left of content area
    float content_w, content_h; // Size of content area
} pz_ui_window_result;

// Draw a draggable window with title bar and close button
// Handles dragging, centering, and mouse consumption
// Returns info about content area; check result.visible before drawing content
pz_ui_window_result pz_ui_window(pz_editor_ui *ui, const char *title, float w,
    float h, bool *open, pz_ui_window_state *state, bool allow_input);

/* ============================================================================
 * Layout Helpers
 * ============================================================================
 */

// Position from screen edge (negative = from right/bottom)
float pz_ui_x(pz_editor_ui *ui, float x);
float pz_ui_y(pz_editor_ui *ui, float y);

// Get default colors
const pz_ui_colors *pz_ui_get_colors(pz_editor_ui *ui);

// Set custom colors
void pz_ui_set_colors(pz_editor_ui *ui, const pz_ui_colors *colors);

#endif // PZ_EDITOR_UI_H
