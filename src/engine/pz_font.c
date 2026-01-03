/*
 * Tank Game - Font System Implementation
 *
 * Uses stb_truetype for SDF glyph generation with our own atlas management.
 */

#include "pz_font.h"
#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"

#include "third_party/stb_truetype.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Helper to copy string safely
static void
font_strlcpy(char *dst, const char *src, size_t size)
{
    if (size == 0)
        return;
    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

/* ============================================================================
 * Constants
 * ============================================================================
 */

#define MAX_FONTS 16
#define MAX_QUADS_PER_FRAME 4096
#define SDF_SIZE 48 // Base size for SDF generation (larger = better quality)

/* ============================================================================
 * Internal Types
 * ============================================================================
 */

// Cached glyph info
typedef struct pz_glyph {
    int codepoint;
    bool valid;

    // Atlas position (in pixels)
    int atlas_x, atlas_y;
    int atlas_w, atlas_h;

    // Glyph metrics (at SDF_SIZE scale)
    float x_offset, y_offset; // Offset from cursor to glyph top-left
    float x_advance; // Cursor advance after this glyph
    float width, height; // Glyph bounding box
} pz_glyph;

// Font instance
struct pz_font {
    char name[64];
    uint8_t *ttf_data; // Raw TTF file data
    stbtt_fontinfo info; // STB font info

    // Font metrics (at scale = 1)
    float ascent;
    float descent;
    float line_gap;
    float scale; // Scale to convert to SDF_SIZE

    // Glyph cache (indexed by codepoint, ASCII only for now)
    pz_glyph glyphs[PZ_FONT_MAX_GLYPHS];
    int glyph_count;

    // SDF atlas position
    int atlas_cursor_x;
    int atlas_cursor_y;
    int atlas_row_height;
    bool atlas_dirty;
};

// Font vertex for rendering
typedef struct pz_font_vertex {
    float x, y; // Screen position
    float u, v; // Texture coords
    float r, g, b, a; // Color
} pz_font_vertex;

// Font manager
struct pz_font_manager {
    pz_renderer *renderer;

    // Loaded fonts
    pz_font *fonts[MAX_FONTS];
    int font_count;

    // Shared SDF atlas
    uint8_t *atlas_data;
    pz_texture_handle atlas_texture;
    bool atlas_dirty;

    // Per-frame vertex batching
    pz_font_vertex *vertices;
    size_t vertex_count;
    size_t vertex_capacity;

    // Rendering resources
    pz_shader_handle shader;
    pz_buffer_handle vertex_buffer;
    pz_pipeline_handle pipeline;
    int screen_width, screen_height;
};

/* ============================================================================
 * Internal: UTF-8 Decoding
 * ============================================================================
 */

static uint32_t
decode_utf8(const char **str)
{
    const uint8_t *s = (const uint8_t *)*str;
    uint32_t c = *s;

    if (c < 0x80) {
        *str += 1;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        c = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        *str += 2;
        return c;
    } else if ((c & 0xF0) == 0xE0) {
        c = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *str += 3;
        return c;
    } else if ((c & 0xF8) == 0xF0) {
        c = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6)
            | (s[3] & 0x3F);
        *str += 4;
        return c;
    }

    *str += 1;
    return 0xFFFD; // Replacement character
}

/* ============================================================================
 * Internal: Glyph Management
 * ============================================================================
 */

