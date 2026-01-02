/*
 * Tank Game - OpenGL 3.3 Renderer Backend
 *
 * OpenGL 3.3 Core Profile implementation.
 */

#include "../../core/pz_log.h"
#include "../../core/pz_mem.h"
#include "pz_render_backend.h"

#ifdef __APPLE__
#    include <OpenGL/gl3.h>
#else
#    include <GL/gl.h>
#    include <GL/glext.h>
#endif

#include <SDL.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================
 */

#define MAX_SHADERS 64
#define MAX_TEXTURES 256
#define MAX_BUFFERS 256
#define MAX_PIPELINES 64
#define MAX_RENDER_TARGETS 32

/* ============================================================================
 * Resource Structures
 * ============================================================================
 */

typedef struct gl_shader {
    GLuint program;
    bool used;
} gl_shader;

typedef struct gl_texture {
    GLuint id;
    int width;
    int height;
    pz_texture_format format;
    bool used;
} gl_texture;

typedef struct gl_buffer {
    GLuint id;
    pz_buffer_type type;
    pz_buffer_usage usage;
    size_t size;
    bool used;
} gl_buffer;

typedef struct gl_pipeline {
    pz_shader_handle shader;
    GLuint vao;
    pz_blend_mode blend;
    pz_depth_mode depth;
    pz_cull_mode cull;
    pz_primitive primitive;
    pz_vertex_layout vertex_layout;
    pz_vertex_attr stored_attrs[8]; // Max 8 attributes
    bool used;
} gl_pipeline;

typedef struct gl_render_target {
    GLuint fbo;
    GLuint color_texture;
    GLuint depth_rbo;
    int width;
    int height;
    bool has_depth;
    bool used;
} gl_render_target;

/* ============================================================================
 * Backend Data
 * ============================================================================
 */

typedef struct gl33_backend_data {
    gl_shader shaders[MAX_SHADERS];
    gl_texture textures[MAX_TEXTURES];
    gl_buffer buffers[MAX_BUFFERS];
    gl_pipeline pipelines[MAX_PIPELINES];
    gl_render_target render_targets[MAX_RENDER_TARGETS];

    // Current state
    pz_shader_handle current_shader;
    pz_pipeline_handle current_pipeline;
} gl33_backend_data;

/* ============================================================================
 * Helper Functions
 * ============================================================================
 */

static const char *
gl_error_string(GLenum err)
{
    switch (err) {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "UNKNOWN_ERROR";
    }
}

static bool
gl_check_error(const char *context)
{
    GLenum err;
    bool had_error = false;
    while ((err = glGetError()) != GL_NO_ERROR) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "GL error at %s: %s (0x%04x)",
            context, gl_error_string(err), err);
        had_error = true;
    }
    return !had_error;
}

static GLenum
gl_texture_format(pz_texture_format fmt)
{
    switch (fmt) {
    case PZ_TEXTURE_RGBA8:
        return GL_RGBA;
    case PZ_TEXTURE_RGB8:
        return GL_RGB;
    case PZ_TEXTURE_R8:
        return GL_RED;
    case PZ_TEXTURE_DEPTH24:
        return GL_DEPTH_COMPONENT;
    default:
        return GL_RGBA;
    }
}

static GLenum
gl_texture_internal_format(pz_texture_format fmt)
{
    switch (fmt) {
    case PZ_TEXTURE_RGBA8:
        return GL_RGBA8;
    case PZ_TEXTURE_RGB8:
        return GL_RGB8;
    case PZ_TEXTURE_R8:
        return GL_R8;
    case PZ_TEXTURE_DEPTH24:
        return GL_DEPTH_COMPONENT24;
    default:
        return GL_RGBA8;
    }
}

static GLenum
gl_filter(pz_texture_filter filter)
{
    switch (filter) {
    case PZ_FILTER_NEAREST:
        return GL_NEAREST;
    case PZ_FILTER_LINEAR:
        return GL_LINEAR;
    case PZ_FILTER_LINEAR_MIPMAP:
        return GL_LINEAR_MIPMAP_LINEAR;
    default:
        return GL_LINEAR;
    }
}

