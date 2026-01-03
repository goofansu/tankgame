/*
 * Tank Game - Background Renderer Implementation
 */

#include "pz_background.h"

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

/* ============================================================================
 * Types
 * ============================================================================
 */

struct pz_background {
    // Rendering resources
    pz_shader_handle shader;
    pz_pipeline_handle pipeline;
    pz_buffer_handle quad_buffer;
    bool render_ready;

    // Current settings
    pz_background_type type;
    pz_vec3 color_start;
    pz_vec3 color_end;
    pz_gradient_direction gradient_dir;
};

/* ============================================================================
 * Quad Vertex Data
 * ============================================================================
 */

// Fullscreen quad vertices (NDC: -1 to 1)
typedef struct {
    float x, y;
} bg_vertex;

static const bg_vertex QUAD_VERTICES[] = {
    { -1.0f, -1.0f }, // Bottom-left
    { 1.0f, -1.0f }, // Bottom-right
    { 1.0f, 1.0f }, // Top-right
    { -1.0f, -1.0f }, // Bottom-left
    { 1.0f, 1.0f }, // Top-right
    { -1.0f, 1.0f }, // Top-left
};

/* ============================================================================
 * Implementation
 * ============================================================================
 */

pz_background *
pz_background_create(pz_renderer *renderer)
{
    pz_background *bg = pz_calloc(1, sizeof(pz_background));
    if (!bg) {
        return NULL;
    }

    // Default: dark gray solid
    bg->type = PZ_BACKGROUND_COLOR;
    bg->color_start = (pz_vec3) { 0.2f, 0.2f, 0.25f };
    bg->color_end = (pz_vec3) { 0.1f, 0.1f, 0.15f };
    bg->gradient_dir = PZ_GRADIENT_VERTICAL;

    // Create shader
    bg->shader = pz_renderer_load_shader(renderer, "shaders/background.vert",
        "shaders/background.frag", "background");

    if (bg->shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to load background shader");
        pz_free(bg);
        return NULL;
    }

    // Create vertex buffer for fullscreen quad
    pz_buffer_desc buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STATIC,
        .data = QUAD_VERTICES,
        .size = sizeof(QUAD_VERTICES),
    };
    bg->quad_buffer = pz_renderer_create_buffer(renderer, &buf_desc);

    if (bg->quad_buffer == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to create background quad buffer");
        pz_renderer_destroy_shader(renderer, bg->shader);
        pz_free(bg);
        return NULL;
    }

    // Create pipeline
    pz_vertex_attr attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT2, .offset = 0 },
    };

    pz_pipeline_desc pipe_desc = {
        .shader = bg->shader,
        .vertex_layout = {
            .attrs = attrs,
            .attr_count = 1,
            .stride = sizeof(bg_vertex),
        },
        .blend = PZ_BLEND_NONE,
        .depth = PZ_DEPTH_NONE,  // No depth test/write for background
        .cull = PZ_CULL_NONE,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };

    bg->pipeline = pz_renderer_create_pipeline(renderer, &pipe_desc);
    if (bg->pipeline == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to create background pipeline");
        pz_renderer_destroy_buffer(renderer, bg->quad_buffer);
        pz_renderer_destroy_shader(renderer, bg->shader);
        pz_free(bg);
        return NULL;
    }

    bg->render_ready = true;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Background renderer created");

    return bg;
}

void
pz_background_destroy(pz_background *bg, pz_renderer *renderer)
{
    if (!bg) {
        return;
    }

    if (bg->pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(renderer, bg->pipeline);
    }
    if (bg->quad_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(renderer, bg->quad_buffer);
    }
    if (bg->shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(renderer, bg->shader);
    }

    pz_free(bg);
}

void
pz_background_set_from_map(pz_background *bg, const pz_map *map)
{
    if (!bg || !map) {
        return;
    }

    const pz_map_background *mb = pz_map_get_background(map);
    if (!mb) {
        return;
    }

    bg->type = mb->type;
    bg->color_start = mb->color;
    bg->color_end = mb->color_end;
    bg->gradient_dir = mb->gradient_dir;
}

void
pz_background_set_color(pz_background *bg, pz_vec3 color)
{
    if (!bg) {
        return;
    }

    bg->type = PZ_BACKGROUND_COLOR;
    bg->color_start = color;
}

void
pz_background_set_gradient(pz_background *bg, pz_vec3 color_start,
    pz_vec3 color_end, pz_gradient_direction direction)
{
    if (!bg) {
        return;
    }

    bg->type = PZ_BACKGROUND_GRADIENT;
    bg->color_start = color_start;
    bg->color_end = color_end;
    bg->gradient_dir = direction;
}

void
pz_background_render(pz_background *bg, pz_renderer *renderer,
    int viewport_width, int viewport_height)
{
    if (!bg || !bg->render_ready) {
        return;
    }

    // Set uniforms
    pz_renderer_set_uniform_vec3(
        renderer, bg->shader, "u_color_start", bg->color_start);
    pz_renderer_set_uniform_vec3(
        renderer, bg->shader, "u_color_end", bg->color_end);

    // Gradient type: 0 = solid, 1 = vertical, 2 = radial
    int gradient_type = 0;
    if (bg->type == PZ_BACKGROUND_GRADIENT) {
        gradient_type = (bg->gradient_dir == PZ_GRADIENT_RADIAL) ? 2 : 1;
    }
    pz_renderer_set_uniform_int(
        renderer, bg->shader, "u_gradient_type", gradient_type);

    // Aspect ratio for radial gradient (make it circular, not elliptical)
    float aspect_x = 1.0f;
    float aspect_y = 1.0f;
    if (viewport_width > 0 && viewport_height > 0) {
        if (viewport_width > viewport_height) {
            aspect_x = (float)viewport_width / (float)viewport_height;
        } else {
            aspect_y = (float)viewport_height / (float)viewport_width;
        }
    }
    pz_renderer_set_uniform_vec2(
        renderer, bg->shader, "u_aspect", (pz_vec2) { aspect_x, aspect_y });

    // Draw the fullscreen quad
    pz_draw_cmd cmd = {
        .pipeline = bg->pipeline,
        .vertex_buffer = bg->quad_buffer,
        .index_buffer = PZ_INVALID_HANDLE,
        .vertex_count = 6,
        .index_count = 0,
        .vertex_offset = 0,
        .index_offset = 0,
    };
    pz_renderer_draw(renderer, &cmd);
}
