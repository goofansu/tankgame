/*
 * Tank Game - Null Renderer Backend
 *
 * No-op implementation for testing and headless runs.
 */

#include "../../core/pz_log.h"
#include "../../core/pz_mem.h"
#include "pz_render_backend.h"

/* ============================================================================
 * Backend Data
 * ============================================================================
 */

typedef struct null_backend_data {
    uint32_t next_shader_id;
    uint32_t next_texture_id;
    uint32_t next_buffer_id;
    uint32_t next_pipeline_id;
    uint32_t next_render_target_id;
} null_backend_data;

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

static bool
null_init(pz_renderer *r, const pz_renderer_config *config)
{
    (void)config;

    null_backend_data *data = pz_calloc(1, sizeof(null_backend_data));
    if (!data)
        return false;

    data->next_shader_id = 1;
    data->next_texture_id = 1;
    data->next_buffer_id = 1;
    data->next_pipeline_id = 1;
    data->next_render_target_id = 1;

    r->backend_data = data;
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Null backend initialized");
    return true;
}

static void
null_shutdown(pz_renderer *r)
{
    if (r->backend_data) {
        pz_free(r->backend_data);
        r->backend_data = NULL;
    }
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Null backend shutdown");
}

/* ============================================================================
 * Viewport
 * ============================================================================
 */

static void
null_get_viewport(pz_renderer *r, int *width, int *height)
{
    *width = r->viewport_width;
    *height = r->viewport_height;
}

static void
null_set_viewport(pz_renderer *r, int width, int height)
{
    r->viewport_width = width;
    r->viewport_height = height;
}

static float
null_get_dpi_scale(pz_renderer *r)
{
    (void)r;
    return 1.0f;
}

/* ============================================================================
 * Shaders
 * ============================================================================
 */

static pz_shader_handle
null_create_shader(pz_renderer *r, const pz_shader_desc *desc)
{
    null_backend_data *data = r->backend_data;
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Null: create shader '%s'",
        desc->name ? desc->name : "unnamed");
    return data->next_shader_id++;
}

static void
null_destroy_shader(pz_renderer *r, pz_shader_handle handle)
{
    (void)r;
    (void)handle;
}

/* ============================================================================
 * Textures
 * ============================================================================
 */

static pz_texture_handle
null_create_texture(pz_renderer *r, const pz_texture_desc *desc)
{
    null_backend_data *data = r->backend_data;
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Null: create texture %dx%d",
        desc->width, desc->height);
    return data->next_texture_id++;
}

static void
null_update_texture(pz_renderer *r, pz_texture_handle handle, int x, int y,
    int width, int height, const void *tex_data)
{
    (void)r;
    (void)handle;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)tex_data;
}

static void
null_destroy_texture(pz_renderer *r, pz_texture_handle handle)
{
    (void)r;
    (void)handle;
}

/* ============================================================================
 * Buffers
 * ============================================================================
 */

static pz_buffer_handle
null_create_buffer(pz_renderer *r, const pz_buffer_desc *desc)
{
    null_backend_data *data = r->backend_data;
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Null: create buffer size=%zu",
        desc->size);
    return data->next_buffer_id++;
}

static void
null_update_buffer(pz_renderer *r, pz_buffer_handle handle, size_t offset,
    const void *buf_data, size_t size)
{
    (void)r;
    (void)handle;
    (void)offset;
    (void)buf_data;
    (void)size;
}

static void
null_destroy_buffer(pz_renderer *r, pz_buffer_handle handle)
{
    (void)r;
    (void)handle;
}

/* ============================================================================
 * Pipelines
 * ============================================================================
 */

static pz_pipeline_handle
null_create_pipeline(pz_renderer *r, const pz_pipeline_desc *desc)
{
    null_backend_data *data = r->backend_data;
    (void)desc;
    return data->next_pipeline_id++;
}

static void
null_destroy_pipeline(pz_renderer *r, pz_pipeline_handle handle)
{
    (void)r;
    (void)handle;
}

/* ============================================================================
 * Render Targets
 * ============================================================================
 */

static pz_render_target_handle
null_create_render_target(pz_renderer *r, const pz_render_target_desc *desc)
{
    null_backend_data *data = r->backend_data;
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Null: create render target %dx%d",
        desc->width, desc->height);
    return data->next_render_target_id++;
}

static pz_texture_handle
null_get_render_target_texture(pz_renderer *r, pz_render_target_handle handle)
{
    (void)r;
    // Return a fake texture handle
    return handle + 1000;
}

static void
null_destroy_render_target(pz_renderer *r, pz_render_target_handle handle)
{
    (void)r;
    (void)handle;
}

/* ============================================================================
 * Frame
 * ============================================================================
 */

static void
null_begin_frame(pz_renderer *r)
{
    (void)r;
}

static void
null_end_frame(pz_renderer *r)
{
    (void)r;
}

/* ============================================================================
 * Render Target Binding
 * ============================================================================
 */

static void
null_set_render_target(pz_renderer *r, pz_render_target_handle handle)
{
    (void)r;
    (void)handle;
}

