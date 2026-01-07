/*
 * Tank Game - Editor UI System Implementation
 */

#include "pz_editor_ui.h"
#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include <string.h>

/* ============================================================================
 * Internal Types
 * ============================================================================
 */

// Quad vertex for UI rendering
typedef struct ui_vertex {
    float x, y; // Position
    float u, v; // UV (for textured quads, 0 for solid)
    float r, g, b, a;
} ui_vertex;

// Panel state
typedef struct ui_panel {
    float x, y, w, h;
    bool *open_ptr;
    bool dragging;
    float drag_offset_x, drag_offset_y;
} ui_panel;

// Textured slot draw command (for deferred rendering)
#define PZ_UI_MAX_TEXTURED_SLOTS 32
#define PZ_UI_MAX_FLUSHES 8
#define PZ_UI_MAX_CLIPS 8
typedef struct ui_textured_slot {
    float x, y, size;
    pz_texture_handle wall_texture;
    pz_texture_handle ground_texture;
} ui_textured_slot;

typedef struct ui_clip_rect {
    float x, y, w, h;
} ui_clip_rect;

struct pz_editor_ui {
    pz_renderer *renderer;
    pz_font_manager *font_mgr;
    pz_font *font;

    // Screen dimensions
    int screen_width;
    int screen_height;

    // Mouse state
    pz_ui_mouse mouse;
    bool mouse_consumed;
    bool keyboard_consumed;
    bool input_enabled;

    // Hot/Active widget tracking
    uintptr_t hot_id; // Widget under mouse
    uintptr_t active_id; // Widget being interacted with

    // Rendering
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    pz_buffer_handle vertex_buffers[PZ_UI_MAX_FLUSHES];
    ui_vertex *vertices;
    int vertex_count;
    int max_vertices;
    int flush_index;

    // Textured rendering (separate shader/pipeline for texture support)
    pz_shader_handle textured_shader;
    pz_pipeline_handle textured_pipeline;
    pz_buffer_handle textured_vertex_buffers[PZ_UI_MAX_FLUSHES];

    // Panel stack
    ui_panel panels[PZ_UI_MAX_PANELS];
    int panel_count;

    // Clip stack
    ui_clip_rect clips[PZ_UI_MAX_CLIPS];
    int clip_count;

    // Textured slots (rendered after main UI pass)
    ui_textured_slot textured_slots[PZ_UI_MAX_TEXTURED_SLOTS];
    int textured_slot_count;

    // Colors
    pz_ui_colors colors;

    float dpi_scale;
};

/* ============================================================================
 * Default Colors
 * ============================================================================
 */

