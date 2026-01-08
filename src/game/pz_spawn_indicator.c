/*
 * Tank Game - Spawn Indicator System Implementation
 */

#include "pz_spawn_indicator.h"
#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * Constants
 * ============================================================================
 */

#define MAX_INDICATOR_VERTICES 512 // Enough for multiple indicators
#define CIRCLE_SEGMENTS 24 // Number of segments for circle rendering

/* ============================================================================
 * Internal Types
 * ============================================================================
 */

typedef struct indicator_vertex {
    float x, y; // Screen position
    float u, v; // Unused texcoord (to match ui_quad shader)
    float r, g, b, a; // Color
} indicator_vertex;

struct pz_spawn_indicator_renderer {
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    pz_buffer_handle vertex_buffer;
    indicator_vertex *vertices;
    int vertex_count;
    int max_vertices;
};

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

pz_spawn_indicator_renderer *
pz_spawn_indicator_create(pz_renderer *renderer)
{
    if (!renderer) {
        return NULL;
    }

    pz_spawn_indicator_renderer *sir
        = pz_calloc(1, sizeof(pz_spawn_indicator_renderer));

    sir->max_vertices = MAX_INDICATOR_VERTICES;
    sir->vertices = pz_calloc(sir->max_vertices, sizeof(indicator_vertex));

    // Create shader (reuse ui_quad shader which has same vertex format)
    pz_shader_desc shader_desc = {
        .name = "ui_quad",
    };
    sir->shader = pz_renderer_create_shader(renderer, &shader_desc);

    if (sir->shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to create spawn indicator shader");
        pz_free(sir->vertices);
        pz_free(sir);
        return NULL;
    }

    // Create pipeline (matching ui_quad shader vertex format)
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
        .shader = sir->shader,
        .vertex_layout = { .attrs = attrs,
            .attr_count = 3,
            .stride = sizeof(indicator_vertex) },
        .blend = PZ_BLEND_ALPHA,
        .depth = PZ_DEPTH_NONE,
        .cull = PZ_CULL_NONE,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };

    sir->pipeline = pz_renderer_create_pipeline(renderer, &desc);

    if (sir->pipeline == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to create spawn indicator pipeline");
        pz_renderer_destroy_shader(renderer, sir->shader);
        pz_free(sir->vertices);
        pz_free(sir);
        return NULL;
    }

    // Create vertex buffer
    pz_buffer_desc buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_DYNAMIC,
        .size = sir->max_vertices * sizeof(indicator_vertex),
    };
    sir->vertex_buffer = pz_renderer_create_buffer(renderer, &buf_desc);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Spawn indicator renderer created");
    return sir;
}

void
pz_spawn_indicator_destroy(
    pz_spawn_indicator_renderer *sir, pz_renderer *renderer)
{
    if (!sir) {
        return;
    }

    if (sir->vertex_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(renderer, sir->vertex_buffer);
    }
    if (sir->pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(renderer, sir->pipeline);
    }
    if (sir->shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(renderer, sir->shader);
    }
    if (sir->vertices) {
        pz_free(sir->vertices);
    }

    pz_free(sir);
    pz_log(
        PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Spawn indicator renderer destroyed");
}

/* ============================================================================
 * Internal Drawing Helpers
 * ============================================================================
 */

static void
add_triangle(pz_spawn_indicator_renderer *sir, float x1, float y1, float x2,
    float y2, float x3, float y3, pz_vec4 color)
{
    if (sir->vertex_count + 3 > sir->max_vertices) {
        return;
    }

    indicator_vertex *v = &sir->vertices[sir->vertex_count];

    v[0].x = x1;
    v[0].y = y1;
    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[0].r = color.x;
    v[0].g = color.y;
    v[0].b = color.z;
    v[0].a = color.w;

    v[1].x = x2;
    v[1].y = y2;
    v[1].u = 0.0f;
    v[1].v = 0.0f;
    v[1].r = color.x;
    v[1].g = color.y;
    v[1].b = color.z;
    v[1].a = color.w;

    v[2].x = x3;
    v[2].y = y3;
    v[2].u = 0.0f;
    v[2].v = 0.0f;
    v[2].r = color.x;
    v[2].g = color.y;
    v[2].b = color.z;
    v[2].a = color.w;

    sir->vertex_count += 3;
}

static void
add_filled_circle(pz_spawn_indicator_renderer *sir, float cx, float cy,
    float radius, pz_vec4 color)
{
    // Draw circle as triangle fan
    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float angle1 = (float)i / CIRCLE_SEGMENTS * 2.0f * PZ_PI;
        float angle2 = (float)(i + 1) / CIRCLE_SEGMENTS * 2.0f * PZ_PI;

        float x1 = cx + cosf(angle1) * radius;
        float y1 = cy + sinf(angle1) * radius;
        float x2 = cx + cosf(angle2) * radius;
        float y2 = cy + sinf(angle2) * radius;

        add_triangle(sir, cx, cy, x1, y1, x2, y2, color);
    }
}

