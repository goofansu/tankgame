/*
 * Tank Game - Custom Cursor Implementation
 */

#include "pz_cursor.h"
#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include <math.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================
 */

#define CURSOR_SIZE 96.0f // Base cursor size in pixels (3x)
#define CROSSHAIR_RADIUS 21.0f // Circle radius (1.5x)
#define CROSSHAIR_TICK_LEN 12.0f // Length of tick marks outside circle (1.5x)
#define CROSSHAIR_GAP 6.0f // Gap at center (1.5x)
#define CROSSHAIR_CENTER_SIZE 4.5f // Size of center cross (1.5x)
#define CIRCLE_SEGMENTS 48 // Number of segments for circle
#define OUTLINE_WIDTH 7.5f // Black outline thickness (3x)

#define ARROW_LENGTH 36.0f // Arrow length (1.5x)
#define ARROW_WIDTH 24.0f // Arrow width at base (1.5x)

#define MAX_VERTICES 1024 // Maximum vertices for cursor rendering

/* ============================================================================
 * Vertex Structure
 * ============================================================================
 */

typedef struct cursor_vertex {
    float x, y; // Position (screen space)
    float r, g, b, a; // Color
} cursor_vertex;

/* ============================================================================
 * Cursor Structure
 * ============================================================================
 */

struct pz_cursor {
    pz_renderer *renderer;
    bool visible;
    pz_cursor_type type;
    float x, y; // Screen position

    // Rendering resources
    pz_shader_handle shader;
    pz_pipeline_handle line_pipeline;
    pz_pipeline_handle triangle_pipeline;
    pz_buffer_handle vb;

    // Vertex data
    cursor_vertex *vertices;
    int vertex_count;

    // Viewport cache
    int viewport_width;
    int viewport_height;
};

/* ============================================================================
 * Internal Drawing Functions
 * ============================================================================
 */

static void
add_line(pz_cursor *cursor, float x0, float y0, float x1, float y1,
    pz_vec4 color, float thickness)
{
    if (cursor->vertex_count + 6 > MAX_VERTICES)
        return;

    // Calculate perpendicular direction for thickness
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f)
        return;

    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;

    cursor_vertex *v = &cursor->vertices[cursor->vertex_count];

    // Two triangles forming a thick line
    v[0] = (cursor_vertex) { x0 - nx, y0 - ny, color.x, color.y, color.z,
        color.w };
    v[1] = (cursor_vertex) { x0 + nx, y0 + ny, color.x, color.y, color.z,
        color.w };
    v[2] = (cursor_vertex) { x1 + nx, y1 + ny, color.x, color.y, color.z,
        color.w };

    v[3] = (cursor_vertex) { x0 - nx, y0 - ny, color.x, color.y, color.z,
        color.w };
    v[4] = (cursor_vertex) { x1 + nx, y1 + ny, color.x, color.y, color.z,
        color.w };
    v[5] = (cursor_vertex) { x1 - nx, y1 - ny, color.x, color.y, color.z,
        color.w };

    cursor->vertex_count += 6;
}

static void
add_circle(pz_cursor *cursor, float cx, float cy, float radius, pz_vec4 color,
    float thickness)
{
    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float a0 = (float)i / CIRCLE_SEGMENTS * 2.0f * PZ_PI;
        float a1 = (float)(i + 1) / CIRCLE_SEGMENTS * 2.0f * PZ_PI;

        float x0 = cx + cosf(a0) * radius;
        float y0 = cy + sinf(a0) * radius;
        float x1 = cx + cosf(a1) * radius;
        float y1 = cy + sinf(a1) * radius;

        add_line(cursor, x0, y0, x1, y1, color, thickness);
    }
}

static void
add_triangle(pz_cursor *cursor, float x0, float y0, float x1, float y1,
    float x2, float y2, pz_vec4 color)
{
    if (cursor->vertex_count + 3 > MAX_VERTICES)
        return;

    cursor_vertex *v = &cursor->vertices[cursor->vertex_count];
    v[0] = (cursor_vertex) { x0, y0, color.x, color.y, color.z, color.w };
    v[1] = (cursor_vertex) { x1, y1, color.x, color.y, color.z, color.w };
    v[2] = (cursor_vertex) { x2, y2, color.x, color.y, color.z, color.w };

    cursor->vertex_count += 3;
}