static GLenum
gl_wrap(pz_texture_wrap wrap)
{
    switch (wrap) {
    case PZ_WRAP_REPEAT:
        return GL_REPEAT;
    case PZ_WRAP_CLAMP:
        return GL_CLAMP_TO_EDGE;
    case PZ_WRAP_MIRROR:
        return GL_MIRRORED_REPEAT;
    default:
        return GL_REPEAT;
    }
}

static GLenum
gl_buffer_target(pz_buffer_type type)
{
    switch (type) {
    case PZ_BUFFER_VERTEX:
        return GL_ARRAY_BUFFER;
    case PZ_BUFFER_INDEX:
        return GL_ELEMENT_ARRAY_BUFFER;
    default:
        return GL_ARRAY_BUFFER;
    }
}

static GLenum
gl_buffer_usage(pz_buffer_usage usage)
{
    switch (usage) {
    case PZ_BUFFER_STATIC:
        return GL_STATIC_DRAW;
    case PZ_BUFFER_DYNAMIC:
        return GL_DYNAMIC_DRAW;
    case PZ_BUFFER_STREAM:
        return GL_STREAM_DRAW;
    default:
        return GL_STATIC_DRAW;
    }
}

static GLenum
gl_primitive(pz_primitive prim)
{
    switch (prim) {
    case PZ_PRIMITIVE_TRIANGLES:
        return GL_TRIANGLES;
    case PZ_PRIMITIVE_LINES:
        return GL_LINES;
    case PZ_PRIMITIVE_POINTS:
        return GL_POINTS;
    default:
        return GL_TRIANGLES;
    }
}

static int
vertex_attr_size(pz_vertex_attr_type type)
{
    switch (type) {
    case PZ_ATTR_FLOAT:
        return 1;
    case PZ_ATTR_FLOAT2:
        return 2;
    case PZ_ATTR_FLOAT3:
        return 3;
    case PZ_ATTR_FLOAT4:
        return 4;
    case PZ_ATTR_UINT8_NORM:
        return 4;
    default:
        return 1;
    }
}

static GLenum
vertex_attr_gl_type(pz_vertex_attr_type type)
{
    switch (type) {
    case PZ_ATTR_FLOAT:
    case PZ_ATTR_FLOAT2:
    case PZ_ATTR_FLOAT3:
    case PZ_ATTR_FLOAT4:
        return GL_FLOAT;
    case PZ_ATTR_UINT8_NORM:
        return GL_UNSIGNED_BYTE;
    default:
        return GL_FLOAT;
    }
}

static GLboolean
vertex_attr_normalized(pz_vertex_attr_type type)
{
    return type == PZ_ATTR_UINT8_NORM ? GL_TRUE : GL_FALSE;
}

/* ============================================================================
 * Resource Allocation Helpers
 * ============================================================================
 */