static pz_glyph *
font_get_glyph(pz_font_manager *mgr, pz_font *font, int codepoint)
{
    // Simple ASCII range check
    if (codepoint < 0 || codepoint >= PZ_FONT_MAX_GLYPHS) {
        codepoint = '?';
    }

    pz_glyph *g = &font->glyphs[codepoint];
    if (g->valid) {
        return g;
    }

    // Generate SDF for this glyph
    int glyph_idx = stbtt_FindGlyphIndex(&font->info, codepoint);
    if (glyph_idx == 0 && codepoint != 0) {
        // Glyph not found, use replacement
        return font_get_glyph(mgr, font, '?');
    }

    // Get glyph metrics
    int advance, lsb;
    stbtt_GetGlyphHMetrics(&font->info, glyph_idx, &advance, &lsb);

    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(
        &font->info, glyph_idx, font->scale, font->scale, &x0, &y0, &x1, &y1);

    int glyph_w = x1 - x0;
    int glyph_h = y1 - y0;

    // Generate SDF bitmap
    int sdf_w = glyph_w + PZ_FONT_SDF_PADDING * 2;
    int sdf_h = glyph_h + PZ_FONT_SDF_PADDING * 2;

    if (sdf_w <= 0)
        sdf_w = 1;
    if (sdf_h <= 0)
        sdf_h = 1;

    // Check if there's room in the atlas
    if (font->atlas_cursor_x + sdf_w > PZ_FONT_ATLAS_SIZE) {
        // Move to next row
        font->atlas_cursor_x = 0;
        font->atlas_cursor_y += font->atlas_row_height;
        font->atlas_row_height = 0;
    }

    if (font->atlas_cursor_y + sdf_h > PZ_FONT_ATLAS_SIZE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "FONT: Atlas full, cannot add glyph %d", codepoint);
        return NULL;
    }

    // Generate SDF
    uint8_t *sdf_bitmap = stbtt_GetGlyphSDF(&font->info, font->scale, glyph_idx,
        PZ_FONT_SDF_PADDING, PZ_FONT_SDF_ONEDGE, PZ_FONT_SDF_SCALE, &sdf_w,
        &sdf_h, NULL, NULL);

    if (!sdf_bitmap) {
        // Empty glyph (like space)
        g->valid = true;
        g->codepoint = codepoint;
        g->atlas_x = 0;
        g->atlas_y = 0;
        g->atlas_w = 0;
        g->atlas_h = 0;
        g->x_offset = 0;
        g->y_offset = 0;
        g->width = 0;
        g->height = 0;
        g->x_advance = advance * font->scale;
        return g;
    }

    // Copy to atlas
    int ax = font->atlas_cursor_x;
    int ay = font->atlas_cursor_y;

    for (int y = 0; y < sdf_h; y++) {
        for (int x = 0; x < sdf_w; x++) {
            int atlas_idx = (ay + y) * PZ_FONT_ATLAS_SIZE + (ax + x);
            mgr->atlas_data[atlas_idx] = sdf_bitmap[y * sdf_w + x];
        }
    }

    stbtt_FreeSDF(sdf_bitmap, NULL);

    // Update atlas cursor
    font->atlas_cursor_x += sdf_w + 1;
    if (sdf_h > font->atlas_row_height) {
        font->atlas_row_height = sdf_h + 1;
    }

    // Fill glyph info
    g->valid = true;
    g->codepoint = codepoint;
    g->atlas_x = ax;
    g->atlas_y = ay;
    g->atlas_w = sdf_w;
    g->atlas_h = sdf_h;
    g->x_offset = x0 - PZ_FONT_SDF_PADDING;
    g->y_offset = y0 - PZ_FONT_SDF_PADDING;
    g->width = (float)sdf_w;
    g->height = (float)sdf_h;
    g->x_advance = advance * font->scale;

    mgr->atlas_dirty = true;
    font->glyph_count++;

    return g;
}

/* ============================================================================
 * Font Manager Lifecycle
 * ============================================================================
 */