static void
build_crosshair(pz_cursor *cursor, float cx, float cy)
{
    pz_vec4 black = { 0.0f, 0.0f, 0.0f, 1.0f };
    pz_vec4 white = { 1.0f, 1.0f, 1.0f, 1.0f };

    float radius = CROSSHAIR_RADIUS;
    float tick_len = CROSSHAIR_TICK_LEN;
    float gap = CROSSHAIR_GAP;
    float center_size = CROSSHAIR_CENTER_SIZE;

    // Draw outline (black, thicker) - scaled 1.5x
    float outline_thick = 4.5f;
    float fill_thick = 2.25f;

    // Circle outline
    add_circle(cursor, cx, cy, radius, black, outline_thick);

    // Tick marks outline (extending outward from circle)
    // Top
    add_line(cursor, cx, cy - radius, cx, cy - radius - tick_len, black,
        outline_thick);
    // Bottom
    add_line(cursor, cx, cy + radius, cx, cy + radius + tick_len, black,
        outline_thick);
    // Left
    add_line(cursor, cx - radius, cy, cx - radius - tick_len, cy, black,
        outline_thick);
    // Right
    add_line(cursor, cx + radius, cy, cx + radius + tick_len, cy, black,
        outline_thick);

    // Center cross outline
    add_line(cursor, cx - center_size, cy, cx + center_size, cy, black,
        outline_thick);
    add_line(cursor, cx, cy - center_size, cx, cy + center_size, black,
        outline_thick);

    // Inner tick marks outline (from gap to circle)
    add_line(cursor, cx, cy - gap, cx, cy - radius, black, outline_thick);
    add_line(cursor, cx, cy + gap, cx, cy + radius, black, outline_thick);
    add_line(cursor, cx - gap, cy, cx - radius, cy, black, outline_thick);
    add_line(cursor, cx + gap, cy, cx + radius, cy, black, outline_thick);

    // Draw fill (white, thinner)
    // Circle fill
    add_circle(cursor, cx, cy, radius, white, fill_thick);

    // Tick marks fill
    add_line(
        cursor, cx, cy - radius, cx, cy - radius - tick_len, white, fill_thick);
    add_line(
        cursor, cx, cy + radius, cx, cy + radius + tick_len, white, fill_thick);
    add_line(
        cursor, cx - radius, cy, cx - radius - tick_len, cy, white, fill_thick);
    add_line(
        cursor, cx + radius, cy, cx + radius + tick_len, cy, white, fill_thick);

    // Center cross fill
    add_line(
        cursor, cx - center_size, cy, cx + center_size, cy, white, fill_thick);
    add_line(
        cursor, cx, cy - center_size, cx, cy + center_size, white, fill_thick);

    // Inner tick marks fill
    add_line(cursor, cx, cy - gap, cx, cy - radius, white, fill_thick);
    add_line(cursor, cx, cy + gap, cx, cy + radius, white, fill_thick);
    add_line(cursor, cx - gap, cy, cx - radius, cy, white, fill_thick);
    add_line(cursor, cx + gap, cy, cx + radius, cy, white, fill_thick);
}

static void
build_arrow(pz_cursor *cursor, float x, float y)
{
    pz_vec4 black = { 0.0f, 0.0f, 0.0f, 1.0f };
    pz_vec4 white = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Simple triangle cursor
    float scale = ARROW_LENGTH / 24.0f;

    // Three vertices: tip, bottom-left, right point
    float p0_x = x; // Tip (hotspot)
    float p0_y = y;
    float p1_x = x; // Bottom of left edge
    float p1_y = y + 32.0f * scale;
    float p2_x = x + 22.0f * scale; // Right point
    float p2_y = y + 20.0f * scale;

    // Draw black outline
    float line_thick = 4.5f * scale;
    add_line(cursor, p0_x, p0_y, p1_x, p1_y, black, line_thick);
    add_line(cursor, p1_x, p1_y, p2_x, p2_y, black, line_thick);
    add_line(cursor, p2_x, p2_y, p0_x, p0_y, black, line_thick);

    // Fill with white
    add_triangle(cursor, p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, white);
}

/* ============================================================================
 * Public API
 * ============================================================================
 */