static uint32_t
alloc_shader(gl33_backend_data *data)
{
    for (uint32_t i = 1; i < MAX_SHADERS; i++) {
        if (!data->shaders[i].used) {
            data->shaders[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static uint32_t
alloc_texture(gl33_backend_data *data)
{
    for (uint32_t i = 1; i < MAX_TEXTURES; i++) {
        if (!data->textures[i].used) {
            data->textures[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static uint32_t
alloc_buffer(gl33_backend_data *data)
{
    for (uint32_t i = 1; i < MAX_BUFFERS; i++) {
        if (!data->buffers[i].used) {
            data->buffers[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static uint32_t
alloc_pipeline(gl33_backend_data *data)
{
    for (uint32_t i = 1; i < MAX_PIPELINES; i++) {
        if (!data->pipelines[i].used) {
            data->pipelines[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static uint32_t
alloc_render_target(gl33_backend_data *data)
{
    for (uint32_t i = 1; i < MAX_RENDER_TARGETS; i++) {
        if (!data->render_targets[i].used) {
            data->render_targets[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

static bool
gl33_init(pz_renderer *r, const pz_renderer_config *config)
{
    (void)config;

    gl33_backend_data *data = pz_calloc(1, sizeof(gl33_backend_data));
    if (!data) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to allocate GL33 backend data");
        return false;
    }

    r->backend_data = data;

    // Set default OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Enable multisampling (MSAA) if available
    glEnable(GL_MULTISAMPLE);

    // Set viewport
    glViewport(0, 0, r->viewport_width, r->viewport_height);

    gl_check_error("gl33_init");

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "OpenGL 3.3 backend initialized");
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "  Vendor:   %s",
        glGetString(GL_VENDOR));
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "  Renderer: %s",
        glGetString(GL_RENDERER));
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "  Version:  %s",
        glGetString(GL_VERSION));

    return true;
}

static void
gl33_shutdown(pz_renderer *r)
{
    gl33_backend_data *data = r->backend_data;
    if (!data)
        return;

    // Clean up all resources
    for (uint32_t i = 0; i < MAX_SHADERS; i++) {
        if (data->shaders[i].used && data->shaders[i].program) {
            glDeleteProgram(data->shaders[i].program);
        }
    }

    for (uint32_t i = 0; i < MAX_TEXTURES; i++) {
        if (data->textures[i].used && data->textures[i].id) {
            glDeleteTextures(1, &data->textures[i].id);
        }
    }

    for (uint32_t i = 0; i < MAX_BUFFERS; i++) {
        if (data->buffers[i].used && data->buffers[i].id) {
            glDeleteBuffers(1, &data->buffers[i].id);
        }
    }

    for (uint32_t i = 0; i < MAX_PIPELINES; i++) {
        if (data->pipelines[i].used && data->pipelines[i].vao) {
            glDeleteVertexArrays(1, &data->pipelines[i].vao);
        }
    }

    for (uint32_t i = 0; i < MAX_RENDER_TARGETS; i++) {
        if (data->render_targets[i].used) {
            if (data->render_targets[i].fbo)
                glDeleteFramebuffers(1, &data->render_targets[i].fbo);
            if (data->render_targets[i].color_texture)
                glDeleteTextures(1, &data->render_targets[i].color_texture);
            if (data->render_targets[i].depth_rbo)
                glDeleteRenderbuffers(1, &data->render_targets[i].depth_rbo);
        }
    }

    pz_free(data);
    r->backend_data = NULL;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "OpenGL 3.3 backend shutdown");
}

/* ============================================================================
 * Viewport
 * ============================================================================
 */

static void
gl33_get_viewport(pz_renderer *r, int *width, int *height)
{
    *width = r->viewport_width;
    *height = r->viewport_height;
}

static void
gl33_set_viewport(pz_renderer *r, int width, int height)
{
    r->viewport_width = width;
    r->viewport_height = height;
    glViewport(0, 0, width, height);
}

/* ============================================================================
 * Shaders
 * ============================================================================
 */

static GLuint
compile_shader(GLenum type, const char *source, const char *name)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Shader compile error (%s): %s",
            name, info_log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static pz_shader_handle
gl33_create_shader(pz_renderer *r, const pz_shader_desc *desc)
{
    gl33_backend_data *data = r->backend_data;

    uint32_t handle = alloc_shader(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max shaders reached");
        return PZ_INVALID_HANDLE;
    }

    const char *name = desc->name ? desc->name : "unnamed";

    // Compile vertex shader
    GLuint vert = compile_shader(GL_VERTEX_SHADER, desc->vertex_source, name);
    if (!vert) {
        data->shaders[handle].used = false;
        return PZ_INVALID_HANDLE;
    }

    // Compile fragment shader
    GLuint frag
        = compile_shader(GL_FRAGMENT_SHADER, desc->fragment_source, name);
    if (!frag) {
        glDeleteShader(vert);
        data->shaders[handle].used = false;
        return PZ_INVALID_HANDLE;
    }

    // Link program
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, sizeof(info_log), NULL, info_log);
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Shader link error (%s): %s",
            name, info_log);
        glDeleteShader(vert);
        glDeleteShader(frag);
        glDeleteProgram(program);
        data->shaders[handle].used = false;
        return PZ_INVALID_HANDLE;
    }

    // Cleanup shader objects
    glDeleteShader(vert);
    glDeleteShader(frag);

    data->shaders[handle].program = program;

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Created shader '%s' (handle=%u)",
        name, handle);

    return handle;
}

static void
gl33_destroy_shader(pz_renderer *r, pz_shader_handle handle)
{
    gl33_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_SHADERS)
        return;
    if (!data->shaders[handle].used)
        return;

    glDeleteProgram(data->shaders[handle].program);
    data->shaders[handle].program = 0;
    data->shaders[handle].used = false;
}

/* ============================================================================
 * Textures
 * ============================================================================
 */

static pz_texture_handle
gl33_create_texture(pz_renderer *r, const pz_texture_desc *desc)
{
    gl33_backend_data *data = r->backend_data;

    uint32_t handle = alloc_texture(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max textures reached");
        return PZ_INVALID_HANDLE;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // Set filtering
    GLenum min_filter = gl_filter(desc->filter);
    GLenum mag_filter
        = desc->filter == PZ_FILTER_LINEAR_MIPMAP ? GL_LINEAR : min_filter;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

    // Set wrapping
    GLenum wrap = gl_wrap(desc->wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

    // Upload data
    GLenum internal_fmt = gl_texture_internal_format(desc->format);
    GLenum format = gl_texture_format(desc->format);
    GLenum type = desc->format == PZ_TEXTURE_DEPTH24 ? GL_UNSIGNED_INT
                                                     : GL_UNSIGNED_BYTE;

    glTexImage2D(GL_TEXTURE_2D, 0, internal_fmt, desc->width, desc->height, 0,
        format, type, desc->data);

    // Generate mipmaps if needed
    if (desc->filter == PZ_FILTER_LINEAR_MIPMAP && desc->data) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    if (!gl_check_error("create_texture")) {
        glDeleteTextures(1, &tex);
        data->textures[handle].used = false;
        return PZ_INVALID_HANDLE;
    }

    data->textures[handle].id = tex;
    data->textures[handle].width = desc->width;
    data->textures[handle].height = desc->height;
    data->textures[handle].format = desc->format;

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Created texture %dx%d (handle=%u)",
        desc->width, desc->height, handle);

    return handle;
}

static void
gl33_update_texture(pz_renderer *r, pz_texture_handle handle, int x, int y,
    int width, int height, const void *tex_data)
{
    gl33_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_TEXTURES)
        return;
    if (!data->textures[handle].used)
        return;

    gl_texture *tex = &data->textures[handle];
    GLenum format = gl_texture_format(tex->format);
    GLenum type = tex->format == PZ_TEXTURE_DEPTH24 ? GL_UNSIGNED_INT
                                                    : GL_UNSIGNED_BYTE;

    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, x, y, width, height, format, type, tex_data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void
gl33_destroy_texture(pz_renderer *r, pz_texture_handle handle)
{
    gl33_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_TEXTURES)
        return;
    if (!data->textures[handle].used)
        return;

    glDeleteTextures(1, &data->textures[handle].id);
    data->textures[handle].id = 0;
    data->textures[handle].used = false;
}

/* ============================================================================
 * Buffers
 * ============================================================================
 */

static pz_buffer_handle
gl33_create_buffer(pz_renderer *r, const pz_buffer_desc *desc)
{
    gl33_backend_data *data = r->backend_data;

    uint32_t handle = alloc_buffer(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max buffers reached");
        return PZ_INVALID_HANDLE;
    }

    GLuint buf;
    glGenBuffers(1, &buf);

    GLenum target = gl_buffer_target(desc->type);
    GLenum usage = gl_buffer_usage(desc->usage);

    glBindBuffer(target, buf);
    glBufferData(target, desc->size, desc->data, usage);
    glBindBuffer(target, 0);

    if (!gl_check_error("create_buffer")) {
        glDeleteBuffers(1, &buf);
        data->buffers[handle].used = false;
        return PZ_INVALID_HANDLE;
    }

    data->buffers[handle].id = buf;
    data->buffers[handle].type = desc->type;
    data->buffers[handle].usage = desc->usage;
    data->buffers[handle].size = desc->size;

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER,
        "Created buffer size=%zu (handle=%u)", desc->size, handle);

    return handle;
}

static void
gl33_update_buffer(pz_renderer *r, pz_buffer_handle handle, size_t offset,
    const void *buf_data, size_t size)
{
    gl33_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_BUFFERS)
        return;
    if (!data->buffers[handle].used)
        return;

    gl_buffer *buf = &data->buffers[handle];
    GLenum target = gl_buffer_target(buf->type);

    glBindBuffer(target, buf->id);
    glBufferSubData(target, offset, size, buf_data);
    glBindBuffer(target, 0);
}

static void
gl33_destroy_buffer(pz_renderer *r, pz_buffer_handle handle)
{
    gl33_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_BUFFERS)
        return;
    if (!data->buffers[handle].used)
        return;

    glDeleteBuffers(1, &data->buffers[handle].id);
    data->buffers[handle].id = 0;
    data->buffers[handle].used = false;
}

/* ============================================================================
 * Pipelines
 * ============================================================================
 */

static pz_pipeline_handle
gl33_create_pipeline(pz_renderer *r, const pz_pipeline_desc *desc)
{
    gl33_backend_data *data = r->backend_data;

    uint32_t handle = alloc_pipeline(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max pipelines reached");
        return PZ_INVALID_HANDLE;
    }

    // Create VAO for this pipeline
    GLuint vao;
    glGenVertexArrays(1, &vao);
    // Store pipeline config
    gl_pipeline *pipeline = &data->pipelines[handle];
    pipeline->vao = vao;
    pipeline->shader = desc->shader;
    pipeline->blend = desc->blend;
    pipeline->depth = desc->depth;
    pipeline->cull = desc->cull;
    pipeline->primitive = desc->primitive;

    // Copy vertex layout
    size_t attr_count = desc->vertex_layout.attr_count;
    if (attr_count > 8)
        attr_count = 8;
    pipeline->vertex_layout.stride = desc->vertex_layout.stride;
    pipeline->vertex_layout.attr_count = attr_count;
    pipeline->vertex_layout.attrs = pipeline->stored_attrs;

    for (size_t i = 0; i < attr_count; i++) {
        pipeline->stored_attrs[i] = desc->vertex_layout.attrs[i];
    }

    glBindVertexArray(0);

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Created pipeline (handle=%u)",
        handle);

    return handle;
}

static void
gl33_destroy_pipeline(pz_renderer *r, pz_pipeline_handle handle)
{
    gl33_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_PIPELINES)
        return;
    if (!data->pipelines[handle].used)
        return;

    glDeleteVertexArrays(1, &data->pipelines[handle].vao);
    data->pipelines[handle].vao = 0;
    data->pipelines[handle].used = false;
}

/* ============================================================================
 * Render Targets
 * ============================================================================
 */

static pz_render_target_handle
gl33_create_render_target(pz_renderer *r, const pz_render_target_desc *desc)
{
    gl33_backend_data *data = r->backend_data;

    uint32_t handle = alloc_render_target(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max render targets reached");
        return PZ_INVALID_HANDLE;
    }

    gl_render_target *rt = &data->render_targets[handle];

    // Create framebuffer
    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    // Create color texture
    glGenTextures(1, &rt->color_texture);
    glBindTexture(GL_TEXTURE_2D, rt->color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0,
        gl_texture_internal_format(desc->color_format), desc->width,
        desc->height, 0, gl_texture_format(desc->color_format),
        GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        rt->color_texture, 0);

    // Create depth renderbuffer if needed
    if (desc->has_depth) {
        glGenRenderbuffers(1, &rt->depth_rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rt->depth_rbo);
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, desc->width, desc->height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, rt->depth_rbo);
    }

    // Check completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Framebuffer incomplete: 0x%04x", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (rt->fbo)
            glDeleteFramebuffers(1, &rt->fbo);
        if (rt->color_texture)
            glDeleteTextures(1, &rt->color_texture);
        if (rt->depth_rbo)
            glDeleteRenderbuffers(1, &rt->depth_rbo);
        rt->used = false;
        return PZ_INVALID_HANDLE;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    rt->width = desc->width;
    rt->height = desc->height;
    rt->has_depth = desc->has_depth;

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER,
        "Created render target %dx%d (handle=%u)", desc->width, desc->height,
        handle);

    return handle;
}

static pz_texture_handle
gl33_get_render_target_texture(pz_renderer *r, pz_render_target_handle handle)
{
    gl33_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_RENDER_TARGETS)
        return PZ_INVALID_HANDLE;
    if (!data->render_targets[handle].used)
        return PZ_INVALID_HANDLE;

    // Find or create a texture handle for this render target's color texture
    // For simplicity, we'll allocate a texture slot and store the GL texture ID
    uint32_t tex_handle = alloc_texture(data);
    if (tex_handle == PZ_INVALID_HANDLE)
        return PZ_INVALID_HANDLE;

    gl_render_target *rt = &data->render_targets[handle];
    data->textures[tex_handle].id = rt->color_texture;
    data->textures[tex_handle].width = rt->width;
    data->textures[tex_handle].height = rt->height;
    data->textures[tex_handle].format = PZ_TEXTURE_RGBA8; // Assume RGBA

    return tex_handle;
}

static void
gl33_destroy_render_target(pz_renderer *r, pz_render_target_handle handle)
{
    gl33_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_RENDER_TARGETS)
        return;
    if (!data->render_targets[handle].used)
        return;

    gl_render_target *rt = &data->render_targets[handle];
    if (rt->fbo)
        glDeleteFramebuffers(1, &rt->fbo);
    if (rt->color_texture)
        glDeleteTextures(1, &rt->color_texture);
    if (rt->depth_rbo)
        glDeleteRenderbuffers(1, &rt->depth_rbo);

    memset(rt, 0, sizeof(*rt));
}

