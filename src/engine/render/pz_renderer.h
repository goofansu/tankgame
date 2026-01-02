/*
 * Tank Game - Renderer Abstraction
 *
 * Backend-agnostic rendering API. All engine/game code uses this interface.
 * No OpenGL/Vulkan/etc types should leak outside of backend implementations.
 */

#ifndef PZ_RENDERER_H
#define PZ_RENDERER_H

#include "../../core/pz_math.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Opaque Handles
 *
 * These are returned by the renderer and passed back for operations.
 * ============================================================================
 */

typedef uint32_t pz_shader_handle;
typedef uint32_t pz_texture_handle;
typedef uint32_t pz_buffer_handle;
typedef uint32_t pz_pipeline_handle;
typedef uint32_t pz_render_target_handle;

#define PZ_INVALID_HANDLE 0

/* ============================================================================
 * Enums
 * ============================================================================
 */

typedef enum pz_renderer_backend {
    PZ_BACKEND_NULL = 0, // No-op backend for testing
    PZ_BACKEND_GL33, // OpenGL 3.3 Core Profile
} pz_renderer_backend;

typedef enum pz_texture_format {
    PZ_TEXTURE_RGBA8 = 0,
    PZ_TEXTURE_RGB8,
    PZ_TEXTURE_R8,
    PZ_TEXTURE_DEPTH24,
} pz_texture_format;

typedef enum pz_texture_filter {
    PZ_FILTER_NEAREST = 0,
    PZ_FILTER_LINEAR,
    PZ_FILTER_LINEAR_MIPMAP,
} pz_texture_filter;

typedef enum pz_texture_wrap {
    PZ_WRAP_REPEAT = 0,
    PZ_WRAP_CLAMP,
    PZ_WRAP_MIRROR,
} pz_texture_wrap;

typedef enum pz_buffer_type {
    PZ_BUFFER_VERTEX = 0,
    PZ_BUFFER_INDEX,
} pz_buffer_type;

typedef enum pz_buffer_usage {
    PZ_BUFFER_STATIC = 0, // Set once, draw many
    PZ_BUFFER_DYNAMIC, // Update frequently
    PZ_BUFFER_STREAM, // Update every frame
} pz_buffer_usage;

typedef enum pz_primitive {
    PZ_PRIMITIVE_TRIANGLES = 0,
    PZ_PRIMITIVE_LINES,
    PZ_PRIMITIVE_POINTS,
} pz_primitive;

typedef enum pz_blend_mode {
    PZ_BLEND_NONE = 0,
    PZ_BLEND_ALPHA,
    PZ_BLEND_ADDITIVE,
    PZ_BLEND_MULTIPLY,
} pz_blend_mode;

typedef enum pz_depth_mode {
    PZ_DEPTH_NONE = 0,
    PZ_DEPTH_READ,
    PZ_DEPTH_WRITE,
    PZ_DEPTH_READ_WRITE,
} pz_depth_mode;

typedef enum pz_cull_mode {
    PZ_CULL_NONE = 0,
    PZ_CULL_BACK,
    PZ_CULL_FRONT,
} pz_cull_mode;

typedef enum pz_vertex_attr_type {
    PZ_ATTR_FLOAT = 0,
    PZ_ATTR_FLOAT2,
    PZ_ATTR_FLOAT3,
    PZ_ATTR_FLOAT4,
    PZ_ATTR_UINT8_NORM, // 4x uint8 normalized to 0-1
} pz_vertex_attr_type;

typedef enum pz_uniform_type {
    PZ_UNIFORM_FLOAT = 0,
    PZ_UNIFORM_VEC2,
    PZ_UNIFORM_VEC3,
    PZ_UNIFORM_VEC4,
    PZ_UNIFORM_MAT4,
    PZ_UNIFORM_INT,
    PZ_UNIFORM_SAMPLER,
} pz_uniform_type;

/* ============================================================================
 * Descriptor Structs
 * ============================================================================
 */

typedef struct pz_vertex_attr {
    const char *name; // Attribute name in shader
    pz_vertex_attr_type type; // Type of attribute
    size_t offset; // Offset in vertex struct
} pz_vertex_attr;

typedef struct pz_vertex_layout {
    pz_vertex_attr *attrs; // Array of attributes
    size_t attr_count; // Number of attributes
    size_t stride; // Size of one vertex
} pz_vertex_layout;

typedef struct pz_shader_desc {
    const char *vertex_source;
    const char *fragment_source;
    const char *name; // For debugging/error messages
} pz_shader_desc;

typedef struct pz_texture_desc {
    int width;
    int height;
    pz_texture_format format;
    pz_texture_filter filter;
    pz_texture_wrap wrap;
    const void *data; // Initial data (can be NULL)
} pz_texture_desc;

typedef struct pz_buffer_desc {
    pz_buffer_type type;
    pz_buffer_usage usage;
    const void *data;
    size_t size;
} pz_buffer_desc;

typedef struct pz_pipeline_desc {
    pz_shader_handle shader;
    pz_vertex_layout vertex_layout;
    pz_blend_mode blend;
    pz_depth_mode depth;
    pz_cull_mode cull;
    pz_primitive primitive;
} pz_pipeline_desc;

