/*
 * Tank Game - Renderer Implementation
 *
 * Dispatches to the appropriate backend.
 */

#include "pz_renderer.h"
#include "../../core/pz_log.h"
#include "../../core/pz_mem.h"
#include "../../core/pz_platform.h"
#include "pz_render_backend.h"

#include "third_party/stb_image_write.h"

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

pz_renderer *
pz_renderer_create(const pz_renderer_config *config)
{
    pz_renderer *r = pz_calloc(1, sizeof(pz_renderer));
    if (!r) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to allocate renderer");
        return NULL;
    }

    r->backend_type = config->backend;
    r->viewport_width = config->viewport_width;
    r->viewport_height = config->viewport_height;
    r->window_handle = config->window_handle;

    // Select backend vtable
    switch (config->backend) {
    case PZ_BACKEND_NULL:
        r->vtable = pz_render_backend_null_vtable();
        break;
    case PZ_BACKEND_GL33:
        r->vtable = pz_render_backend_gl33_vtable();
        break;
    case PZ_BACKEND_SOKOL:
#ifdef PZ_ENABLE_SOKOL
        r->vtable = pz_render_backend_sokol_vtable();
        break;
#else
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Sokol backend not enabled in this build");
        pz_free(r);
        return NULL;
#endif
    default:
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Unknown backend type: %d",
            config->backend);
        pz_free(r);
        return NULL;
    }

    // Initialize backend
    if (!r->vtable->init(r, config)) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to initialize backend");
        pz_free(r);
        return NULL;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Renderer created (backend=%d)",
        r->backend_type);
    return r;
}

void
pz_renderer_destroy(pz_renderer *r)
{
    if (!r)
        return;

    if (r->vtable && r->vtable->shutdown) {
        r->vtable->shutdown(r);
    }

    pz_free(r);
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Renderer destroyed");
}

/* ============================================================================
 * Info
 * ============================================================================
 */

pz_renderer_backend
pz_renderer_get_backend(pz_renderer *r)
{
    return r->backend_type;
}

void
pz_renderer_get_viewport(pz_renderer *r, int *width, int *height)
{
    r->vtable->get_viewport(r, width, height);
}

void
pz_renderer_set_viewport(pz_renderer *r, int width, int height)
{
    r->vtable->set_viewport(r, width, height);
}

/* ============================================================================
 * Shaders
 * ============================================================================
 */

pz_shader_handle
pz_renderer_create_shader(pz_renderer *r, const pz_shader_desc *desc)
{
    return r->vtable->create_shader(r, desc);
}

void
pz_renderer_destroy_shader(pz_renderer *r, pz_shader_handle handle)
{
    r->vtable->destroy_shader(r, handle);
}

/* ============================================================================
 * Textures
 * ============================================================================
 */

pz_texture_handle
pz_renderer_create_texture(pz_renderer *r, const pz_texture_desc *desc)
{
    return r->vtable->create_texture(r, desc);
}

void
pz_renderer_update_texture(pz_renderer *r, pz_texture_handle handle, int x,
    int y, int width, int height, const void *data)
{
    r->vtable->update_texture(r, handle, x, y, width, height, data);
}

void
pz_renderer_destroy_texture(pz_renderer *r, pz_texture_handle handle)
{
    r->vtable->destroy_texture(r, handle);
}

/* ============================================================================
 * Buffers
 * ============================================================================
 */

pz_buffer_handle
pz_renderer_create_buffer(pz_renderer *r, const pz_buffer_desc *desc)
{
    return r->vtable->create_buffer(r, desc);
}

void
pz_renderer_update_buffer(pz_renderer *r, pz_buffer_handle handle,
    size_t offset, const void *data, size_t size)
{
    r->vtable->update_buffer(r, handle, offset, data, size);
}

void
pz_renderer_destroy_buffer(pz_renderer *r, pz_buffer_handle handle)
{
    r->vtable->destroy_buffer(r, handle);
}

/* ============================================================================
 * Pipelines
 * ============================================================================
 */

pz_pipeline_handle
pz_renderer_create_pipeline(pz_renderer *r, const pz_pipeline_desc *desc)
{
    return r->vtable->create_pipeline(r, desc);
}

void
pz_renderer_destroy_pipeline(pz_renderer *r, pz_pipeline_handle handle)
{
    r->vtable->destroy_pipeline(r, handle);
}

/* ============================================================================
 * Render Targets
 * ============================================================================
 */

pz_render_target_handle
pz_renderer_create_render_target(
    pz_renderer *r, const pz_render_target_desc *desc)
{
    return r->vtable->create_render_target(r, desc);
}

pz_texture_handle
pz_renderer_get_render_target_texture(
    pz_renderer *r, pz_render_target_handle handle)
{
    return r->vtable->get_render_target_texture(r, handle);
}

void
pz_renderer_destroy_render_target(
    pz_renderer *r, pz_render_target_handle handle)
{
    r->vtable->destroy_render_target(r, handle);
}

/* ============================================================================
 * Frame
 * ============================================================================
 */

void
pz_renderer_begin_frame(pz_renderer *r)
{
    r->vtable->begin_frame(r);
}

void
pz_renderer_end_frame(pz_renderer *r)
{
    r->vtable->end_frame(r);
}

/* ============================================================================
 * Render Target Binding
 * ============================================================================
 */

void
pz_renderer_set_render_target(pz_renderer *r, pz_render_target_handle handle)
{
    r->vtable->set_render_target(r, handle);
}

void
pz_renderer_clear(
    pz_renderer *r, float r_, float g, float b, float a, float depth)
{
    r->vtable->clear(r, r_, g, b, a, depth);
}