/* ============================================================================
 * Frame
 * ============================================================================
 */

static void
gl33_begin_frame(pz_renderer *r)
{
    (void)r;
    // Reset state at start of frame if needed
}

static void
gl33_end_frame(pz_renderer *r)
{
    (void)r;
    // Reset state at end of frame
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    // Swap is done externally via SDL
}

/* ============================================================================
 * Render Target Binding
 * ============================================================================
 */

static void
gl33_set_render_target(pz_renderer *r, pz_render_target_handle handle)
{
    gl33_backend_data *data = r->backend_data;

    if (handle == PZ_INVALID_HANDLE || handle == 0) {
        // Bind default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, r->viewport_width, r->viewport_height);
    } else if (handle < MAX_RENDER_TARGETS
        && data->render_targets[handle].used) {
        gl_render_target *rt = &data->render_targets[handle];
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        glViewport(0, 0, rt->width, rt->height);
    }
}

static void
gl33_clear(pz_renderer *r, float cr, float g, float b, float a, float depth)
{
    (void)r;
    // Ensure depth write is enabled for clear to work
    glDepthMask(GL_TRUE);
    glClearColor(cr, g, b, a);
    glClearDepth(depth);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void
gl33_clear_color(pz_renderer *r, float cr, float g, float b, float a)
{
    (void)r;
    glClearColor(cr, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void
gl33_clear_depth(pz_renderer *r, float depth)
{
    (void)r;
    // Ensure depth write is enabled for clear to work
    glDepthMask(GL_TRUE);
    glClearDepth(depth);
    glClear(GL_DEPTH_BUFFER_BIT);
}

/* ============================================================================
 * Uniforms
 * ============================================================================
 */

static GLint
get_uniform_location(
    gl33_backend_data *data, pz_shader_handle shader, const char *name)
{
    if (shader == PZ_INVALID_HANDLE || shader >= MAX_SHADERS)
        return -1;
    if (!data->shaders[shader].used)
        return -1;

    return glGetUniformLocation(data->shaders[shader].program, name);
}

static void
gl33_set_uniform_float(
    pz_renderer *r, pz_shader_handle shader, const char *name, float value)
{
    gl33_backend_data *data = r->backend_data;
    GLint loc = get_uniform_location(data, shader, name);
    if (loc >= 0) {
        glUseProgram(data->shaders[shader].program);
        glUniform1f(loc, value);
    }
}

static void
gl33_set_uniform_vec2(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec2 value)
{
    gl33_backend_data *data = r->backend_data;
    GLint loc = get_uniform_location(data, shader, name);
    if (loc >= 0) {
        glUseProgram(data->shaders[shader].program);
        glUniform2f(loc, value.x, value.y);
    }
}

static void
gl33_set_uniform_vec3(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec3 value)
{
    gl33_backend_data *data = r->backend_data;
    GLint loc = get_uniform_location(data, shader, name);
    if (loc >= 0) {
        glUseProgram(data->shaders[shader].program);
        glUniform3f(loc, value.x, value.y, value.z);
    }
}

static void
gl33_set_uniform_vec4(
    pz_renderer *r, pz_shader_handle shader, const char *name, pz_vec4 value)
{
    gl33_backend_data *data = r->backend_data;
    GLint loc = get_uniform_location(data, shader, name);
    if (loc >= 0) {
        glUseProgram(data->shaders[shader].program);
        glUniform4f(loc, value.x, value.y, value.z, value.w);
    }
}

static void
gl33_set_uniform_mat4(pz_renderer *r, pz_shader_handle shader, const char *name,
    const pz_mat4 *value)
{
    gl33_backend_data *data = r->backend_data;
    GLint loc = get_uniform_location(data, shader, name);
    if (loc >= 0) {
        glUseProgram(data->shaders[shader].program);
        glUniformMatrix4fv(loc, 1, GL_FALSE, value->m);
    }
}

static void
gl33_set_uniform_int(
    pz_renderer *r, pz_shader_handle shader, const char *name, int value)
{
    gl33_backend_data *data = r->backend_data;
    GLint loc = get_uniform_location(data, shader, name);
    if (loc >= 0) {
        glUseProgram(data->shaders[shader].program);
        glUniform1i(loc, value);
    }
}

/* ============================================================================
 * Texture Binding
 * ============================================================================
 */

static void
gl33_bind_texture(pz_renderer *r, int slot, pz_texture_handle handle)
{
    gl33_backend_data *data = r->backend_data;

    glActiveTexture(GL_TEXTURE0 + slot);

    if (handle == PZ_INVALID_HANDLE || handle >= MAX_TEXTURES
        || !data->textures[handle].used) {
        glBindTexture(GL_TEXTURE_2D, 0);
    } else {
        glBindTexture(GL_TEXTURE_2D, data->textures[handle].id);
    }
}

/* ============================================================================
 * Drawing
 * ============================================================================
 */

static void
apply_blend_mode(pz_blend_mode mode)
{
    switch (mode) {
    case PZ_BLEND_NONE:
        glDisable(GL_BLEND);
        break;
    case PZ_BLEND_ALPHA:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case PZ_BLEND_ADDITIVE:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    case PZ_BLEND_MULTIPLY:
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;
    }
}

static void
apply_depth_mode(pz_depth_mode mode)
{
    switch (mode) {
    case PZ_DEPTH_NONE:
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        break;
    case PZ_DEPTH_READ:
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        break;
    case PZ_DEPTH_WRITE:
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        break;
    case PZ_DEPTH_READ_WRITE:
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        break;
    }
}

static void
apply_cull_mode(pz_cull_mode mode)
{
    switch (mode) {
    case PZ_CULL_NONE:
        glDisable(GL_CULL_FACE);
        break;
    case PZ_CULL_BACK:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case PZ_CULL_FRONT:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    }
}

/* ============================================================================
 * Screenshot
 * ============================================================================
 */

static uint8_t *
gl33_screenshot(pz_renderer *r, int *out_width, int *out_height)
{
    int width = r->viewport_width;
    int height = r->viewport_height;

    // Allocate buffer for RGBA pixels
    size_t pixel_count = (size_t)width * (size_t)height * 4;
    uint8_t *pixels = pz_alloc(pixel_count);
    if (!pixels) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to allocate screenshot buffer");
        return NULL;
    }

    // Read pixels from framebuffer (OpenGL reads bottom-to-top)
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    if (!gl_check_error("screenshot")) {
        pz_free(pixels);
        return NULL;
    }

    // Flip the image vertically (OpenGL is bottom-up, images are top-down)
    size_t row_size = (size_t)width * 4;
    uint8_t *temp_row = pz_alloc(row_size);
    if (temp_row) {
        for (int y = 0; y < height / 2; y++) {
            uint8_t *top_row = pixels + (size_t)y * row_size;
            uint8_t *bottom_row = pixels + (size_t)(height - 1 - y) * row_size;
            memcpy(temp_row, top_row, row_size);
            memcpy(top_row, bottom_row, row_size);
            memcpy(bottom_row, temp_row, row_size);
        }
        pz_free(temp_row);
    }

    *out_width = width;
    *out_height = height;
    return pixels;
}

/* ============================================================================
 * Drawing
 * ============================================================================
 */

static void
gl33_draw(pz_renderer *r, const pz_draw_cmd *cmd)
{
    gl33_backend_data *data = r->backend_data;

    // Validate pipeline
    if (cmd->pipeline == PZ_INVALID_HANDLE || cmd->pipeline >= MAX_PIPELINES
        || !data->pipelines[cmd->pipeline].used) {
        pz_log(
            PZ_LOG_WARN, PZ_LOG_CAT_RENDER, "Invalid pipeline in draw command");
        return;
    }

    gl_pipeline *pipeline = &data->pipelines[cmd->pipeline];

    // Validate vertex buffer
    if (cmd->vertex_buffer == PZ_INVALID_HANDLE
        || cmd->vertex_buffer >= MAX_BUFFERS
        || !data->buffers[cmd->vertex_buffer].used) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
            "Invalid vertex buffer in draw command");
        return;
    }

    // Bind shader
    if (pipeline->shader != PZ_INVALID_HANDLE && pipeline->shader < MAX_SHADERS
        && data->shaders[pipeline->shader].used) {
        glUseProgram(data->shaders[pipeline->shader].program);
    }

    // Apply render state
    apply_blend_mode(pipeline->blend);
    apply_depth_mode(pipeline->depth);
    apply_cull_mode(pipeline->cull);

    // Bind VAO and VBO
    glBindVertexArray(pipeline->vao);
    glBindBuffer(GL_ARRAY_BUFFER, data->buffers[cmd->vertex_buffer].id);

    // Set up vertex attributes (must be done after VBO is bound)
    GLuint program = data->shaders[pipeline->shader].program;
    for (size_t i = 0; i < pipeline->vertex_layout.attr_count; i++) {
        const pz_vertex_attr *attr = &pipeline->vertex_layout.attrs[i];
        GLint loc = glGetAttribLocation(program, attr->name);
        if (loc >= 0) {
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, vertex_attr_size(attr->type),
                vertex_attr_gl_type(attr->type),
                vertex_attr_normalized(attr->type),
                (GLsizei)pipeline->vertex_layout.stride, (void *)attr->offset);
        }
    }

    GLenum primitive = gl_primitive(pipeline->primitive);

    if (cmd->index_buffer != PZ_INVALID_HANDLE
        && cmd->index_buffer < MAX_BUFFERS
        && data->buffers[cmd->index_buffer].used && cmd->index_count > 0) {
        // Indexed draw
        glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER, data->buffers[cmd->index_buffer].id);
        glDrawElements(primitive, (GLsizei)cmd->index_count, GL_UNSIGNED_INT,
            (void *)(cmd->index_offset * sizeof(uint32_t)));
    } else if (cmd->vertex_count > 0) {
        // Non-indexed draw
        glDrawArrays(
            primitive, (GLint)cmd->vertex_offset, (GLsizei)cmd->vertex_count);
    }

    glBindVertexArray(0);

    gl_check_error("draw");
}