static const pz_ui_colors DEFAULT_COLORS = {
    .panel_bg = { 0.15f, 0.15f, 0.18f, 0.95f },
    .panel_title_bg = { 0.2f, 0.2f, 0.25f, 1.0f },
    .panel_border = { 0.3f, 0.3f, 0.35f, 1.0f },
    .button_bg = { 0.25f, 0.25f, 0.3f, 1.0f },
    .button_hover = { 0.35f, 0.35f, 0.4f, 1.0f },
    .button_active = { 0.2f, 0.4f, 0.6f, 1.0f },
    .button_border = { 0.4f, 0.4f, 0.45f, 1.0f },
    .text = { 1.0f, 1.0f, 1.0f, 1.0f },
    .text_dim = { 0.6f, 0.6f, 0.6f, 1.0f },
    .slot_empty = { 0.2f, 0.2f, 0.2f, 0.5f },
    .slot_filled = { 0.3f, 0.3f, 0.35f, 1.0f },
    .slot_selected = { 0.3f, 0.5f, 0.7f, 1.0f },
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================
 */

static bool
point_in_rect(
    pz_editor_ui *ui, float px, float py, float x, float y, float w, float h)
{
    if (!(px >= x && px < x + w && py >= y && py < y + h)) {
        return false;
    }
    if (!ui || ui->clip_count <= 0) {
        return true;
    }

    ui_clip_rect *clip = &ui->clips[ui->clip_count - 1];
    return px >= clip->x && px < clip->x + clip->w && py >= clip->y
        && py < clip->y + clip->h;
}

static const char *
ui_truncate_text(pz_editor_ui *ui, const pz_text_style *style, const char *text,
    float max_width, char *buffer, size_t buffer_len)
{
    if (!ui || !style || !ui->font || !text || max_width <= 0.0f
        || buffer_len < 4) {
        return text;
    }

    pz_text_bounds bounds = pz_font_measure(style, text);
    if (bounds.width <= max_width) {
        return text;
    }

    const char *ellipsis = "...";
    pz_text_bounds ellipsis_bounds = pz_font_measure(style, ellipsis);
    float available = max_width - ellipsis_bounds.width;
    if (available <= 0.0f) {
        if (ellipsis_bounds.width <= max_width) {
            strncpy(buffer, ellipsis, buffer_len);
            buffer[buffer_len - 1] = '\0';
            return buffer;
        }
        buffer[0] = '\0';
        return buffer;
    }

    size_t text_len = strlen(text);
    size_t lo = 0;
    size_t hi = text_len;
    while (lo < hi) {
        size_t mid = (lo + hi + 1) / 2;
        if (mid >= buffer_len) {
            mid = buffer_len - 1;
        }
        memcpy(buffer, text, mid);
        buffer[mid] = '\0';
        pz_text_bounds test_bounds = pz_font_measure(style, buffer);
        if (test_bounds.width <= available) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    if (lo >= buffer_len - 4) {
        lo = buffer_len - 4;
    }
    memcpy(buffer, text, lo);
    buffer[lo] = '\0';
    strncat(buffer, ellipsis, buffer_len - strlen(buffer) - 1);
    return buffer;
}

static ui_clip_rect
ui_clip_intersect(ui_clip_rect a, ui_clip_rect b)
{
    float x0 = pz_maxf(a.x, b.x);
    float y0 = pz_maxf(a.y, b.y);
    float x1 = pz_minf(a.x + a.w, b.x + b.w);
    float y1 = pz_minf(a.y + a.h, b.y + b.h);

    ui_clip_rect out = { x0, y0, x1 - x0, y1 - y0 };
    if (out.w < 0.0f) {
        out.w = 0.0f;
    }
    if (out.h < 0.0f) {
        out.h = 0.0f;
    }
    return out;
}

static void
ui_apply_scissor(pz_editor_ui *ui, ui_clip_rect rect)
{
    if (!ui || !ui->renderer) {
        return;
    }

    float scale = ui->dpi_scale > 0.0f ? ui->dpi_scale : 1.0f;
    int sx = (int)floorf(rect.x * scale);
    int sy = (int)floorf(rect.y * scale);
    int sw = (int)ceilf(rect.w * scale);
    int sh = (int)ceilf(rect.h * scale);

    int max_w = (int)ceilf((float)ui->screen_width * scale);
    int max_h = (int)ceilf((float)ui->screen_height * scale);

    if (sx < 0) {
        sw += sx;
        sx = 0;
    }
    if (sy < 0) {
        sh += sy;
        sy = 0;
    }
    if (sx + sw > max_w) {
        sw = max_w - sx;
    }
    if (sy + sh > max_h) {
        sh = max_h - sy;
    }
    if (sw < 0) {
        sw = 0;
    }
    if (sh < 0) {
        sh = 0;
    }

    pz_renderer_set_scissor(ui->renderer, sx, sy, sw, sh);
}

static void
push_quad(pz_editor_ui *ui, float x, float y, float w, float h, pz_vec4 color)
{
    if (ui->vertex_count + 6 > ui->max_vertices) {
        return;
    }

    ui_vertex *v = &ui->vertices[ui->vertex_count];

    // Two triangles for quad
    // Triangle 1
    v[0] = (ui_vertex) { x, y, 0, 0, color.x, color.y, color.z, color.w };
    v[1] = (ui_vertex) { x + w, y, 0, 0, color.x, color.y, color.z, color.w };
    v[2] = (ui_vertex) { x + w, y + h, 0, 0, color.x, color.y, color.z,
        color.w };

    // Triangle 2
    v[3] = (ui_vertex) { x, y, 0, 0, color.x, color.y, color.z, color.w };
    v[4] = (ui_vertex) { x + w, y + h, 0, 0, color.x, color.y, color.z,
        color.w };
    v[5] = (ui_vertex) { x, y + h, 0, 0, color.x, color.y, color.z, color.w };

    ui->vertex_count += 6;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

pz_editor_ui *
pz_editor_ui_create(pz_renderer *renderer, pz_font_manager *font_mgr)
{
    pz_editor_ui *ui = pz_alloc(sizeof(pz_editor_ui));
    if (!ui) {
        return NULL;
    }

    memset(ui, 0, sizeof(pz_editor_ui));
    ui->renderer = renderer;
    ui->font_mgr = font_mgr;
    ui->colors = DEFAULT_COLORS;

    // Get default font
    ui->font = pz_font_get(font_mgr, "RussoOne-Regular");
    if (!ui->font) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
            "Editor UI: Could not find RussoOne font, trying fallback");
        ui->font = pz_font_get(font_mgr, "CaveatBrush-Regular");
    }

    // Create shader (reuse ui_quad shader if it exists, otherwise create
    // simple one)
    ui->shader = pz_renderer_load_shader(
        renderer, "shaders/ui_quad.vert", "shaders/ui_quad.frag", "ui_quad");

    if (ui->shader != PZ_INVALID_HANDLE) {
        pz_vertex_attr attrs[] = {
            { .name = "a_position", .type = PZ_ATTR_FLOAT2, .offset = 0 },
            { .name = "a_texcoord",
                .type = PZ_ATTR_FLOAT2,
                .offset = 2 * sizeof(float) },
            { .name = "a_color",
                .type = PZ_ATTR_FLOAT4,
                .offset = 4 * sizeof(float) },
        };

        pz_pipeline_desc desc = {
            .shader = ui->shader,
            .vertex_layout
            = { .attrs = attrs, .attr_count = 3, .stride = sizeof(ui_vertex) },
            .blend = PZ_BLEND_ALPHA,
            .depth = PZ_DEPTH_NONE,
            .cull = PZ_CULL_NONE,
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        ui->pipeline = pz_renderer_create_pipeline(renderer, &desc);
    }

    // Allocate vertex buffer
    ui->max_vertices = PZ_UI_MAX_QUADS * 6;
    ui->vertices = pz_alloc(ui->max_vertices * sizeof(ui_vertex));

    pz_buffer_desc buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_DYNAMIC,
        .size = ui->max_vertices * sizeof(ui_vertex),
    };
    for (int i = 0; i < PZ_UI_MAX_FLUSHES; i++) {
        ui->vertex_buffers[i] = pz_renderer_create_buffer(renderer, &buf_desc);
    }

    // Create textured shader and pipeline (for tile previews)
    ui->textured_shader = pz_renderer_load_shader(renderer,
        "shaders/ui_textured.vert", "shaders/ui_textured.frag", "ui_textured");
    if (ui->textured_shader != PZ_INVALID_HANDLE) {
        pz_vertex_attr tex_attrs[] = {
            { .name = "a_position", .type = PZ_ATTR_FLOAT2, .offset = 0 },
            { .name = "a_texcoord",
                .type = PZ_ATTR_FLOAT2,
                .offset = 2 * sizeof(float) },
            { .name = "a_color",
                .type = PZ_ATTR_FLOAT4,
                .offset = 4 * sizeof(float) },
        };

        pz_pipeline_desc tex_desc = {
            .shader = ui->textured_shader,
            .vertex_layout = { .attrs = tex_attrs,
                .attr_count = 3,
                .stride = sizeof(ui_vertex) },
            .blend = PZ_BLEND_ALPHA,
            .depth = PZ_DEPTH_NONE,
            .cull = PZ_CULL_NONE,
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        ui->textured_pipeline
            = pz_renderer_create_pipeline(renderer, &tex_desc);

        // Create separate vertex buffer for textured slot triangles
        pz_buffer_desc tex_buf_desc = {
            .type = PZ_BUFFER_VERTEX,
            .usage = PZ_BUFFER_DYNAMIC,
            .size = PZ_UI_MAX_TEXTURED_SLOTS * 6 * sizeof(ui_vertex),
        };
        for (int i = 0; i < PZ_UI_MAX_FLUSHES; i++) {
            ui->textured_vertex_buffers[i]
                = pz_renderer_create_buffer(renderer, &tex_buf_desc);
        }
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Editor UI created");

    return ui;
}

void
pz_editor_ui_destroy(pz_editor_ui *ui)
{
    if (!ui) {
        return;
    }

    if (ui->vertices) {
        pz_free(ui->vertices);
    }

    for (int i = 0; i < PZ_UI_MAX_FLUSHES; i++) {
        if (ui->vertex_buffers[i] != PZ_INVALID_HANDLE) {
            pz_renderer_destroy_buffer(ui->renderer, ui->vertex_buffers[i]);
        }
        if (ui->textured_vertex_buffers[i] != PZ_INVALID_HANDLE) {
            pz_renderer_destroy_buffer(
                ui->renderer, ui->textured_vertex_buffers[i]);
        }
    }

    pz_free(ui);
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Editor UI destroyed");
}

/* ============================================================================
 * Frame Management
 * ============================================================================
 */

void
pz_editor_ui_begin(
    pz_editor_ui *ui, int screen_width, int screen_height, pz_ui_mouse mouse)
{
    if (!ui) {
        return;
    }

    ui->screen_width = screen_width;
    ui->screen_height = screen_height;
    ui->mouse = mouse;
    ui->mouse_consumed = false;
    ui->keyboard_consumed = false;
    ui->input_enabled = true;
    ui->vertex_count = 0;
    ui->panel_count = 0;
    ui->textured_slot_count = 0;
    ui->hot_id = 0;
    ui->flush_index = 0;
    ui->clip_count = 0;
    ui->dpi_scale = pz_renderer_get_dpi_scale(ui->renderer);
    pz_renderer_clear_scissor(ui->renderer);

    // Begin font frame for text rendering
    pz_font_begin_frame(ui->font_mgr);
}

static void
ui_flush_batches(pz_editor_ui *ui, bool restart_font)
{
    if (!ui) {
        return;
    }

    if (ui->flush_index >= PZ_UI_MAX_FLUSHES) {
        ui->vertex_count = 0;
        ui->textured_slot_count = 0;
        if (restart_font) {
            pz_font_flush(ui->font_mgr);
        } else {
            pz_font_end_frame(ui->font_mgr);
        }
        return;
    }

    pz_buffer_handle vertex_buffer = ui->vertex_buffers[ui->flush_index];
    pz_buffer_handle textured_vertex_buffer
        = ui->textured_vertex_buffers[ui->flush_index];

    // Set up orthographic projection for screen space
    pz_mat4 ortho = pz_mat4_ortho(
        0, (float)ui->screen_width, (float)ui->screen_height, 0, -1, 1);

    // Flush UI quads (solid color pass)
    if (ui->vertex_count > 0 && ui->pipeline != PZ_INVALID_HANDLE
        && vertex_buffer != PZ_INVALID_HANDLE) {
        // Update vertex buffer
        pz_renderer_update_buffer(ui->renderer, vertex_buffer, 0, ui->vertices,
            ui->vertex_count * sizeof(ui_vertex));

        pz_renderer_set_uniform_mat4(
            ui->renderer, ui->shader, "u_projection", &ortho);

        // Draw solid color quads
        pz_draw_cmd cmd = {
            .pipeline = ui->pipeline,
            .vertex_buffer = vertex_buffer,
            .vertex_count = ui->vertex_count,
        };
        pz_renderer_draw(ui->renderer, &cmd);
    }

    // Render textured slots (batched into one buffer update)
    if (ui->textured_slot_count > 0
        && ui->textured_pipeline != PZ_INVALID_HANDLE
        && textured_vertex_buffer != PZ_INVALID_HANDLE) {
        ui_vertex textured_vertices[PZ_UI_MAX_TEXTURED_SLOTS * 6];
        size_t vertex_count = 0;

        typedef struct ui_textured_draw {
            pz_texture_handle texture;
            size_t vertex_offset;
        } ui_textured_draw;

        ui_textured_draw draws[PZ_UI_MAX_TEXTURED_SLOTS * 2];
        size_t draw_count = 0;

        for (int i = 0; i < ui->textured_slot_count; i++) {
            ui_textured_slot *slot = &ui->textured_slots[i];
            float x = slot->x;
            float y = slot->y;
            float size = slot->size;
            float inset = 4.0f;
            float x0 = x + inset;
            float y0 = y + inset;
            float x1 = x + size - inset;
            float y1 = y + size - inset;

            // Triangle 1 (wall texture) - top-left triangle
            draws[draw_count++] = (ui_textured_draw) {
                .texture = slot->wall_texture,
                .vertex_offset = vertex_count,
            };
            textured_vertices[vertex_count++]
                = (ui_vertex) { x0, y0, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
            textured_vertices[vertex_count++]
                = (ui_vertex) { x1, y0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
            textured_vertices[vertex_count++]
                = (ui_vertex) { x0, y1, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

            // Triangle 2 (ground texture) - bottom-right triangle
            draws[draw_count++] = (ui_textured_draw) {
                .texture = slot->ground_texture,
                .vertex_offset = vertex_count,
            };
            textured_vertices[vertex_count++]
                = (ui_vertex) { x1, y0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
            textured_vertices[vertex_count++]
                = (ui_vertex) { x1, y1, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
            textured_vertices[vertex_count++]
                = (ui_vertex) { x0, y1, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        }

        if (vertex_count > 0) {
            pz_renderer_update_buffer(ui->renderer, textured_vertex_buffer, 0,
                textured_vertices, vertex_count * sizeof(ui_vertex));

            pz_renderer_set_uniform_mat4(
                ui->renderer, ui->textured_shader, "u_projection", &ortho);

            for (size_t i = 0; i < draw_count; i++) {
                pz_renderer_bind_texture(ui->renderer, 0, draws[i].texture);

                pz_draw_cmd cmd = {
                    .pipeline = ui->textured_pipeline,
                    .vertex_buffer = textured_vertex_buffer,
                    .vertex_count = 3,
                    .vertex_offset = draws[i].vertex_offset,
                };
                pz_renderer_draw(ui->renderer, &cmd);
            }
        }
    }

    // Render text on top (font system handles its own batching)
    if (restart_font) {
        pz_font_flush(ui->font_mgr);
    } else {
        pz_font_end_frame(ui->font_mgr);
    }

    ui->vertex_count = 0;
    ui->textured_slot_count = 0;
    ui->flush_index++;
}

void
pz_editor_ui_end(pz_editor_ui *ui)
{
    if (!ui) {
        return;
    }

    ui_flush_batches(ui, false);
    ui->clip_count = 0;
    pz_renderer_clear_scissor(ui->renderer);

    // Clear active widget if mouse released
    if (ui->mouse.left_released) {
        ui->active_id = 0;
    }
}

bool
pz_editor_ui_wants_mouse(pz_editor_ui *ui)
{
    return ui ? ui->mouse_consumed : false;
}

bool
pz_editor_ui_wants_keyboard(pz_editor_ui *ui)
{
    return ui ? ui->keyboard_consumed : false;
}

void
pz_editor_ui_set_input_enabled(pz_editor_ui *ui, bool enabled)
{
    if (!ui) {
        return;
    }
    ui->input_enabled = enabled;
}

void
pz_ui_consume_mouse(pz_editor_ui *ui)
{
    if (!ui) {
        return;
    }
    ui->mouse_consumed = true;
}

/* ============================================================================
 * Basic Drawing
 * ============================================================================
 */

void
pz_ui_rect(pz_editor_ui *ui, float x, float y, float w, float h, pz_vec4 color)
{
    if (!ui) {
        return;
    }
    push_quad(ui, x, y, w, h, color);
}

void
pz_ui_rect_outline(pz_editor_ui *ui, float x, float y, float w, float h,
    pz_vec4 color, float thickness)
{
    if (!ui) {
        return;
    }

    // Top
    push_quad(ui, x, y, w, thickness, color);
    // Bottom
    push_quad(ui, x, y + h - thickness, w, thickness, color);
    // Left
    push_quad(ui, x, y + thickness, thickness, h - 2 * thickness, color);
    // Right
    push_quad(ui, x + w - thickness, y + thickness, thickness,
        h - 2 * thickness, color);
}

void
pz_ui_label(pz_editor_ui *ui, float x, float y, const char *text, pz_vec4 color)
{
    if (!ui || !ui->font || !text) {
        return;
    }

    pz_text_style style = PZ_TEXT_STYLE_DEFAULT(ui->font, 16.0f);
    style.color = color;
    style.align_v = PZ_FONT_ALIGN_TOP;

    // Don't call begin_frame here - it's managed by the UI system
    pz_font_draw(ui->font_mgr, &style, x, y, text);
}

void
pz_ui_label_fit(pz_editor_ui *ui, float x, float y, float max_w,
    const char *text, pz_vec4 color)
{
    if (!ui || !ui->font || !text) {
        return;
    }

    pz_text_style style = PZ_TEXT_STYLE_DEFAULT(ui->font, 16.0f);
    style.color = color;
    style.align_v = PZ_FONT_ALIGN_TOP;

    char clipped[128];
    const char *draw_text
        = ui_truncate_text(ui, &style, text, max_w, clipped, sizeof(clipped));

    // Don't call begin_frame here - it's managed by the UI system
    pz_font_draw(ui->font_mgr, &style, x, y, draw_text);
}

void
pz_ui_label_centered(pz_editor_ui *ui, float x, float y, float w, float h,
    const char *text, pz_vec4 color)
{
    if (!ui || !ui->font || !text) {
        return;
    }

    pz_text_style style = PZ_TEXT_STYLE_DEFAULT(ui->font, 16.0f);
    style.color = color;
    style.align_h = PZ_FONT_ALIGN_CENTER;
    style.align_v = PZ_FONT_ALIGN_MIDDLE;

    // Don't call begin_frame here - it's managed by the UI system
    pz_font_draw(ui->font_mgr, &style, x + w / 2, y + h / 2, text);
}

/* ============================================================================
 * Widgets
 * ============================================================================
 */

bool
pz_ui_button(
    pz_editor_ui *ui, float x, float y, float w, float h, const char *label)
{
    if (!ui) {
        return false;
    }

    uintptr_t id = (uintptr_t)label; // Use label pointer as ID
    bool hovered = ui->input_enabled
        && point_in_rect(ui, ui->mouse.x, ui->mouse.y, x, y, w, h);
    bool clicked = false;

    // Update hot/active state
    if (hovered) {
        ui->hot_id = id;
        ui->mouse_consumed = true;

        if (ui->mouse.left_clicked) {
            ui->active_id = id;
        }
    }

    // Check for click
    if (ui->active_id == id && ui->mouse.left_released && hovered) {
        clicked = true;
    }

    // Choose color based on state
    pz_vec4 bg_color = ui->colors.button_bg;
    if (ui->active_id == id) {
        bg_color = ui->colors.button_active;
    } else if (hovered) {
        bg_color = ui->colors.button_hover;
    }

    // Draw button
    push_quad(ui, x, y, w, h, bg_color);
    pz_ui_rect_outline(ui, x, y, w, h, ui->colors.button_border, 1.0f);

    // Draw label
    if (label) {
        pz_ui_label_centered(ui, x, y, w, h, label, ui->colors.text);
    }

    return clicked;
}

int
pz_ui_slot(pz_editor_ui *ui, float x, float y, float size, bool selected,
    bool filled, const char *label, const char *content_label,
    pz_vec4 preview_color)
{
    if (!ui) {
        return PZ_UI_NONE;
    }

    uintptr_t id = (uintptr_t)&x + (uintptr_t)(y * 1000); // Generate unique ID
    bool hovered = ui->input_enabled
        && point_in_rect(ui, ui->mouse.x, ui->mouse.y, x, y, size, size);
    int result = PZ_UI_NONE;

    if (hovered) {
        ui->hot_id = id;
        ui->mouse_consumed = true;
        result |= PZ_UI_HOVERED;

        if (ui->mouse.left_clicked) {
            ui->active_id = id;
            result |= PZ_UI_CLICKED;
        }
    }

    // Background
    pz_vec4 bg = filled ? ui->colors.slot_filled : ui->colors.slot_empty;
    if (selected) {
        bg = ui->colors.slot_selected;
    }
    push_quad(ui, x, y, size, size, bg);

    // Preview color (inner rect)
    if (filled) {
        float inset = 4.0f;
        push_quad(ui, x + inset, y + inset, size - 2 * inset, size - 2 * inset,
            preview_color);
    }

    // Border
    pz_vec4 border
        = selected ? (pz_vec4) { 1, 1, 1, 1 } : ui->colors.button_border;
    float border_width = selected ? 2.0f : 1.0f;
    pz_ui_rect_outline(ui, x, y, size, size, border, border_width);

    // Label (number key) - top-left corner
    if (label) {
        pz_text_style style = PZ_TEXT_STYLE_DEFAULT(ui->font, 12.0f);
        style.color = ui->colors.text;
        style.align_h = PZ_FONT_ALIGN_LEFT;
        style.align_v = PZ_FONT_ALIGN_TOP;

        pz_font_draw(ui->font_mgr, &style, x + 3, y + 2, label);
    }

    // Content label (tile/tag name) - centered
    if (content_label && filled) {
        pz_text_style style = PZ_TEXT_STYLE_DEFAULT(ui->font, 10.0f);
        style.color = (pz_vec4) { 1.0f, 1.0f, 1.0f, 0.9f };
        style.align_h = PZ_FONT_ALIGN_CENTER;
        style.align_v = PZ_FONT_ALIGN_MIDDLE;

        char clipped[64];
        const char *draw_label = ui_truncate_text(
            ui, &style, content_label, size - 8.0f, clipped, sizeof(clipped));

        pz_font_draw(
            ui->font_mgr, &style, x + size / 2, y + size / 2, draw_label);
    }

    return result;
}

int
pz_ui_slot_textured(pz_editor_ui *ui, float x, float y, float size,
    bool selected, const char *label, pz_texture_handle wall_texture,
    pz_texture_handle ground_texture)
{
    if (!ui) {
        return PZ_UI_NONE;
    }

    uintptr_t id = (uintptr_t)&x + (uintptr_t)(y * 1000);
    bool hovered = ui->input_enabled
        && point_in_rect(ui, ui->mouse.x, ui->mouse.y, x, y, size, size);
    int result = PZ_UI_NONE;

    if (hovered) {
        ui->hot_id = id;
        ui->mouse_consumed = true;
        result |= PZ_UI_HOVERED;

        if (ui->mouse.left_clicked) {
            ui->active_id = id;
            result |= PZ_UI_CLICKED;
        }
    }

    // Background
    pz_vec4 bg = selected ? ui->colors.slot_selected : ui->colors.slot_filled;
    push_quad(ui, x, y, size, size, bg);

    // Queue textured preview (rendered after solid color pass)
    if (ui->textured_slot_count < PZ_UI_MAX_TEXTURED_SLOTS
        && wall_texture != PZ_INVALID_HANDLE
        && ground_texture != PZ_INVALID_HANDLE) {
        ui_textured_slot *slot = &ui->textured_slots[ui->textured_slot_count++];
        slot->x = x;
        slot->y = y;
        slot->size = size;
        slot->wall_texture = wall_texture;
        slot->ground_texture = ground_texture;
    }

    // Border
    pz_vec4 border
        = selected ? (pz_vec4) { 1, 1, 1, 1 } : ui->colors.button_border;
    float border_width = selected ? 2.0f : 1.0f;
    pz_ui_rect_outline(ui, x, y, size, size, border, border_width);

    // Label (number key) - top-left corner
    if (label) {
        pz_text_style style = PZ_TEXT_STYLE_DEFAULT(ui->font, 12.0f);
        style.color = ui->colors.text;
        style.align_h = PZ_FONT_ALIGN_LEFT;
        style.align_v = PZ_FONT_ALIGN_TOP;

        pz_font_draw(ui->font_mgr, &style, x + 3, y + 2, label);
    }

    return result;
}

/* ============================================================================
 * Clipping
 * ============================================================================
 */

bool
pz_ui_clip_begin(pz_editor_ui *ui, float x, float y, float w, float h)
{
    if (!ui) {
        return false;
    }

    if (ui->clip_count >= PZ_UI_MAX_CLIPS) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
            "Editor UI: Clip stack limit reached");
        return false;
    }

    ui_flush_batches(ui, true);

    ui_clip_rect rect = { x, y, w, h };
    if (rect.w < 0.0f) {
        rect.w = 0.0f;
    }
    if (rect.h < 0.0f) {
        rect.h = 0.0f;
    }

    if (ui->clip_count > 0) {
        rect = ui_clip_intersect(rect, ui->clips[ui->clip_count - 1]);
    }

    ui->clips[ui->clip_count++] = rect;
    ui_apply_scissor(ui, rect);

    return rect.w > 0.0f && rect.h > 0.0f;
}

void
pz_ui_clip_end(pz_editor_ui *ui)
{
    if (!ui || ui->clip_count <= 0) {
        return;
    }

    ui_flush_batches(ui, true);
    ui->clip_count--;

    if (ui->clip_count > 0) {
        ui_apply_scissor(ui, ui->clips[ui->clip_count - 1]);
    } else {
        pz_renderer_clear_scissor(ui->renderer);
    }
}

/* ============================================================================
 * Panels
 * ============================================================================
 */

bool
pz_ui_panel_begin(pz_editor_ui *ui, const char *title, float x, float y,
    float w, float h, bool *open)
{
    if (!ui || ui->panel_count >= PZ_UI_MAX_PANELS) {
        return false;
    }

    if (open && !*open) {
        return false;
    }

    ui_panel *panel = &ui->panels[ui->panel_count++];
    panel->x = x;
    panel->y = y;
    panel->w = w;
    panel->h = h;
    panel->open_ptr = open;

    // Check if panel consumes mouse
    if (ui->input_enabled
        && point_in_rect(ui, ui->mouse.x, ui->mouse.y, x, y, w, h)) {
        ui->mouse_consumed = true;
    }

    // Draw panel background
    push_quad(ui, x, y, w, h, ui->colors.panel_bg);
    pz_ui_rect_outline(ui, x, y, w, h, ui->colors.panel_border, 1.0f);

    // Draw title bar
    push_quad(ui, x, y, w, PZ_UI_PANEL_TITLE_HEIGHT, ui->colors.panel_title_bg);

    // Title text
    if (title) {
        pz_text_style style = PZ_TEXT_STYLE_DEFAULT(ui->font, 14.0f);
        style.color = ui->colors.text;
        style.align_v = PZ_FONT_ALIGN_MIDDLE;

        pz_font_draw(ui->font_mgr, &style, x + 8,
            y + PZ_UI_PANEL_TITLE_HEIGHT / 2, title);
    }

    // Close button
    if (open) {
        float btn_size = PZ_UI_PANEL_TITLE_HEIGHT - 4;
        float btn_x = x + w - btn_size - 2;
        float btn_y = y + 2;

        bool btn_hovered = ui->input_enabled
            && point_in_rect(
                ui, ui->mouse.x, ui->mouse.y, btn_x, btn_y, btn_size, btn_size);

        pz_vec4 btn_color
            = btn_hovered ? ui->colors.button_hover : ui->colors.button_bg;
        push_quad(ui, btn_x, btn_y, btn_size, btn_size, btn_color);

        // X symbol
        pz_ui_label_centered(
            ui, btn_x, btn_y, btn_size, btn_size, "X", ui->colors.text);

        if (btn_hovered && ui->mouse.left_clicked) {
            *open = false;
        }
    }

    return true;
}

void
pz_ui_panel_end(pz_editor_ui *ui)
{
    if (!ui || ui->panel_count <= 0) {
        return;
    }
    ui->panel_count--;
}

/* ============================================================================
 * Windows (Draggable Dialogs)
 * ============================================================================
 */

pz_ui_window_result
pz_ui_window(pz_editor_ui *ui, const char *title, float w, float h, bool *open,
    pz_ui_window_state *state, bool allow_input)
{
    pz_ui_window_result result = { 0 };

    if (!ui || !open || !*open || !state) {
        return result;
    }

    ui_flush_batches(ui, true);

    float title_bar_h = 28.0f;

    // Auto-center if position is (0,0)
    if (state->x == 0 && state->y == 0) {
        state->x = (ui->screen_width - w) / 2;
        state->y = (ui->screen_height - h) / 2;
    }

    // Clamp to screen bounds
    if (state->x < 0)
        state->x = 0;
    if (state->y < 0)
        state->y = 0;
    if (state->x + w > ui->screen_width)
        state->x = ui->screen_width - w;
    if (state->y + h > ui->screen_height)
        state->y = ui->screen_height - h;

    float win_x = state->x;
    float win_y = state->y;

    // Check mouse position
    bool in_title_bar = point_in_rect(
        ui, ui->mouse.x, ui->mouse.y, win_x, win_y, w, title_bar_h);
    bool in_window
        = point_in_rect(ui, ui->mouse.x, ui->mouse.y, win_x, win_y, w, h);

    bool handle_input = allow_input || state->dragging;

    // Consume mouse if over window or dragging (only for active window)
    if (handle_input && (in_window || state->dragging)) {
        ui->mouse_consumed = true;
    }

    // Handle dragging
    if (handle_input && state->dragging) {
        state->x = ui->mouse.x - state->drag_offset_x;
        state->y = ui->mouse.y - state->drag_offset_y;

        if (!ui->mouse.left_down) {
            state->dragging = false;
        }
    } else if (handle_input && in_title_bar && ui->mouse.left_clicked) {
        state->dragging = true;
        state->drag_offset_x = ui->mouse.x - win_x;
        state->drag_offset_y = ui->mouse.y - win_y;
    }

    // Draw window background
    push_quad(ui, win_x, win_y, w, h, ui->colors.panel_bg);
    pz_ui_rect_outline(ui, win_x, win_y, w, h, ui->colors.panel_border, 1.0f);

    // Draw title bar
    push_quad(ui, win_x, win_y, w, title_bar_h, ui->colors.panel_title_bg);

    // Title text
    if (title) {
        pz_ui_label(ui, win_x + 10, win_y + 6, title, ui->colors.text);
    }

    // Close button
    if (allow_input
        && pz_ui_button(ui, win_x + w - 60, win_y + 4, 50, 20, "Close")) {
        *open = false;
        state->dragging = false;
        state->z_order = 0;
        return result; // Return early, window closing
    }

    // Fill in result
    result.visible = true;
    result.content_x = win_x + 10;
    result.content_y = win_y + title_bar_h + 8;
    result.content_w = w - 20;
    result.content_h = h - title_bar_h - 16;

    return result;
}

void
pz_ui_panel_content_area(
    pz_editor_ui *ui, float *x, float *y, float *w, float *h)
{
    if (!ui || ui->panel_count <= 0) {
        if (x)
            *x = 0;
        if (y)
            *y = 0;
        if (w)
            *w = 0;
        if (h)
            *h = 0;
        return;
    }

    ui_panel *panel = &ui->panels[ui->panel_count - 1];
    if (x)
        *x = panel->x + PZ_UI_PANEL_PADDING;
    if (y)
        *y = panel->y + PZ_UI_PANEL_TITLE_HEIGHT + PZ_UI_PANEL_PADDING;
    if (w)
        *w = panel->w - 2 * PZ_UI_PANEL_PADDING;
    if (h)
        *h = panel->h - PZ_UI_PANEL_TITLE_HEIGHT - 2 * PZ_UI_PANEL_PADDING;
}

/* ============================================================================
 * Layout Helpers
 * ============================================================================
 */

float
pz_ui_x(pz_editor_ui *ui, float x)
{
    if (!ui) {
        return x;
    }
    if (x < 0) {
        return ui->screen_width + x;
    }
    return x;
}

float
pz_ui_y(pz_editor_ui *ui, float y)
{
    if (!ui) {
        return y;
    }
    if (y < 0) {
        return ui->screen_height + y;
    }
    return y;
}

const pz_ui_colors *
pz_ui_get_colors(pz_editor_ui *ui)
{
    return ui ? &ui->colors : &DEFAULT_COLORS;
}

void
pz_ui_set_colors(pz_editor_ui *ui, const pz_ui_colors *colors)
{
    if (ui && colors) {
        ui->colors = *colors;
    }
}