pz_font_manager *
pz_font_manager_create(pz_renderer *renderer)
{
    pz_font_manager *mgr = pz_alloc(sizeof(pz_font_manager));
    memset(mgr, 0, sizeof(pz_font_manager));

    mgr->renderer = renderer;

    // Allocate atlas
    size_t atlas_size = PZ_FONT_ATLAS_SIZE * PZ_FONT_ATLAS_SIZE;
    mgr->atlas_data = pz_alloc(atlas_size);
    memset(mgr->atlas_data, 0, atlas_size);

    // Create atlas texture
    pz_texture_desc tex_desc = {
        .width = PZ_FONT_ATLAS_SIZE,
        .height = PZ_FONT_ATLAS_SIZE,
        .format = PZ_TEXTURE_R8,
        .filter = PZ_FILTER_LINEAR,
        .wrap = PZ_WRAP_CLAMP,
        .data = NULL, // Don't pass initial data - we'll update when dirty
    };
    mgr->atlas_texture = pz_renderer_create_texture(renderer, &tex_desc);
    mgr->atlas_dirty = false; // Will be set when glyphs are added

    // Allocate vertex buffer
    mgr->vertex_capacity = MAX_QUADS_PER_FRAME * 6; // 6 vertices per quad
    mgr->vertices = pz_alloc(mgr->vertex_capacity * sizeof(pz_font_vertex));

    // Create vertex buffer
    pz_buffer_desc buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STREAM,
        .data = NULL,
        .size = mgr->vertex_capacity * sizeof(pz_font_vertex),
    };
    mgr->vertex_buffer = pz_renderer_create_buffer(renderer, &buf_desc);

    // Get shader (loaded by sokol shader system)
    mgr->shader = pz_renderer_create_shader(
        renderer, &(pz_shader_desc) { .name = "sdf_text" });

    // Create pipeline
    pz_vertex_attr attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT2, .offset = 0 },
        { .name = "a_texcoord",
            .type = PZ_ATTR_FLOAT2,
            .offset = sizeof(float) * 2 },
        { .name = "a_color",
            .type = PZ_ATTR_FLOAT4,
            .offset = sizeof(float) * 4 },
    };

    pz_pipeline_desc pipe_desc = {
        .shader = mgr->shader,
        .vertex_layout =
            {
                .attrs = attrs,
                .attr_count = 3,
                .stride = sizeof(pz_font_vertex),
            },
        .blend = PZ_BLEND_ALPHA,
        .depth = PZ_DEPTH_NONE,
        .cull = PZ_CULL_NONE,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    mgr->pipeline = pz_renderer_create_pipeline(renderer, &pipe_desc);

    pz_renderer_get_viewport(renderer, &mgr->screen_width, &mgr->screen_height);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER,
        "FONT: Manager created (atlas %dx%d)", PZ_FONT_ATLAS_SIZE,
        PZ_FONT_ATLAS_SIZE);

    return mgr;
}

void
pz_font_manager_destroy(pz_font_manager *mgr)
{
    if (!mgr)
        return;

    // Destroy all fonts
    for (int i = 0; i < mgr->font_count; i++) {
        if (mgr->fonts[i]) {
            pz_free(mgr->fonts[i]->ttf_data);
            pz_free(mgr->fonts[i]);
        }
    }

    // Destroy rendering resources
    pz_renderer_destroy_pipeline(mgr->renderer, mgr->pipeline);
    pz_renderer_destroy_buffer(mgr->renderer, mgr->vertex_buffer);
    pz_renderer_destroy_shader(mgr->renderer, mgr->shader);
    pz_renderer_destroy_texture(mgr->renderer, mgr->atlas_texture);

    pz_free(mgr->atlas_data);
    pz_free(mgr->vertices);
    pz_free(mgr);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "FONT: Manager destroyed");
}

/* ============================================================================
 * Font Loading
 * ============================================================================
 */

pz_font *
pz_font_load(pz_font_manager *mgr, const char *path)
{
    if (mgr->font_count >= MAX_FONTS) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "FONT: Max fonts reached");
        return NULL;
    }

    // Load file
    size_t file_size;
    char *data = pz_file_read(path, &file_size);
    if (!data) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "FONT: Failed to load font: %s",
            path);
        return NULL;
    }

    // Initialize font
    pz_font *font = pz_alloc(sizeof(pz_font));
    memset(font, 0, sizeof(pz_font));

    font->ttf_data = (uint8_t *)data;

    if (!stbtt_InitFont(&font->info, font->ttf_data, 0)) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "FONT: Failed to parse font: %s", path);
        pz_free(data);
        pz_free(font);
        return NULL;
    }

    // Extract name from path
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    font_strlcpy(font->name, filename, sizeof(font->name));

    // Remove extension
    char *dot = strrchr(font->name, '.');
    if (dot)
        *dot = '\0';

    // Get font metrics
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);

    font->scale = stbtt_ScaleForPixelHeight(&font->info, SDF_SIZE);
    font->ascent = ascent * font->scale;
    font->descent = descent * font->scale;
    font->line_gap = line_gap * font->scale;

    // Register font
    mgr->fonts[mgr->font_count++] = font;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER,
        "FONT: Loaded font '%s' (ascent=%.1f, descent=%.1f)", font->name,
        font->ascent, font->descent);

    return font;
}

