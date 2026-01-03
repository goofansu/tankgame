/*
 * Tank Game - Font System
 *
 * SDF-based TrueType font rendering with atlas caching.
 * All text rendering goes through this API - no direct fontstash/stb_truetype
 * usage.
 */

#ifndef PZ_FONT_H
#define PZ_FONT_H

#include "../core/pz_math.h"
#include "render/pz_renderer.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Configuration
 * ============================================================================
 */

#define PZ_FONT_ATLAS_SIZE 1024 // Atlas texture size (square)
#define PZ_FONT_MAX_GLYPHS 256 // Max cached glyphs per font
#define PZ_FONT_SDF_PADDING 8 // Padding around SDF glyphs
#define PZ_FONT_SDF_ONEDGE 128 // SDF edge value (0-255)
#define PZ_FONT_SDF_SCALE 32.0f // SDF pixel distance per unit

/* ============================================================================
 * Alignment
 * ============================================================================
 */

typedef enum pz_font_align_h {
    PZ_FONT_ALIGN_LEFT = 0,
    PZ_FONT_ALIGN_CENTER,
    PZ_FONT_ALIGN_RIGHT,
} pz_font_align_h;

typedef enum pz_font_align_v {
    PZ_FONT_ALIGN_TOP = 0,
    PZ_FONT_ALIGN_MIDDLE,
    PZ_FONT_ALIGN_BOTTOM,
    PZ_FONT_ALIGN_BASELINE,
} pz_font_align_v;

/* ============================================================================
 * Types
 * ============================================================================
 */

// Opaque font handle
typedef struct pz_font pz_font;

// Opaque font manager handle
typedef struct pz_font_manager pz_font_manager;

// Text style for rendering
typedef struct pz_text_style {
    pz_font *font;
    float size; // Font size in pixels
    pz_vec4 color; // RGBA color
    pz_font_align_h align_h;
    pz_font_align_v align_v;
    float outline_width; // 0 = no outline
    pz_vec4 outline_color;
} pz_text_style;

// Text bounds result
typedef struct pz_text_bounds {
    float x, y; // Top-left corner
    float width, height;
} pz_text_bounds;

/* ============================================================================
 * Font Manager Lifecycle
 * ============================================================================
 */

// Create font manager (owns renderer reference)
pz_font_manager *pz_font_manager_create(pz_renderer *renderer);

// Destroy font manager and all loaded fonts
void pz_font_manager_destroy(pz_font_manager *mgr);

/* ============================================================================
 * Font Loading
 * ============================================================================
 */

// Load a font from a TTF file
// Returns NULL on failure
pz_font *pz_font_load(pz_font_manager *mgr, const char *path);

// Get a font by name (filename without extension)
// Returns NULL if not found
pz_font *pz_font_get(pz_font_manager *mgr, const char *name);

// Destroy a specific font
void pz_font_destroy(pz_font_manager *mgr, pz_font *font);

/* ============================================================================
 * Text Measurement
 * ============================================================================
 */

// Get bounds of text without rendering
pz_text_bounds pz_font_measure(const pz_text_style *style, const char *text);

// Get line height for a font at given size
float pz_font_line_height(pz_font *font, float size);

// Get baseline offset (distance from top to baseline)
float pz_font_baseline(pz_font *font, float size);

/* ============================================================================
 * Text Rendering
 *
 * Call these between pz_font_begin_frame() and pz_font_end_frame()
 * ============================================================================
 */

// Begin text rendering for this frame
void pz_font_begin_frame(pz_font_manager *mgr);

// Draw text at position (screen coordinates)
void pz_font_draw(pz_font_manager *mgr, const pz_text_style *style, float x,
    float y, const char *text);

// Draw formatted text
void pz_font_drawf(pz_font_manager *mgr, const pz_text_style *style, float x,
    float y, const char *fmt, ...);

// End text rendering (flushes batched quads)
void pz_font_end_frame(pz_font_manager *mgr);

/* ============================================================================
 * Convenience Macros
 * ============================================================================
 */

// Create a default text style
#define PZ_TEXT_STYLE_DEFAULT(font_, size_)                                    \
    (pz_text_style)                                                            \
    {                                                                          \
        .font = (font_), .size = (size_), .color = pz_vec4_new(1, 1, 1, 1),    \
        .align_h = PZ_FONT_ALIGN_LEFT, .align_v = PZ_FONT_ALIGN_BASELINE,      \
        .outline_width = 0, .outline_color = pz_vec4_new(0, 0, 0, 1)           \
    }

#endif // PZ_FONT_H