void
pz_renderer_clear_color(pz_renderer *r, float r_, float g, float b, float a)
{
    r->vtable->clear_color(r, r_, g, b, a);
}

void
pz_renderer_clear_depth(pz_renderer *r, float depth)
{
    r->vtable->clear_depth(r, depth);
}

/* ============================================================================
 * Uniforms
 * ============================================================================
 */

void
pz_renderer_set_uniform_float(
    pz_renderer *r, pz_shader_handle shader, const char *name, float value)
{
    r->vtable->set_uniform_float(r, shader, name, value);
}

void
pz_renderer_set_uniform_vec2(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec2 value)
{
    r->vtable->set_uniform_vec2(r, shader, name, value);
}

void
pz_renderer_set_uniform_vec3(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec3 value)
{
    r->vtable->set_uniform_vec3(r, shader, name, value);
}

void
pz_renderer_set_uniform_vec4(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec4 value)
{
    r->vtable->set_uniform_vec4(r, shader, name, value);
}

void
pz_renderer_set_uniform_mat4(pz_renderer *r, pz_shader_handle shader,
    const char *name, const pz_mat4 *value)
{
    r->vtable->set_uniform_mat4(r, shader, name, value);
}

void
pz_renderer_set_uniform_int(
    pz_renderer *r, pz_shader_handle shader, const char *name, int value)
{
    r->vtable->set_uniform_int(r, shader, name, value);
}

/* ============================================================================
 * Textures
 * ============================================================================
 */

void
pz_renderer_bind_texture(pz_renderer *r, int slot, pz_texture_handle handle)
{
    r->vtable->bind_texture(r, slot, handle);
}

/* ============================================================================
 * Drawing
 * ============================================================================
 */

void
pz_renderer_draw(pz_renderer *r, const pz_draw_cmd *cmd)
{
    r->vtable->draw(r, cmd);
}

/* ============================================================================
 * Shader Loading Helpers
 * ============================================================================
 */

pz_shader_handle
pz_renderer_load_shader(pz_renderer *r, const char *vertex_path,
    const char *fragment_path, const char *name)
{
    char *vertex_src = pz_file_read_text(vertex_path);
    if (!vertex_src) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to load vertex shader: %s", vertex_path);
        return PZ_INVALID_HANDLE;
    }

    char *fragment_src = pz_file_read_text(fragment_path);
    if (!fragment_src) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to load fragment shader: %s", fragment_path);
        pz_free(vertex_src);
        return PZ_INVALID_HANDLE;
    }

    pz_shader_desc desc = {
        .vertex_source = vertex_src,
        .fragment_source = fragment_src,
        .name = name,
    };

    pz_shader_handle handle = pz_renderer_create_shader(r, &desc);

    pz_free(vertex_src);
    pz_free(fragment_src);

    return handle;
}

bool
pz_renderer_reload_shader(pz_renderer *r, pz_shader_handle handle,
    const char *vertex_path, const char *fragment_path)
{
    // For hot-reload: destroy old shader and create new one
    // The handle will change, so caller needs to update their reference

    // For now, just return false (not implemented)
    // TODO: Implement proper hot-reload that keeps the same handle
    (void)r;
    (void)handle;
    (void)vertex_path;
    (void)fragment_path;
    return false;
}

/* ============================================================================
 * Screenshot
 * ============================================================================
 */

uint8_t *
pz_renderer_screenshot(pz_renderer *r, int *out_width, int *out_height)
{
    if (!r || !r->vtable || !r->vtable->screenshot) {
        return NULL;
    }
    return r->vtable->screenshot(r, out_width, out_height);
}

bool
pz_renderer_save_screenshot(pz_renderer *r, const char *path)
{
    int width, height;
    uint8_t *pixels = pz_renderer_screenshot(r, &width, &height);
    if (!pixels) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to capture screenshot");
        return false;
    }

    // Ensure screenshots directory exists
    char *dir = pz_path_dirname(path);
    if (dir) {
        pz_dir_create(dir);
        pz_free(dir);
    }

    // Save as PNG
    int result = stbi_write_png(path, width, height, 4, pixels, width * 4);
    pz_free(pixels);

    if (result) {
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Screenshot saved: %s (%dx%d)",
            path, width, height);
        return true;
    } else {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to write screenshot: %s", path);
        return false;
    }
}

// Forward declaration - implemented in pz_render_gl33.c
extern uint8_t *pz_render_gl33_read_render_target(pz_renderer *r,
    pz_render_target_handle handle, int *out_width, int *out_height);
#ifdef PZ_ENABLE_SOKOL
extern uint8_t *pz_render_sokol_read_render_target(pz_renderer *r,
    pz_render_target_handle handle, int *out_width, int *out_height);
#endif

bool
pz_renderer_save_render_target(
    pz_renderer *r, pz_render_target_handle handle, const char *path)
{
    int width, height;
    uint8_t *pixels = NULL;

    if (r->backend_type == PZ_BACKEND_GL33) {
        pixels = pz_render_gl33_read_render_target(r, handle, &width, &height);
#ifdef PZ_ENABLE_SOKOL
    } else if (r->backend_type == PZ_BACKEND_SOKOL) {
        pixels = pz_render_sokol_read_render_target(r, handle, &width, &height);
#endif
    }

    if (!pixels) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to read render target pixels");
        return false;
    }

    // Ensure directory exists
    char *dir = pz_path_dirname(path);
    if (dir) {
        pz_dir_create(dir);
        pz_free(dir);
    }

    // Save as PNG
    int result = stbi_write_png(path, width, height, 4, pixels, width * 4);
    pz_free(pixels);

    if (result) {
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER,
            "Render target saved: %s (%dx%d)", path, width, height);
        return true;
    } else {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to write render target: %s", path);
        return false;
    }
}
