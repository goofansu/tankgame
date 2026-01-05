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

#define CURSOR_SIZE 32.0f // Base cursor size in pixels
#define CROSSHAIR_RADIUS 14.0f // Circle radius
#define CROSSHAIR_TICK_LEN 8.0f // Length of tick marks outside circle
#define CROSSHAIR_GAP 4.0f // Gap at center
#define CROSSHAIR_CENTER_SIZE 3.0f // Size of center cross
#define CIRCLE_SEGMENTS 48 // Number of segments for circle
#define OUTLINE_WIDTH 2.5f // Black outline thickness

#define ARROW_LENGTH 24.0f // Arrow length
#define ARROW_WIDTH 16.0f // Arrow width at base

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

    // Draw outline (black, thicker)
    float outline_thick = 3.0f;
    float fill_thick = 1.5f;

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

    // Arrow points:
    // Tip at (x, y), pointing up-left
    // Main body goes down-right

    float len = ARROW_LENGTH;
    float width = ARROW_WIDTH;

    // Arrow shape vertices (tip at origin, pointing up-left style)
    // Point 0: Tip (hotspot)
    // Point 1: Bottom of main triangle (right side)
    // Point 2: Notch (where stem meets head)
    // Point 3: Bottom of main triangle (left side, along edge)
    // Point 4: End of stem (right)
    // Point 5: End of stem (left)

    float tip_x = x;
    float tip_y = y;

    // Main arrow body direction (pointing down-right from tip)
    float dir_x = 0.7071f; // 45 degrees
    float dir_y = 0.7071f;

    // Perpendicular for width
    float perp_x = -dir_y;
    float perp_y = dir_x;

    // Key points
    float base_x = tip_x + dir_x * len;
    float base_y = tip_y + dir_y * len;

    float left_x = tip_x + dir_x * len * 0.7f + perp_x * width * 0.5f;
    float left_y = tip_y + dir_y * len * 0.7f + perp_y * width * 0.5f;

    float right_x = tip_x + dir_x * len * 0.35f - perp_x * width * 0.15f;
    float right_y = tip_y + dir_y * len * 0.35f - perp_y * width * 0.15f;

    float notch_x = tip_x + dir_x * len * 0.45f + perp_x * width * 0.1f;
    float notch_y = tip_y + dir_y * len * 0.45f + perp_y * width * 0.1f;

    float stem_end_x = base_x + perp_x * width * 0.15f;
    float stem_end_y = base_y + perp_y * width * 0.15f;

    // Draw black outline (slightly larger)
    float o = 1.5f; // Outline offset

    // Outline - draw as thick lines around the shape
    add_line(cursor, tip_x, tip_y, left_x, left_y, black, 4.0f);
    add_line(cursor, left_x, left_y, notch_x, notch_y, black, 4.0f);
    add_line(cursor, notch_x, notch_y, stem_end_x, stem_end_y, black, 4.0f);
    add_line(cursor, stem_end_x, stem_end_y, base_x, base_y, black, 4.0f);
    add_line(cursor, base_x, base_y, right_x, right_y, black, 4.0f);
    add_line(cursor, right_x, right_y, tip_x, tip_y, black, 4.0f);

    // Fill - main arrow triangles (white)
    // Main head triangle
    add_triangle(cursor, tip_x, tip_y, left_x, left_y, right_x, right_y, white);

    // Connect to notch area
    add_triangle(
        cursor, right_x, right_y, left_x, left_y, notch_x, notch_y, white);

    // Stem
    add_triangle(cursor, notch_x, notch_y, stem_end_x, stem_end_y, base_x,
        base_y, white);
    add_triangle(
        cursor, notch_x, notch_y, base_x, base_y, right_x, right_y, white);
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