static void
null_clear(pz_renderer *r, float cr, float g, float b, float a, float depth)
{
    (void)r;
    (void)cr;
    (void)g;
    (void)b;
    (void)a;
    (void)depth;
}

static void
null_clear_color(pz_renderer *r, float cr, float g, float b, float a)
{
    (void)r;
    (void)cr;
    (void)g;
    (void)b;
    (void)a;
}

static void
null_clear_depth(pz_renderer *r, float depth)
{
    (void)r;
    (void)depth;
}

/* ============================================================================
 * Uniforms
 * ============================================================================
 */

static void
null_set_uniform_float(
    pz_renderer *r, pz_shader_handle shader, const char *name, float value)
{
    (void)r;
    (void)shader;
    (void)name;
    (void)value;
}

static void
null_set_uniform_vec2(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec2 value)
{
    (void)r;
    (void)shader;
    (void)name;
    (void)value;
}

static void
null_set_uniform_vec3(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec3 value)
{
    (void)r;
    (void)shader;
    (void)name;
    (void)value;
}

static void
null_set_uniform_vec4(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec4 value)
{
    (void)r;
    (void)shader;
    (void)name;
    (void)value;
}

static void
null_set_uniform_mat4(pz_renderer *r, pz_shader_handle shader, const char *name,
    const pz_mat4 *value)
{
    (void)r;
    (void)shader;
    (void)name;
    (void)value;
}

static void
null_set_uniform_int(
    pz_renderer *r, pz_shader_handle shader, const char *name, int value)
{
    (void)r;
    (void)shader;
    (void)name;
    (void)value;
}

/* ============================================================================
 * Textures
 * ============================================================================
 */

static void
null_bind_texture(pz_renderer *r, int slot, pz_texture_handle handle)
{
    (void)r;
    (void)slot;
    (void)handle;
}

/* ============================================================================
 * Drawing
 * ============================================================================
 */

static void
null_draw(pz_renderer *r, const pz_draw_cmd *cmd)
{
    (void)r;
    (void)cmd;
}

/* ============================================================================
 * Screenshot
 * ============================================================================
 */

static uint8_t *
null_screenshot(pz_renderer *r, int *out_width, int *out_height)
{
    // Return a solid color image for the null backend
    int width = r->viewport_width;
    int height = r->viewport_height;

    size_t pixel_count = (size_t)width * (size_t)height * 4;
    uint8_t *pixels = pz_calloc(1, pixel_count);
    if (!pixels)
        return NULL;

    // Fill with cornflower blue for testing
    for (size_t i = 0; i < pixel_count; i += 4) {
        pixels[i + 0] = 100; // R
        pixels[i + 1] = 149; // G
        pixels[i + 2] = 237; // B
        pixels[i + 3] = 255; // A
    }

    *out_width = width;
    *out_height = height;
    return pixels;
}

/* ============================================================================
 * Vtable
 * ============================================================================
 */

static const pz_render_backend_vtable s_null_vtable = {
    .init = null_init,
    .shutdown = null_shutdown,
    .get_viewport = null_get_viewport,
    .set_viewport = null_set_viewport,
    .get_dpi_scale = null_get_dpi_scale,
    .create_shader = null_create_shader,
    .destroy_shader = null_destroy_shader,
    .create_texture = null_create_texture,
    .update_texture = null_update_texture,
    .destroy_texture = null_destroy_texture,
    .create_buffer = null_create_buffer,
    .update_buffer = null_update_buffer,
    .destroy_buffer = null_destroy_buffer,
    .create_pipeline = null_create_pipeline,
    .destroy_pipeline = null_destroy_pipeline,
    .create_render_target = null_create_render_target,
    .get_render_target_texture = null_get_render_target_texture,
    .destroy_render_target = null_destroy_render_target,
    .begin_frame = null_begin_frame,
    .end_frame = null_end_frame,
    .set_render_target = null_set_render_target,
    .clear = null_clear,
    .clear_color = null_clear_color,
    .clear_depth = null_clear_depth,
    .set_uniform_float = null_set_uniform_float,
    .set_uniform_vec2 = null_set_uniform_vec2,
    .set_uniform_vec3 = null_set_uniform_vec3,
    .set_uniform_vec4 = null_set_uniform_vec4,
    .set_uniform_mat4 = null_set_uniform_mat4,
    .set_uniform_int = null_set_uniform_int,
    .bind_texture = null_bind_texture,
    .draw = null_draw,
    .screenshot = null_screenshot,
};

const pz_render_backend_vtable *
pz_render_backend_null_vtable(void)
{
    return &s_null_vtable;
}

// Weak stub for render target reading (used by pz_renderer.c when GL33 not
// linked) The real implementation in pz_render_gl33.c overrides this.
__attribute__((weak)) uint8_t *
pz_render_gl33_read_render_target(pz_renderer *r,
    pz_render_target_handle handle, int *out_width, int *out_height)
{
    (void)r;
    (void)handle;
    (void)out_width;
    (void)out_height;
    return NULL; // Not supported in null backend
}