/* ============================================================================
 * Vtable
 * ============================================================================
 */

static const pz_render_backend_vtable s_gl33_vtable = {
    .init = gl33_init,
    .shutdown = gl33_shutdown,
    .get_viewport = gl33_get_viewport,
    .set_viewport = gl33_set_viewport,
    .create_shader = gl33_create_shader,
    .destroy_shader = gl33_destroy_shader,
    .create_texture = gl33_create_texture,
    .update_texture = gl33_update_texture,
    .destroy_texture = gl33_destroy_texture,
    .create_buffer = gl33_create_buffer,
    .update_buffer = gl33_update_buffer,
    .destroy_buffer = gl33_destroy_buffer,
    .create_pipeline = gl33_create_pipeline,
    .destroy_pipeline = gl33_destroy_pipeline,
    .create_render_target = gl33_create_render_target,
    .get_render_target_texture = gl33_get_render_target_texture,
    .destroy_render_target = gl33_destroy_render_target,
    .begin_frame = gl33_begin_frame,
    .end_frame = gl33_end_frame,
    .set_render_target = gl33_set_render_target,
    .clear = gl33_clear,
    .clear_color = gl33_clear_color,
    .clear_depth = gl33_clear_depth,
    .set_uniform_float = gl33_set_uniform_float,
    .set_uniform_vec2 = gl33_set_uniform_vec2,
    .set_uniform_vec3 = gl33_set_uniform_vec3,
    .set_uniform_vec4 = gl33_set_uniform_vec4,
    .set_uniform_mat4 = gl33_set_uniform_mat4,
    .set_uniform_int = gl33_set_uniform_int,
    .bind_texture = gl33_bind_texture,
    .draw = gl33_draw,
    .screenshot = gl33_screenshot,
};

const pz_render_backend_vtable *
pz_render_backend_gl33_vtable(void)
{
    return &s_gl33_vtable;
}
