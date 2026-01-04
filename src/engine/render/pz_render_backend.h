/*
 * Tank Game - Renderer Backend Interface
 *
 * Internal interface that backends must implement.
 * This header is NOT part of the public API.
 */

#ifndef PZ_RENDER_BACKEND_H
#define PZ_RENDER_BACKEND_H

#include "pz_renderer.h"

/* ============================================================================
 * Backend Vtable
 *
 * Each backend provides an implementation of these functions.
 * ============================================================================
 */

typedef struct pz_render_backend_vtable {
    // Lifecycle
    bool (*init)(pz_renderer *r, const pz_renderer_config *config);
    void (*shutdown)(pz_renderer *r);

    // Viewport
    void (*get_viewport)(pz_renderer *r, int *width, int *height);
    void (*set_viewport)(pz_renderer *r, int width, int height);
    float (*get_dpi_scale)(pz_renderer *r);

    // Shaders
    pz_shader_handle (*create_shader)(
        pz_renderer *r, const pz_shader_desc *desc);
    void (*destroy_shader)(pz_renderer *r, pz_shader_handle handle);

    // Textures
    pz_texture_handle (*create_texture)(
        pz_renderer *r, const pz_texture_desc *desc);
    void (*update_texture)(pz_renderer *r, pz_texture_handle handle, int x,
        int y, int width, int height, const void *data);
    void (*destroy_texture)(pz_renderer *r, pz_texture_handle handle);

    // Texture arrays (optional - can be NULL)
    pz_texture_handle (*create_texture_array)(pz_renderer *r, int width,
        int height, int layers, const void **data_per_layer,
        pz_texture_filter filter, pz_texture_wrap wrap);

    // Buffers
    pz_buffer_handle (*create_buffer)(
        pz_renderer *r, const pz_buffer_desc *desc);
    void (*update_buffer)(pz_renderer *r, pz_buffer_handle handle,
        size_t offset, const void *data, size_t size);
    void (*destroy_buffer)(pz_renderer *r, pz_buffer_handle handle);

    // Pipelines
    pz_pipeline_handle (*create_pipeline)(
        pz_renderer *r, const pz_pipeline_desc *desc);
    void (*destroy_pipeline)(pz_renderer *r, pz_pipeline_handle handle);

    // Render targets
    pz_render_target_handle (*create_render_target)(
        pz_renderer *r, const pz_render_target_desc *desc);
    pz_texture_handle (*get_render_target_texture)(
        pz_renderer *r, pz_render_target_handle handle);
    void (*destroy_render_target)(
        pz_renderer *r, pz_render_target_handle handle);

    // Frame
    void (*begin_frame)(pz_renderer *r);
    void (*end_frame)(pz_renderer *r);

    // Render target binding
    void (*set_render_target)(pz_renderer *r, pz_render_target_handle handle);
    void (*clear)(
        pz_renderer *r, float r_, float g, float b, float a, float depth);
    void (*clear_color)(pz_renderer *r, float r_, float g, float b, float a);
    void (*clear_depth)(pz_renderer *r, float depth);

    // Uniforms
    void (*set_uniform_float)(
        pz_renderer *r, pz_shader_handle shader, const char *name, float value);
    void (*set_uniform_vec2)(pz_renderer *r, pz_shader_handle shader,
        const char *name, pz_vec2 value);
    void (*set_uniform_vec3)(pz_renderer *r, pz_shader_handle shader,
        const char *name, pz_vec3 value);
    void (*set_uniform_vec4)(pz_renderer *r, pz_shader_handle shader,
        const char *name, pz_vec4 value);
    void (*set_uniform_mat4)(pz_renderer *r, pz_shader_handle shader,
        const char *name, const pz_mat4 *value);
    void (*set_uniform_int)(
        pz_renderer *r, pz_shader_handle shader, const char *name, int value);

    // Textures
    void (*bind_texture)(pz_renderer *r, int slot, pz_texture_handle handle);

    // Drawing
    void (*draw)(pz_renderer *r, const pz_draw_cmd *cmd);

    // Screenshot
    uint8_t *(*screenshot)(pz_renderer *r, int *out_width, int *out_height);

} pz_render_backend_vtable;

/* ============================================================================
 * Renderer Internal Structure
 *
 * The concrete renderer struct that backends populate.
 * ============================================================================
 */

struct pz_renderer {
    pz_renderer_backend backend_type;
    const pz_render_backend_vtable *vtable;

    // Common state
    int viewport_width;
    int viewport_height;
    void *window_handle;

    // Backend-specific data (each backend casts this)
    void *backend_data;
};

// Backend registration functions
const pz_render_backend_vtable *pz_render_backend_null_vtable(void);
const pz_render_backend_vtable *pz_render_backend_gl33_vtable(void);
const pz_render_backend_vtable *pz_render_backend_sokol_vtable(void);

#endif // PZ_RENDER_BACKEND_H