pz_cursor *
pz_cursor_create(pz_renderer *renderer)
{
    pz_cursor *cursor = pz_calloc(1, sizeof(pz_cursor));
    cursor->renderer = renderer;
    cursor->visible = true;
    cursor->type = PZ_CURSOR_CROSSHAIR;
    cursor->x = 0.0f;
    cursor->y = 0.0f;

    pz_renderer_get_viewport(
        renderer, &cursor->viewport_width, &cursor->viewport_height);

    // Create shader (uses pre-compiled cursor shader)
    pz_shader_desc shader_desc = {
        .name = "cursor",
    };
    cursor->shader = pz_renderer_create_shader(renderer, &shader_desc);

    if (cursor->shader == PZ_INVALID_HANDLE) {
        pz_log(
            PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to create cursor shader");
        pz_free(cursor);
        return NULL;
    }

    // Create pipeline for triangles
    pz_vertex_attr attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT2, .offset = 0 },
        { .name = "a_color",
            .type = PZ_ATTR_FLOAT4,
            .offset = 2 * sizeof(float) },
    };

    pz_pipeline_desc pipeline_desc = {
        .shader = cursor->shader,
        .vertex_layout = {
            .attrs = attrs,
            .attr_count = 2,
            .stride = sizeof(cursor_vertex),
        },
        .blend = PZ_BLEND_ALPHA,
        .depth = PZ_DEPTH_NONE,
        .cull = PZ_CULL_NONE,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    cursor->triangle_pipeline
        = pz_renderer_create_pipeline(renderer, &pipeline_desc);

    // Create vertex buffer
    cursor->vertices = pz_alloc(MAX_VERTICES * sizeof(cursor_vertex));

    pz_buffer_desc vb_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_DYNAMIC,
        .data = NULL,
        .size = MAX_VERTICES * sizeof(cursor_vertex),
    };
    cursor->vb = pz_renderer_create_buffer(renderer, &vb_desc);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Custom cursor renderer created");

    return cursor;
}

void
pz_cursor_destroy(pz_cursor *cursor)
{
    if (!cursor)
        return;

    pz_renderer_destroy_buffer(cursor->renderer, cursor->vb);
    pz_renderer_destroy_pipeline(cursor->renderer, cursor->triangle_pipeline);
    pz_renderer_destroy_shader(cursor->renderer, cursor->shader);
    pz_free(cursor->vertices);
    pz_free(cursor);
}

void
pz_cursor_set_type(pz_cursor *cursor, pz_cursor_type type)
{
    if (cursor)
        cursor->type = type;
}

void
pz_cursor_set_position(pz_cursor *cursor, float x, float y)
{
    if (cursor) {
        cursor->x = x;
        cursor->y = y;
    }
}

void
pz_cursor_set_visible(pz_cursor *cursor, bool visible)
{
    if (cursor)
        cursor->visible = visible;
}

bool
pz_cursor_is_visible(pz_cursor *cursor)
{
    return cursor ? cursor->visible : false;
}

void
pz_cursor_render(pz_cursor *cursor)
{
    if (!cursor || !cursor->visible)
        return;

    // Update viewport
    pz_renderer_get_viewport(
        cursor->renderer, &cursor->viewport_width, &cursor->viewport_height);

    // Reset vertices
    cursor->vertex_count = 0;

    // Build cursor geometry
    switch (cursor->type) {
    case PZ_CURSOR_CROSSHAIR:
        build_crosshair(cursor, cursor->x, cursor->y);
        break;
    case PZ_CURSOR_ARROW:
        build_arrow(cursor, cursor->x, cursor->y);
        break;
    }

    if (cursor->vertex_count == 0)
        return;

    // Upload vertices
    pz_renderer_update_buffer(cursor->renderer, cursor->vb, 0, cursor->vertices,
        cursor->vertex_count * sizeof(cursor_vertex));

    // Set uniforms
    pz_vec2 screen_size
        = { (float)cursor->viewport_width, (float)cursor->viewport_height };
    pz_renderer_set_uniform_vec2(
        cursor->renderer, cursor->shader, "u_screen_size", screen_size);

    // Draw
    pz_draw_cmd cmd = {
        .pipeline = cursor->triangle_pipeline,
        .vertex_buffer = cursor->vb,
        .vertex_count = cursor->vertex_count,
    };
    pz_renderer_draw(cursor->renderer, &cmd);
}