pz_font *
pz_font_get(pz_font_manager *mgr, const char *name)
{
    for (int i = 0; i < mgr->font_count; i++) {
        if (strcmp(mgr->fonts[i]->name, name) == 0) {
            return mgr->fonts[i];
        }
    }
    return NULL;
}

void
pz_font_destroy(pz_font_manager *mgr, pz_font *font)
{
    for (int i = 0; i < mgr->font_count; i++) {
        if (mgr->fonts[i] == font) {
            pz_free(font->ttf_data);
            pz_free(font);
            // Shift remaining fonts
            for (int j = i; j < mgr->font_count - 1; j++) {
                mgr->fonts[j] = mgr->fonts[j + 1];
            }
            mgr->font_count--;
            return;
        }
    }
}

/* ============================================================================
 * Text Measurement
 * ============================================================================
 */

pz_text_bounds
pz_font_measure(const pz_text_style *style, const char *text)
{
    pz_text_bounds bounds = { 0 };

    if (!style || !style->font || !text)
        return bounds;

    pz_font *font = style->font;
    float scale = style->size / SDF_SIZE;
    float x = 0;
    float min_y = 0, max_y = 0;

    const char *p = text;
    while (*p) {
        uint32_t cp = decode_utf8(&p);
        if (cp == '\n') {
            // For single-line measurement, stop at newline
            break;
        }

        int glyph_idx = stbtt_FindGlyphIndex(&font->info, (int)cp);
        int advance, lsb;
        stbtt_GetGlyphHMetrics(&font->info, glyph_idx, &advance, &lsb);

        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(&font->info, glyph_idx, font->scale,
            font->scale, &x0, &y0, &x1, &y1);

        if (y0 < min_y)
            min_y = (float)y0;
        if (y1 > max_y)
            max_y = (float)y1;

        x += advance * font->scale;
    }

    bounds.width = x * scale;
    bounds.height = (max_y - min_y) * scale;
    if (bounds.height < style->size)
        bounds.height = style->size;

    return bounds;
}

float
pz_font_line_height(pz_font *font, float size)
{
    if (!font)
        return size;
    float scale = size / SDF_SIZE;
    return (font->ascent - font->descent + font->line_gap) * scale;
}

float
pz_font_baseline(pz_font *font, float size)
{
    if (!font)
        return size;
    float scale = size / SDF_SIZE;
    return font->ascent * scale;
}

/* ============================================================================
 * Text Rendering
 * ============================================================================
 */

void
pz_font_begin_frame(pz_font_manager *mgr)
{
    mgr->vertex_count = 0;
    pz_renderer_get_viewport(
        mgr->renderer, &mgr->screen_width, &mgr->screen_height);
}

static void
push_quad(pz_font_manager *mgr, float x0, float y0, float x1, float y1,
    float u0, float v0, float u1, float v1, pz_vec4 color)
{
    if (mgr->vertex_count + 6 > mgr->vertex_capacity)
        return;

    pz_font_vertex *v = &mgr->vertices[mgr->vertex_count];

    // Triangle 1
    v[0] = (pz_font_vertex) { x0, y0, u0, v0, color.x, color.y, color.z,
        color.w };
    v[1] = (pz_font_vertex) { x1, y0, u1, v0, color.x, color.y, color.z,
        color.w };
    v[2] = (pz_font_vertex) { x1, y1, u1, v1, color.x, color.y, color.z,
        color.w };

    // Triangle 2
    v[3] = (pz_font_vertex) { x0, y0, u0, v0, color.x, color.y, color.z,
        color.w };
    v[4] = (pz_font_vertex) { x1, y1, u1, v1, color.x, color.y, color.z,
        color.w };
    v[5] = (pz_font_vertex) { x0, y1, u0, v1, color.x, color.y, color.z,
        color.w };

    mgr->vertex_count += 6;
}