static void
add_circle_outline(pz_spawn_indicator_renderer *sir, float cx, float cy,
    float radius, float thickness, pz_vec4 color)
{
    float inner_radius = radius - thickness * 0.5f;
    float outer_radius = radius + thickness * 0.5f;

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float angle1 = (float)i / CIRCLE_SEGMENTS * 2.0f * PZ_PI;
        float angle2 = (float)(i + 1) / CIRCLE_SEGMENTS * 2.0f * PZ_PI;

        float cos1 = cosf(angle1);
        float sin1 = sinf(angle1);
        float cos2 = cosf(angle2);
        float sin2 = sinf(angle2);

        float inner_x1 = cx + cos1 * inner_radius;
        float inner_y1 = cy + sin1 * inner_radius;
        float outer_x1 = cx + cos1 * outer_radius;
        float outer_y1 = cy + sin1 * outer_radius;
        float inner_x2 = cx + cos2 * inner_radius;
        float inner_y2 = cy + sin2 * inner_radius;
        float outer_x2 = cx + cos2 * outer_radius;
        float outer_y2 = cy + sin2 * outer_radius;

        // Two triangles for each segment
        add_triangle(sir, inner_x1, inner_y1, outer_x1, outer_y1, outer_x2,
            outer_y2, color);
        add_triangle(sir, inner_x1, inner_y1, outer_x2, outer_y2, inner_x2,
            inner_y2, color);
    }
}

static void
add_thick_line(pz_spawn_indicator_renderer *sir, float x1, float y1, float x2,
    float y2, float thickness, pz_vec4 color)
{
    // Calculate perpendicular direction
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) {
        return;
    }

    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;

    // Four corners of the thick line
    float ax = x1 + nx, ay = y1 + ny;
    float bx = x1 - nx, by = y1 - ny;
    float cx = x2 - nx, cy = y2 - ny;
    float dxx = x2 + nx, dxy = y2 + ny;

    add_triangle(sir, ax, ay, bx, by, cx, cy, color);
    add_triangle(sir, ax, ay, cx, cy, dxx, dxy, color);
}

/* ============================================================================
 * Rendering
 * ============================================================================
 */