typedef struct pz_render_target_desc {
    int width;
    int height;
    pz_texture_format color_format;
    bool has_depth;
} pz_render_target_desc;

/* ============================================================================
 * Draw Command
 * ============================================================================
 */

typedef struct pz_draw_cmd {
    pz_pipeline_handle pipeline;
    pz_buffer_handle vertex_buffer;
    pz_buffer_handle index_buffer; // 0 if not indexed
    size_t vertex_count;
    size_t index_count; // 0 if not indexed
    size_t vertex_offset;
    size_t index_offset;
} pz_draw_cmd;

/* ============================================================================
 * Renderer Context
 * ============================================================================
 */

typedef struct pz_renderer pz_renderer;

typedef struct pz_renderer_config {
    pz_renderer_backend backend;
    void *window_handle; // SDL_Window* or similar
    int viewport_width;
    int viewport_height;
} pz_renderer_config;

// Renderer lifecycle
pz_renderer *pz_renderer_create(const pz_renderer_config *config);
void pz_renderer_destroy(pz_renderer *r);

// Get info
pz_renderer_backend pz_renderer_get_backend(pz_renderer *r);
void pz_renderer_get_viewport(pz_renderer *r, int *width, int *height);
void pz_renderer_set_viewport(pz_renderer *r, int width, int height);

// Shader operations
pz_shader_handle pz_renderer_create_shader(
    pz_renderer *r, const pz_shader_desc *desc);
void pz_renderer_destroy_shader(pz_renderer *r, pz_shader_handle handle);

// Texture operations
pz_texture_handle pz_renderer_create_texture(
    pz_renderer *r, const pz_texture_desc *desc);
void pz_renderer_update_texture(pz_renderer *r, pz_texture_handle handle, int x,
    int y, int width, int height, const void *data);
void pz_renderer_destroy_texture(pz_renderer *r, pz_texture_handle handle);

// Buffer operations
pz_buffer_handle pz_renderer_create_buffer(
    pz_renderer *r, const pz_buffer_desc *desc);
void pz_renderer_update_buffer(pz_renderer *r, pz_buffer_handle handle,
    size_t offset, const void *data, size_t size);
void pz_renderer_destroy_buffer(pz_renderer *r, pz_buffer_handle handle);

// Pipeline operations
pz_pipeline_handle pz_renderer_create_pipeline(
    pz_renderer *r, const pz_pipeline_desc *desc);
void pz_renderer_destroy_pipeline(pz_renderer *r, pz_pipeline_handle handle);

// Render target operations
pz_render_target_handle pz_renderer_create_render_target(
    pz_renderer *r, const pz_render_target_desc *desc);
pz_texture_handle pz_renderer_get_render_target_texture(
    pz_renderer *r, pz_render_target_handle handle);
void pz_renderer_destroy_render_target(
    pz_renderer *r, pz_render_target_handle handle);

// Frame operations
void pz_renderer_begin_frame(pz_renderer *r);
void pz_renderer_end_frame(pz_renderer *r);

// Render target binding
void pz_renderer_set_render_target(pz_renderer *r,
    pz_render_target_handle handle); // 0 = default framebuffer
void pz_renderer_clear(
    pz_renderer *r, float r_, float g, float b, float a, float depth);
void pz_renderer_clear_color(
    pz_renderer *r, float r_, float g, float b, float a);
void pz_renderer_clear_depth(pz_renderer *r, float depth);

// Uniforms
void pz_renderer_set_uniform_float(
    pz_renderer *r, pz_shader_handle shader, const char *name, float value);
void pz_renderer_set_uniform_vec2(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec2 value);
void pz_renderer_set_uniform_vec3(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec3 value);
void pz_renderer_set_uniform_vec4(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec4 value);
void pz_renderer_set_uniform_mat4(pz_renderer *r, pz_shader_handle shader,
    const char *name, const pz_mat4 *value);
void pz_renderer_set_uniform_int(
    pz_renderer *r, pz_shader_handle shader, const char *name, int value);

// Texture binding
void pz_renderer_bind_texture(
    pz_renderer *r, int slot, pz_texture_handle handle);

// Drawing
void pz_renderer_draw(pz_renderer *r, const pz_draw_cmd *cmd);

/* ============================================================================
 * Shader Loading Helpers
 * ============================================================================
 */

// Load shader from files (caller must pz_free the result, or use directly)
pz_shader_handle pz_renderer_load_shader(pz_renderer *r,
    const char *vertex_path, const char *fragment_path, const char *name);

// Reload a shader from files (for hot-reload)
bool pz_renderer_reload_shader(pz_renderer *r, pz_shader_handle handle,
    const char *vertex_path, const char *fragment_path);

/* ============================================================================
 * Screenshot
 * ============================================================================
 */

// Capture the current framebuffer to RGBA pixel data
// Returns pixel data (caller must pz_free), NULL on failure
// Pixels are in top-to-bottom order (suitable for image writing)
uint8_t *pz_renderer_screenshot(
    pz_renderer *r, int *out_width, int *out_height);

// Save a screenshot to a PNG file
// Returns true on success
bool pz_renderer_save_screenshot(pz_renderer *r, const char *path);

#endif // PZ_RENDERER_H