void
pz_font_draw(pz_font_manager *mgr, const pz_text_style *style, float x, float y,
    const char *text)
{
    if (!style || !style->font || !text)
        return;

    pz_font *font = style->font;
    float scale = style->size / SDF_SIZE;

    // Apply alignment
    pz_text_bounds bounds = pz_font_measure(style, text);

    switch (style->align_h) {
    case PZ_FONT_ALIGN_CENTER:
        x -= bounds.width * 0.5f;
        break;
    case PZ_FONT_ALIGN_RIGHT:
        x -= bounds.width;
        break;
    default:
        break;
    }

    switch (style->align_v) {
    case PZ_FONT_ALIGN_TOP:
        y += pz_font_baseline(font, style->size);
        break;
    case PZ_FONT_ALIGN_MIDDLE:
        y += pz_font_baseline(font, style->size) - bounds.height * 0.5f;
        break;
    case PZ_FONT_ALIGN_BOTTOM:
        y -= (bounds.height - pz_font_baseline(font, style->size));
        break;
    default: // BASELINE
        break;
    }

    float cursor_x = x;
    const char *p = text;

    while (*p) {
        uint32_t cp = decode_utf8(&p);

        if (cp == '\n') {
            cursor_x = x;
            y += pz_font_line_height(font, style->size);
            continue;
        }

        pz_glyph *g = font_get_glyph(mgr, font, (int)cp);
        if (!g || g->atlas_w == 0) {
            // Empty glyph (like space)
            if (g)
                cursor_x += g->x_advance * scale;
            continue;
        }

        // Calculate quad position
        float gx = cursor_x + g->x_offset * scale;
        float gy = y + g->y_offset * scale;
        float gw = g->width * scale;
        float gh = g->height * scale;

        // Calculate UV coordinates
        float inv_atlas = 1.0f / PZ_FONT_ATLAS_SIZE;
        float u0 = g->atlas_x * inv_atlas;
        float v0 = g->atlas_y * inv_atlas;
        float u1 = (g->atlas_x + g->atlas_w) * inv_atlas;
        float v1 = (g->atlas_y + g->atlas_h) * inv_atlas;

        push_quad(mgr, gx, gy, gx + gw, gy + gh, u0, v0, u1, v1, style->color);

        cursor_x += g->x_advance * scale;
    }
}

void
pz_font_drawf(pz_font_manager *mgr, const pz_text_style *style, float x,
    float y, const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    pz_font_draw(mgr, style, x, y, buf);
}

void
pz_font_end_frame(pz_font_manager *mgr)
{
    if (mgr->vertex_count == 0)
        return;

    // Update atlas if dirty
    if (mgr->atlas_dirty) {
        pz_renderer_update_texture(mgr->renderer, mgr->atlas_texture, 0, 0,
            PZ_FONT_ATLAS_SIZE, PZ_FONT_ATLAS_SIZE, mgr->atlas_data);
        mgr->atlas_dirty = false;
    }

    // Update vertex buffer
    pz_renderer_update_buffer(mgr->renderer, mgr->vertex_buffer, 0,
        mgr->vertices, mgr->vertex_count * sizeof(pz_font_vertex));

    // Set up uniforms
    pz_renderer_set_uniform_vec2(mgr->renderer, mgr->shader, "u_screen_size",
        pz_vec2_new((float)mgr->screen_width, (float)mgr->screen_height));

    // Bind texture
    pz_renderer_bind_texture(mgr->renderer, 0, mgr->atlas_texture);

    // Draw
    pz_draw_cmd cmd = {
        .pipeline = mgr->pipeline,
        .vertex_buffer = mgr->vertex_buffer,
        .vertex_count = mgr->vertex_count,
    };
    pz_renderer_draw(mgr->renderer, &cmd);
}