void
pz_spawn_indicator_render(pz_spawn_indicator_renderer *sir,
    pz_renderer *renderer, pz_font_manager *font_mgr, pz_font *font,
    pz_tank_manager *tank_mgr, const pz_camera *camera, int screen_width,
    int screen_height)
{
    if (!sir || !renderer || !tank_mgr || !camera) {
        return;
    }

    sir->vertex_count = 0;

    float dpi_scale = pz_renderer_get_dpi_scale(renderer);

    // Process each tank with active spawn indicator
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &tank_mgr->tanks[i];

        // Only show for active, alive, player tanks with active indicator
        if (!(tank->flags & PZ_TANK_FLAG_ACTIVE)) {
            continue;
        }
        if (tank->flags & PZ_TANK_FLAG_DEAD) {
            continue;
        }
        if (!(tank->flags & PZ_TANK_FLAG_PLAYER)) {
            continue;
        }
        if (tank->spawn_indicator_timer <= 0.0f) {
            continue;
        }

        // Calculate alpha based on remaining time (fade out)
        float t = tank->spawn_indicator_timer / PZ_SPAWN_INDICATOR_DURATION;
        float alpha
            = pz_minf(t * 2.0f, 1.0f); // Full opacity for first half, fade out

        // Convert world position to screen space
        pz_vec3 world_pos = { tank->pos.x, 0.5f, tank->pos.y };
        pz_vec3 screen_pos = pz_camera_world_to_screen(camera, world_pos);

        // Skip if behind camera
        if (screen_pos.z < 0.0f || screen_pos.z > 1.0f) {
            continue;
        }

        // Convert to logical pixels (divide by DPI scale)
        float tank_screen_x = screen_pos.x / dpi_scale;
        float tank_screen_y = screen_pos.y / dpi_scale;

        // Indicator position (above tank)
        float indicator_y = tank_screen_y - PZ_SPAWN_INDICATOR_HEIGHT;
        float indicator_x = tank_screen_x;

        // Tank body color for the indicator
        pz_vec4 base_color = tank->body_color;
        pz_vec4 fill_color
            = { base_color.x, base_color.y, base_color.z, alpha * 0.9f };
        pz_vec4 outline_color = { pz_minf(base_color.x + 0.3f, 1.0f),
            pz_minf(base_color.y + 0.3f, 1.0f),
            pz_minf(base_color.z + 0.3f, 1.0f), alpha };
        pz_vec4 dark_outline = { base_color.x * 0.3f, base_color.y * 0.3f,
            base_color.z * 0.3f, alpha };

        // Draw the line from indicator to tank
        add_thick_line(sir, indicator_x,
            indicator_y + PZ_SPAWN_INDICATOR_CIRCLE_RADIUS, tank_screen_x,
            tank_screen_y - 10.0f, PZ_SPAWN_INDICATOR_LINE_WIDTH + 2.0f,
            dark_outline);
        add_thick_line(sir, indicator_x,
            indicator_y + PZ_SPAWN_INDICATOR_CIRCLE_RADIUS, tank_screen_x,
            tank_screen_y - 10.0f, PZ_SPAWN_INDICATOR_LINE_WIDTH, fill_color);

        // Draw filled circle (dark outline first, then fill)
        add_filled_circle(sir, indicator_x, indicator_y,
            PZ_SPAWN_INDICATOR_CIRCLE_RADIUS + 3.0f, dark_outline);
        add_filled_circle(sir, indicator_x, indicator_y,
            PZ_SPAWN_INDICATOR_CIRCLE_RADIUS, fill_color);
        add_circle_outline(sir, indicator_x, indicator_y,
            PZ_SPAWN_INDICATOR_CIRCLE_RADIUS, 3.0f, outline_color);

        // Draw player number text
        if (font_mgr && font) {
            char label[8];
            snprintf(label, sizeof(label), "P%d", tank->player_number);

            pz_text_style style = PZ_TEXT_STYLE_DEFAULT(font, 24.0f);
            style.align_h = PZ_FONT_ALIGN_CENTER;
            style.align_v = PZ_FONT_ALIGN_MIDDLE;
            style.color = pz_vec4_new(1.0f, 1.0f, 1.0f, alpha);
            style.outline_width = 3.0f;
            style.outline_color = pz_vec4_new(0.0f, 0.0f, 0.0f, alpha);

            pz_font_draw(font_mgr, &style, indicator_x, indicator_y, label);
        }
    }

    // Flush the shapes
    if (sir->vertex_count > 0) {
        // Set up orthographic projection for screen space
        pz_mat4 ortho = pz_mat4_ortho(
            0, (float)screen_width, (float)screen_height, 0, -1, 1);

        pz_renderer_update_buffer(renderer, sir->vertex_buffer, 0,
            sir->vertices, sir->vertex_count * sizeof(indicator_vertex));

        pz_renderer_set_uniform_mat4(
            renderer, sir->shader, "u_projection", &ortho);

        pz_draw_cmd cmd = {
            .pipeline = sir->pipeline,
            .vertex_buffer = sir->vertex_buffer,
            .vertex_count = sir->vertex_count,
        };
        pz_renderer_draw(renderer, &cmd);
    }
}
