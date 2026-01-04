/*
 * Tank Game - Sokol GFX Renderer Backend
 */

#include "../../core/pz_log.h"
#include "../../core/pz_mem.h"
#include "../../core/pz_platform.h"
#include "pz_render_backend.h"

#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_gfx.h"
#include "third_party/sokol/sokol_glue.h"
#include "third_party/sokol/sokol_log.h"

#if defined(SOKOL_GLCORE)
#    ifdef __APPLE__
#        include <OpenGL/gl3.h>
#    else
#        include <GL/gl.h>
#    endif
#endif

#include <stdio.h>
#include <string.h>

#include "pz_sokol_shaders.h"

/* ============================================================================
 * Constants
 * ============================================================================
 */

#define MAX_SHADERS 64
#define MAX_TEXTURES 256
#define MAX_BUFFERS 256
#define MAX_PIPELINES 64
#define MAX_RENDER_TARGETS 32
#define MAX_SHADER_UNIFORMS 128

/* ============================================================================
 * Shader Metadata
 * ============================================================================
 */

typedef const sg_shader_desc *(*pz_sokol_shader_desc_fn)(sg_backend backend);

typedef struct shader_desc_entry {
    const char *name;
    pz_sokol_shader_desc_fn fn;
} shader_desc_entry;

static const shader_desc_entry SHADER_DESC_TABLE[] = {
    { "test", tankgame_test_shader_desc },
    { "textured", tankgame_textured_shader_desc },
    { "ground", tankgame_ground_shader_desc },
    { "water", tankgame_water_shader_desc },
    { "wall", tankgame_wall_shader_desc },
    { "entity", tankgame_entity_shader_desc },
    { "tank", tankgame_tank_shader_desc },
    { "projectile", tankgame_projectile_shader_desc },
    { "powerup", tankgame_powerup_shader_desc },
    { "track", tankgame_track_shader_desc },
    { "lightmap", tankgame_lightmap_shader_desc },
    { "particle", tankgame_particle_shader_desc },
    { "laser", tankgame_laser_shader_desc },
    { "debug_text", tankgame_debug_text_shader_desc },
    { "debug_line", tankgame_debug_line_shader_desc },
    { "debug_line_3d", tankgame_debug_line_3d_shader_desc },
    { "sdf_text", tankgame_sdf_text_shader_desc },
    { "background", tankgame_background_shader_desc },
};

typedef int (*pz_uniform_offset_fn)(const char *ub_name, const char *u_name);
typedef sg_glsl_shader_uniform (*pz_uniform_desc_fn)(
    const char *ub_name, const char *u_name);
typedef int (*pz_uniformblock_slot_fn)(const char *ub_name);
typedef size_t (*pz_uniformblock_size_fn)(const char *ub_name);

typedef struct shader_reflection {
    const char *name;
    pz_uniform_offset_fn uniform_offset;
    pz_uniform_desc_fn uniform_desc;
    pz_uniformblock_slot_fn uniformblock_slot;
    pz_uniformblock_size_fn uniformblock_size;
    const char **uniform_blocks;
    int uniform_block_count;
} shader_reflection;

static const char *SHADER_BLOCKS_TEST[] = { "test_vs_params" };
static const char *SHADER_BLOCKS_TEXTURED[] = { "textured_vs_params" };
static const char *SHADER_BLOCKS_GROUND[]
    = { "ground_vs_params", "ground_fs_params" };
static const char *SHADER_BLOCKS_WATER[]
    = { "water_vs_params", "water_fs_params" };
static const char *SHADER_BLOCKS_WALL[]
    = { "wall_vs_params", "wall_fs_params" };
static const char *SHADER_BLOCKS_ENTITY[]
    = { "entity_vs_params", "entity_fs_params" };
static const char *SHADER_BLOCKS_TRACK[] = { "track_fs_params" };
static const char *SHADER_BLOCKS_LIGHTMAP[] = { "lightmap_fs_params" };
static const char *SHADER_BLOCKS_PARTICLE[]
    = { "particle_vs_params", "particle_fs_params" };
static const char *SHADER_BLOCKS_LASER[]
    = { "laser_vs_params", "laser_fs_params" };
static const char *SHADER_BLOCKS_DEBUG_TEXT[] = { "debug_text_vs_params" };
static const char *SHADER_BLOCKS_DEBUG_LINE[] = { "debug_line_vs_params" };
static const char *SHADER_BLOCKS_DEBUG_LINE_3D[]
    = { "debug_line_3d_vs_params" };
static const char *SHADER_BLOCKS_SDF_TEXT[] = { "sdf_text_vs_params" };
static const char *SHADER_BLOCKS_BACKGROUND[] = { "background_fs_params" };

static const shader_reflection SHADER_REFLECTION_TABLE[] = {
    { "test", tankgame_test_uniform_offset, tankgame_test_uniform_desc,
        tankgame_test_uniformblock_slot, tankgame_test_uniformblock_size,
        SHADER_BLOCKS_TEST,
        (int)(sizeof(SHADER_BLOCKS_TEST) / sizeof(SHADER_BLOCKS_TEST[0])) },
    { "textured", tankgame_textured_uniform_offset,
        tankgame_textured_uniform_desc, tankgame_textured_uniformblock_slot,
        tankgame_textured_uniformblock_size, SHADER_BLOCKS_TEXTURED,
        (int)(sizeof(SHADER_BLOCKS_TEXTURED)
            / sizeof(SHADER_BLOCKS_TEXTURED[0])) },
    { "ground", tankgame_ground_uniform_offset, tankgame_ground_uniform_desc,
        tankgame_ground_uniformblock_slot, tankgame_ground_uniformblock_size,
        SHADER_BLOCKS_GROUND,
        (int)(sizeof(SHADER_BLOCKS_GROUND) / sizeof(SHADER_BLOCKS_GROUND[0])) },
    { "water", tankgame_water_uniform_offset, tankgame_water_uniform_desc,
        tankgame_water_uniformblock_slot, tankgame_water_uniformblock_size,
        SHADER_BLOCKS_WATER,
        (int)(sizeof(SHADER_BLOCKS_WATER) / sizeof(SHADER_BLOCKS_WATER[0])) },
    { "wall", tankgame_wall_uniform_offset, tankgame_wall_uniform_desc,
        tankgame_wall_uniformblock_slot, tankgame_wall_uniformblock_size,
        SHADER_BLOCKS_WALL,
        (int)(sizeof(SHADER_BLOCKS_WALL) / sizeof(SHADER_BLOCKS_WALL[0])) },
    { "entity", tankgame_entity_uniform_offset, tankgame_entity_uniform_desc,
        tankgame_entity_uniformblock_slot, tankgame_entity_uniformblock_size,
        SHADER_BLOCKS_ENTITY,
        (int)(sizeof(SHADER_BLOCKS_ENTITY) / sizeof(SHADER_BLOCKS_ENTITY[0])) },
    { "tank", tankgame_tank_uniform_offset, tankgame_tank_uniform_desc,
        tankgame_tank_uniformblock_slot, tankgame_tank_uniformblock_size,
        SHADER_BLOCKS_ENTITY,
        (int)(sizeof(SHADER_BLOCKS_ENTITY) / sizeof(SHADER_BLOCKS_ENTITY[0])) },
    { "projectile", tankgame_projectile_uniform_offset,
        tankgame_projectile_uniform_desc, tankgame_projectile_uniformblock_slot,
        tankgame_projectile_uniformblock_size, SHADER_BLOCKS_ENTITY,
        (int)(sizeof(SHADER_BLOCKS_ENTITY) / sizeof(SHADER_BLOCKS_ENTITY[0])) },
    { "powerup", tankgame_powerup_uniform_offset, tankgame_powerup_uniform_desc,
        tankgame_powerup_uniformblock_slot, tankgame_powerup_uniformblock_size,
        SHADER_BLOCKS_ENTITY,
        (int)(sizeof(SHADER_BLOCKS_ENTITY) / sizeof(SHADER_BLOCKS_ENTITY[0])) },
    { "track", tankgame_track_uniform_offset, tankgame_track_uniform_desc,
        tankgame_track_uniformblock_slot, tankgame_track_uniformblock_size,
        SHADER_BLOCKS_TRACK,
        (int)(sizeof(SHADER_BLOCKS_TRACK) / sizeof(SHADER_BLOCKS_TRACK[0])) },
    { "lightmap", tankgame_lightmap_uniform_offset,
        tankgame_lightmap_uniform_desc, tankgame_lightmap_uniformblock_slot,
        tankgame_lightmap_uniformblock_size, SHADER_BLOCKS_LIGHTMAP,
        (int)(sizeof(SHADER_BLOCKS_LIGHTMAP)
            / sizeof(SHADER_BLOCKS_LIGHTMAP[0])) },
    { "particle", tankgame_particle_uniform_offset,
        tankgame_particle_uniform_desc, tankgame_particle_uniformblock_slot,
        tankgame_particle_uniformblock_size, SHADER_BLOCKS_PARTICLE,
        (int)(sizeof(SHADER_BLOCKS_PARTICLE)
            / sizeof(SHADER_BLOCKS_PARTICLE[0])) },
    { "laser", tankgame_laser_uniform_offset, tankgame_laser_uniform_desc,
        tankgame_laser_uniformblock_slot, tankgame_laser_uniformblock_size,
        SHADER_BLOCKS_LASER,
        (int)(sizeof(SHADER_BLOCKS_LASER) / sizeof(SHADER_BLOCKS_LASER[0])) },
    { "debug_text", tankgame_debug_text_uniform_offset,
        tankgame_debug_text_uniform_desc, tankgame_debug_text_uniformblock_slot,
        tankgame_debug_text_uniformblock_size, SHADER_BLOCKS_DEBUG_TEXT,
        (int)(sizeof(SHADER_BLOCKS_DEBUG_TEXT)
            / sizeof(SHADER_BLOCKS_DEBUG_TEXT[0])) },
    { "debug_line", tankgame_debug_line_uniform_offset,
        tankgame_debug_line_uniform_desc, tankgame_debug_line_uniformblock_slot,
        tankgame_debug_line_uniformblock_size, SHADER_BLOCKS_DEBUG_LINE,
        (int)(sizeof(SHADER_BLOCKS_DEBUG_LINE)
            / sizeof(SHADER_BLOCKS_DEBUG_LINE[0])) },
    { "debug_line_3d", tankgame_debug_line_3d_uniform_offset,
        tankgame_debug_line_3d_uniform_desc,
        tankgame_debug_line_3d_uniformblock_slot,
        tankgame_debug_line_3d_uniformblock_size, SHADER_BLOCKS_DEBUG_LINE_3D,
        (int)(sizeof(SHADER_BLOCKS_DEBUG_LINE_3D)
            / sizeof(SHADER_BLOCKS_DEBUG_LINE_3D[0])) },
    { "sdf_text", tankgame_sdf_text_uniform_offset,
        tankgame_sdf_text_uniform_desc, tankgame_sdf_text_uniformblock_slot,
        tankgame_sdf_text_uniformblock_size, SHADER_BLOCKS_SDF_TEXT,
        (int)(sizeof(SHADER_BLOCKS_SDF_TEXT)
            / sizeof(SHADER_BLOCKS_SDF_TEXT[0])) },
    { "background", tankgame_background_uniform_offset,
        tankgame_background_uniform_desc, tankgame_background_uniformblock_slot,
        tankgame_background_uniformblock_size, SHADER_BLOCKS_BACKGROUND,
        (int)(sizeof(SHADER_BLOCKS_BACKGROUND)
            / sizeof(SHADER_BLOCKS_BACKGROUND[0])) },
};

static const sg_shader_desc *
find_shader_desc(const char *name)
{
    if (!name)
        return NULL;

    for (size_t i = 0;
         i < sizeof(SHADER_DESC_TABLE) / sizeof(SHADER_DESC_TABLE[0]); i++) {
        if (strcmp(SHADER_DESC_TABLE[i].name, name) == 0) {
            return SHADER_DESC_TABLE[i].fn(sg_query_backend());
        }
    }

    return NULL;
}

static const shader_reflection *
find_shader_reflection(const char *name)
{
    if (!name)
        return NULL;

    for (size_t i = 0; i
         < sizeof(SHADER_REFLECTION_TABLE) / sizeof(SHADER_REFLECTION_TABLE[0]);
         i++) {
        if (strcmp(SHADER_REFLECTION_TABLE[i].name, name) == 0) {
            return &SHADER_REFLECTION_TABLE[i];
        }
    }

    return NULL;
}

/* ============================================================================
 * Resource Structures
 * ============================================================================
 */

typedef struct sokol_uniform_ref {
    char name[64];
    int block_index;
    size_t offset;
    size_t size;
    sg_uniform_type type;
} sokol_uniform_ref;

typedef struct sokol_shader {
    sg_shader shader;
    bool used;
    int uniform_count;
    sokol_uniform_ref uniforms[MAX_SHADER_UNIFORMS];
    uint8_t *uniform_blocks[SG_MAX_UNIFORMBLOCK_BINDSLOTS];
    size_t uniform_block_sizes[SG_MAX_UNIFORMBLOCK_BINDSLOTS];
    int attr_count;
    const char *attr_names[SG_MAX_VERTEX_ATTRIBUTES];
    pz_uniform_offset_fn uniform_offset;
    pz_uniform_desc_fn uniform_desc;
    pz_uniformblock_slot_fn uniformblock_slot;
    pz_uniformblock_size_fn uniformblock_size;
    const char **uniform_block_names;
    int uniform_block_name_count;
} sokol_shader;

typedef struct sokol_texture {
    sg_image image;
    sg_view view;
    sg_sampler sampler;
    int width;
    int height;
    pz_texture_format format;
    bool mipmapped;
    bool owns_image;
    bool used;
} sokol_texture;

typedef struct sokol_buffer {
    sg_buffer buffer;
    pz_buffer_type type;
    size_t size;
    bool used;
} sokol_buffer;

typedef struct sokol_pipeline {
    sg_pipeline pipeline;
    pz_shader_handle shader;
    bool used;
} sokol_pipeline;

typedef struct sokol_render_target {
    sg_image color_image;
    sg_image depth_image;
    sg_view color_view;
    sg_view depth_view;
    sg_pass pass;
    int width;
    int height;
    bool has_depth;
    bool used;
} sokol_render_target;

typedef struct sokol_backend_data {
    sokol_shader shaders[MAX_SHADERS];
    sokol_texture textures[MAX_TEXTURES];
    sokol_buffer buffers[MAX_BUFFERS];
    sokol_pipeline pipelines[MAX_PIPELINES];
    sokol_render_target render_targets[MAX_RENDER_TARGETS];

    sg_bindings bindings;
    sg_pass_action pass_action;
    bool pass_active;
    pz_render_target_handle current_target;
    int sample_count;
} sokol_backend_data;

/* ============================================================================
 * Helpers
 * ============================================================================
 */

static size_t
align_up(size_t value, size_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static size_t
std140_alignment(sg_uniform_type type)
{
    switch (type) {
    case SG_UNIFORMTYPE_FLOAT:
    case SG_UNIFORMTYPE_INT:
        return 4;
    case SG_UNIFORMTYPE_FLOAT2:
    case SG_UNIFORMTYPE_INT2:
        return 8;
    case SG_UNIFORMTYPE_FLOAT3:
    case SG_UNIFORMTYPE_INT3:
        return 12;
    case SG_UNIFORMTYPE_FLOAT4:
    case SG_UNIFORMTYPE_INT4:
    case SG_UNIFORMTYPE_MAT4:
        return 16;
    default:
        return 4;
    }
}

static size_t
std140_size(sg_uniform_type type)
{
    switch (type) {
    case SG_UNIFORMTYPE_FLOAT:
    case SG_UNIFORMTYPE_INT:
        return 4;
    case SG_UNIFORMTYPE_FLOAT2:
    case SG_UNIFORMTYPE_INT2:
        return 8;
    case SG_UNIFORMTYPE_FLOAT3:
    case SG_UNIFORMTYPE_FLOAT4:
    case SG_UNIFORMTYPE_INT3:
    case SG_UNIFORMTYPE_INT4:
        return 16;
    case SG_UNIFORMTYPE_MAT4:
        return 64;
    default:
        return 4;
    }
}

static size_t
std140_array_stride(sg_uniform_type type)
{
    if (type == SG_UNIFORMTYPE_MAT4) {
        return 64;
    }
    return 16;
}

static int
alloc_shader(sokol_backend_data *data)
{
    for (int i = 1; i < MAX_SHADERS; i++) {
        if (!data->shaders[i].used) {
            data->shaders[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static int
alloc_texture(sokol_backend_data *data)
{
    for (int i = 1; i < MAX_TEXTURES; i++) {
        if (!data->textures[i].used) {
            data->textures[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static int
alloc_buffer(sokol_backend_data *data)
{
    for (int i = 1; i < MAX_BUFFERS; i++) {
        if (!data->buffers[i].used) {
            data->buffers[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static int
alloc_pipeline(sokol_backend_data *data)
{
    for (int i = 1; i < MAX_PIPELINES; i++) {
        if (!data->pipelines[i].used) {
            data->pipelines[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static int
alloc_render_target(sokol_backend_data *data)
{
    for (int i = 1; i < MAX_RENDER_TARGETS; i++) {
        if (!data->render_targets[i].used) {
            data->render_targets[i].used = true;
            return i;
        }
    }
    return PZ_INVALID_HANDLE;
}

static sg_pixel_format
pz_to_sg_format(pz_texture_format fmt)
{
    switch (fmt) {
    case PZ_TEXTURE_RGBA8:
        return SG_PIXELFORMAT_RGBA8;
    case PZ_TEXTURE_RGB8:
        return SG_PIXELFORMAT_RGBA8;
    case PZ_TEXTURE_R8:
        return SG_PIXELFORMAT_R8;
    case PZ_TEXTURE_DEPTH24:
        return SG_PIXELFORMAT_DEPTH;
    default:
        return SG_PIXELFORMAT_RGBA8;
    }
}

static size_t
calc_image_data_size(pz_texture_format fmt, int width, int height)
{
    sg_pixel_format sg_fmt = pz_to_sg_format(fmt);
    return (size_t)sg_query_surface_pitch(sg_fmt, width, height, 1);
}

static sg_vertex_format
pz_to_sg_vertex_format(pz_vertex_attr_type type)
{
    switch (type) {
    case PZ_ATTR_FLOAT:
        return SG_VERTEXFORMAT_FLOAT;
    case PZ_ATTR_FLOAT2:
        return SG_VERTEXFORMAT_FLOAT2;
    case PZ_ATTR_FLOAT3:
        return SG_VERTEXFORMAT_FLOAT3;
    case PZ_ATTR_FLOAT4:
        return SG_VERTEXFORMAT_FLOAT4;
    case PZ_ATTR_UINT8_NORM:
        return SG_VERTEXFORMAT_UBYTE4N;
    default:
        return SG_VERTEXFORMAT_INVALID;
    }
}

static sg_primitive_type
pz_to_sg_primitive(pz_primitive prim)
{
    switch (prim) {
    case PZ_PRIMITIVE_TRIANGLES:
        return SG_PRIMITIVETYPE_TRIANGLES;
    case PZ_PRIMITIVE_LINES:
        return SG_PRIMITIVETYPE_LINES;
    case PZ_PRIMITIVE_POINTS:
        return SG_PRIMITIVETYPE_POINTS;
    default:
        return SG_PRIMITIVETYPE_TRIANGLES;
    }
}

static sg_cull_mode
pz_to_sg_cull(pz_cull_mode mode)
{
    switch (mode) {
    case PZ_CULL_NONE:
        return SG_CULLMODE_NONE;
    case PZ_CULL_BACK:
        return SG_CULLMODE_BACK;
    case PZ_CULL_FRONT:
        return SG_CULLMODE_FRONT;
    default:
        return SG_CULLMODE_NONE;
    }
}

static void
init_pass_action(sg_pass_action *action)
{
    memset(action, 0, sizeof(*action));
    action->colors[0].load_action = SG_LOADACTION_LOAD;
    action->colors[0].store_action = SG_STOREACTION_STORE;
    action->depth.load_action = SG_LOADACTION_LOAD;
    action->depth.store_action = SG_STOREACTION_STORE;
}

static int
shader_attr_index(const sokol_shader *shader, const char *name)
{
    for (int i = 0; i < shader->attr_count; i++) {
        if (shader->attr_names[i] && strcmp(shader->attr_names[i], name) == 0)
            return i;
    }
    return -1;
}

static bool
add_uniform_ref(sokol_shader *shader, const char *name, int block_index,
    size_t offset, size_t size, sg_uniform_type type)
{
    if (shader->uniform_count >= MAX_SHADER_UNIFORMS)
        return false;

    sokol_uniform_ref *ref = &shader->uniforms[shader->uniform_count++];
    snprintf(ref->name, sizeof(ref->name), "%s", name);
    ref->block_index = block_index;
    ref->offset = offset;
    ref->size = size;
    ref->type = type;
    return true;
}

static sokol_uniform_ref *
resolve_uniform_ref(sokol_shader *shader, const char *name)
{
    if (!shader->uniform_offset || !shader->uniform_desc
        || !shader->uniformblock_slot || !shader->uniformblock_size
        || !shader->uniform_block_names) {
        return NULL;
    }

    for (int i = 0; i < shader->uniform_block_name_count; i++) {
        const char *block_name = shader->uniform_block_names[i];
        int offset = shader->uniform_offset(block_name, name);
        if (offset < 0) {
            continue;
        }

        int slot = shader->uniformblock_slot(block_name);
        size_t block_size = shader->uniformblock_size(block_name);
        if (slot < 0 || slot >= SG_MAX_UNIFORMBLOCK_BINDSLOTS) {
            continue;
        }

        if (!shader->uniform_blocks[slot] && block_size > 0) {
            shader->uniform_blocks[slot] = pz_calloc(1, block_size);
            shader->uniform_block_sizes[slot] = block_size;
        }

        sg_glsl_shader_uniform uniform = shader->uniform_desc(block_name, name);
        if (uniform.type == SG_UNIFORMTYPE_INVALID) {
            continue;
        }

        uint16_t array_count
            = uniform.array_count > 0 ? uniform.array_count : 1;
        size_t size = std140_size(uniform.type);
        if (array_count > 1) {
            size = std140_array_stride(uniform.type) * (size_t)array_count;
        }

        if ((size_t)offset + size > block_size) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
                "Uniform %s exceeds block %s size", name, block_name);
        }

        if (!add_uniform_ref(
                shader, name, slot, (size_t)offset, size, uniform.type)) {
            return NULL;
        }

        return &shader->uniforms[shader->uniform_count - 1];
    }

    return NULL;
}

static sokol_uniform_ref *
find_uniform_ref(sokol_shader *shader, const char *name)
{
    for (int i = 0; i < shader->uniform_count; i++) {
        if (strcmp(shader->uniforms[i].name, name) == 0)
            return &shader->uniforms[i];
    }
    return resolve_uniform_ref(shader, name);
}

static void
build_uniform_block_from_desc(
    sokol_shader *shader, const sg_shader_uniform_block *block, int block_index)
{
    size_t offset = 0;

    for (int i = 0; i < SG_MAX_UNIFORMBLOCK_MEMBERS; i++) {
        const sg_glsl_shader_uniform uniform = block->glsl_uniforms[i];
        if (uniform.type == SG_UNIFORMTYPE_INVALID) {
            break;
        }

        if (!uniform.glsl_name || uniform.glsl_name[0] == '\0') {
            continue;
        }

        size_t align = std140_alignment(uniform.type);
        offset = align_up(offset, align);

        uint16_t array_count
            = uniform.array_count > 0 ? uniform.array_count : 1;
        size_t size = std140_size(uniform.type);
        if (array_count > 1) {
            size = std140_array_stride(uniform.type) * (size_t)array_count;
        }

        if (!add_uniform_ref(shader, uniform.glsl_name, block_index, offset,
                size, uniform.type)) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
                "Shader uniform table overflow for %s", uniform.glsl_name);
            continue;
        }

        offset += size;
    }

    if (offset > block->size) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
            "Uniform block %d layout exceeds size (%zu > %u)", block_index,
            offset, block->size);
    }
}

static int
calc_mip_count(int width, int height)
{
    int levels = 1;
    int size = width > height ? width : height;
    while (size > 1) {
        size >>= 1;
        levels++;
    }
    return levels;
}

static int
pz_texture_channel_count(pz_texture_format fmt)
{
    switch (fmt) {
    case PZ_TEXTURE_R8:
        return 1;
    case PZ_TEXTURE_RGB8:
    case PZ_TEXTURE_RGBA8:
    default:
        return 4;
    }
}

static void
downsample_mip_level(const uint8_t *src, int src_width, int src_height,
    uint8_t *dst, int dst_width, int dst_height, int channels)
{
    for (int y = 0; y < dst_height; y++) {
        int src_y = y * 2;
        for (int x = 0; x < dst_width; x++) {
            int src_x = x * 2;
            for (int c = 0; c < channels; c++) {
                int sum = 0;
                int count = 0;
                for (int oy = 0; oy < 2; oy++) {
                    int sy = src_y + oy;
                    if (sy >= src_height)
                        sy = src_height - 1;
                    for (int ox = 0; ox < 2; ox++) {
                        int sx = src_x + ox;
                        if (sx >= src_width)
                            sx = src_width - 1;
                        sum += src[(sy * src_width + sx) * channels + c];
                        count++;
                    }
                }
                dst[(y * dst_width + x) * channels + c]
                    = (uint8_t)(sum / count);
            }
        }
    }
}

static bool
build_mip_chain(const void *src_data, int width, int height,
    pz_texture_format fmt, sg_image_data *out_data, uint8_t **out_buffer)
{
    if (!src_data || !out_data || !out_buffer)
        return false;

    int channels = pz_texture_channel_count(fmt);
    if (channels != 1 && channels != 4)
        return false;

    int mip_count = calc_mip_count(width, height);
    size_t total_size = 0;
    int mip_width = width;
    int mip_height = height;

    for (int level = 0; level < mip_count; level++) {
        total_size += (size_t)mip_width * (size_t)mip_height * (size_t)channels;
        mip_width = mip_width > 1 ? mip_width >> 1 : 1;
        mip_height = mip_height > 1 ? mip_height >> 1 : 1;
    }

    uint8_t *buffer = pz_alloc(total_size);
    if (!buffer)
        return false;

    memset(out_data, 0, sizeof(*out_data));

    uint8_t *dst = buffer;
    const uint8_t *src = (const uint8_t *)src_data;
    int level_width = width;
    int level_height = height;
    int prev_width = width;
    int prev_height = height;

    for (int level = 0; level < mip_count; level++) {
        size_t level_size
            = (size_t)level_width * (size_t)level_height * (size_t)channels;
        out_data->mip_levels[level].ptr = dst;
        out_data->mip_levels[level].size = level_size;

        if (level == 0) {
            memcpy(dst, src, level_size);
        } else {
            downsample_mip_level(src, prev_width, prev_height, dst, level_width,
                level_height, channels);
        }

        src = dst;
        dst += level_size;
        prev_width = level_width;
        prev_height = level_height;
        level_width = level_width > 1 ? level_width >> 1 : 1;
        level_height = level_height > 1 ? level_height >> 1 : 1;
    }

    *out_buffer = buffer;
    return true;
}

static void
begin_pass_if_needed(pz_renderer *r)
{
    sokol_backend_data *data = r->backend_data;
    if (data->pass_active)
        return;

    sg_pass pass = { 0 };
    if (data->current_target == 0
        || data->current_target == PZ_INVALID_HANDLE) {
        pass.swapchain = sglue_swapchain();
    } else if (data->current_target < MAX_RENDER_TARGETS
        && data->render_targets[data->current_target].used) {
        pass.attachments
            = data->render_targets[data->current_target].pass.attachments;
    }

    pass.action = data->pass_action;
    sg_begin_pass(&pass);

    if (data->current_target == 0
        || data->current_target == PZ_INVALID_HANDLE) {
        sg_apply_viewport(0, 0, r->viewport_width, r->viewport_height, true);
    } else {
        sokol_render_target *rt = &data->render_targets[data->current_target];
        sg_apply_viewport(0, 0, rt->width, rt->height, true);
    }

    data->pass_active = true;
    init_pass_action(&data->pass_action);
}

static void
end_pass_if_active(sokol_backend_data *data)
{
    if (data->pass_active) {
        sg_end_pass();
        data->pass_active = false;
    }
}

/* ============================================================================
 * Backend Interface
 * ============================================================================
 */

static bool
sokol_init(pz_renderer *r, const pz_renderer_config *config)
{
    (void)config;

    sokol_backend_data *data = pz_calloc(1, sizeof(sokol_backend_data));
    if (!data)
        return false;

    sg_setup(&(sg_desc) {
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    data->sample_count = sapp_sample_count();
    init_pass_action(&data->pass_action);
    data->current_target = 0;

    r->backend_data = data;
    return true;
}

static void
sokol_shutdown(pz_renderer *r)
{
    sokol_backend_data *data = r->backend_data;
    if (!data)
        return;

    for (int i = 1; i < MAX_SHADERS; i++) {
        if (data->shaders[i].used) {
            sg_destroy_shader(data->shaders[i].shader);
            for (int b = 0; b < SG_MAX_UNIFORMBLOCK_BINDSLOTS; b++) {
                pz_free(data->shaders[i].uniform_blocks[b]);
            }
        }
    }

    for (int i = 1; i < MAX_TEXTURES; i++) {
        if (data->textures[i].used) {
            sg_destroy_view(data->textures[i].view);
            sg_destroy_sampler(data->textures[i].sampler);
            if (data->textures[i].owns_image) {
                sg_destroy_image(data->textures[i].image);
            }
        }
    }

    for (int i = 1; i < MAX_BUFFERS; i++) {
        if (data->buffers[i].used) {
            sg_destroy_buffer(data->buffers[i].buffer);
        }
    }

    for (int i = 1; i < MAX_PIPELINES; i++) {
        if (data->pipelines[i].used) {
            sg_destroy_pipeline(data->pipelines[i].pipeline);
        }
    }

    for (int i = 1; i < MAX_RENDER_TARGETS; i++) {
        if (data->render_targets[i].used) {
            sg_destroy_view(data->render_targets[i].color_view);
            sg_destroy_image(data->render_targets[i].color_image);
            if (data->render_targets[i].has_depth) {
                sg_destroy_view(data->render_targets[i].depth_view);
                sg_destroy_image(data->render_targets[i].depth_image);
            }
        }
    }

    sg_shutdown();
    pz_free(data);
    r->backend_data = NULL;
}

static void
sokol_get_viewport(pz_renderer *r, int *width, int *height)
{
    if (width)
        *width = r->viewport_width;
    if (height)
        *height = r->viewport_height;
}

static void
sokol_set_viewport(pz_renderer *r, int width, int height)
{
    r->viewport_width = width;
    r->viewport_height = height;
}

static float
sokol_get_dpi_scale(pz_renderer *r)
{
    (void)r;
    return sapp_dpi_scale();
}

static pz_shader_handle
sokol_create_shader(pz_renderer *r, const pz_shader_desc *desc)
{
    sokol_backend_data *data = r->backend_data;
    const sg_shader_desc *shd_desc = find_shader_desc(desc->name);

    if (!shd_desc) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "No sokol shader descriptor for '%s'", desc->name);
        return PZ_INVALID_HANDLE;
    }

    int handle = alloc_shader(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max shaders reached");
        return PZ_INVALID_HANDLE;
    }

    sokol_shader *shader = &data->shaders[handle];
    memset(shader, 0, sizeof(*shader));
    shader->used = true;

    shader->attr_count = 0;
    for (int i = 0; i < SG_MAX_VERTEX_ATTRIBUTES; i++) {
        if (shd_desc->attrs[i].glsl_name) {
            shader->attr_names[i] = shd_desc->attrs[i].glsl_name;
            shader->attr_count = i + 1;
        }
    }

    const shader_reflection *reflection = find_shader_reflection(desc->name);
    if (reflection) {
        shader->uniform_offset = reflection->uniform_offset;
        shader->uniform_desc = reflection->uniform_desc;
        shader->uniformblock_slot = reflection->uniformblock_slot;
        shader->uniformblock_size = reflection->uniformblock_size;
        shader->uniform_block_names = reflection->uniform_blocks;
        shader->uniform_block_name_count = reflection->uniform_block_count;

        for (int i = 0; i < reflection->uniform_block_count; i++) {
            const char *block_name = reflection->uniform_blocks[i];
            int slot = reflection->uniformblock_slot(block_name);
            size_t size = reflection->uniformblock_size(block_name);
            if (slot < 0 || slot >= SG_MAX_UNIFORMBLOCK_BINDSLOTS
                || size == 0) {
                continue;
            }
            shader->uniform_blocks[slot] = pz_calloc(1, size);
            shader->uniform_block_sizes[slot] = size;
        }
    } else {
        for (int i = 0; i < SG_MAX_UNIFORMBLOCK_BINDSLOTS; i++) {
            const sg_shader_uniform_block *block = &shd_desc->uniform_blocks[i];
            if (block->stage == SG_SHADERSTAGE_NONE || block->size == 0) {
                continue;
            }
            if (block->layout != SG_UNIFORMLAYOUT_STD140) {
                pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
                    "Shader '%s' uses non-std140 uniform layout", desc->name);
            }
            shader->uniform_block_sizes[i] = block->size;
            shader->uniform_blocks[i] = pz_calloc(1, block->size);
            build_uniform_block_from_desc(shader, block, i);
        }
    }

    shader->shader = sg_make_shader(shd_desc);

    if (!shader->shader.id) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to create sokol shader '%s'", desc->name);
        for (int i = 0; i < SG_MAX_UNIFORMBLOCK_BINDSLOTS; i++) {
            pz_free(shader->uniform_blocks[i]);
            shader->uniform_blocks[i] = NULL;
            shader->uniform_block_sizes[i] = 0;
        }
        shader->used = false;
        return PZ_INVALID_HANDLE;
    }

    return handle;
}

static void
sokol_destroy_shader(pz_renderer *r, pz_shader_handle handle)
{
    sokol_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_SHADERS)
        return;
    if (!data->shaders[handle].used)
        return;

    sg_destroy_shader(data->shaders[handle].shader);
    for (int i = 0; i < SG_MAX_UNIFORMBLOCK_BINDSLOTS; i++) {
        pz_free(data->shaders[handle].uniform_blocks[i]);
        data->shaders[handle].uniform_blocks[i] = NULL;
        data->shaders[handle].uniform_block_sizes[i] = 0;
    }
    data->shaders[handle].used = false;
}

static pz_texture_handle
sokol_create_texture(pz_renderer *r, const pz_texture_desc *desc)
{
    sokol_backend_data *data = r->backend_data;

    int handle = alloc_texture(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max textures reached");
        return PZ_INVALID_HANDLE;
    }

    sokol_texture *tex = &data->textures[handle];
    memset(tex, 0, sizeof(*tex));
    tex->used = true;
    tex->width = desc->width;
    tex->height = desc->height;
    tex->format = desc->format;
    tex->mipmapped = desc->filter == PZ_FILTER_LINEAR_MIPMAP;
    tex->owns_image = true;

    sg_image_desc img_desc;
    memset(&img_desc, 0, sizeof(img_desc));
    img_desc.width = desc->width;
    img_desc.height = desc->height;
    img_desc.pixel_format = pz_to_sg_format(desc->format);
    img_desc.usage.dynamic_update = true;
    img_desc.usage.immutable = false;
    if (tex->mipmapped) {
        img_desc.num_mipmaps = calc_mip_count(desc->width, desc->height);
    }

    tex->image = sg_make_image(&img_desc);
    if (!tex->image.id) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to create texture");
        tex->used = false;
        return PZ_INVALID_HANDLE;
    }

    tex->view = sg_make_view(&(sg_view_desc) {
        .texture = { .image = tex->image },
    });

    sg_sampler_desc sampler_desc;
    memset(&sampler_desc, 0, sizeof(sampler_desc));

    if (desc->filter == PZ_FILTER_NEAREST) {
        sampler_desc.min_filter = SG_FILTER_NEAREST;
        sampler_desc.mag_filter = SG_FILTER_NEAREST;
        sampler_desc.mipmap_filter = SG_FILTER_NEAREST;
    } else if (desc->filter == PZ_FILTER_LINEAR_MIPMAP) {
        sampler_desc.min_filter = SG_FILTER_LINEAR;
        sampler_desc.mag_filter = SG_FILTER_LINEAR;
        sampler_desc.mipmap_filter = SG_FILTER_LINEAR;
    } else {
        sampler_desc.min_filter = SG_FILTER_LINEAR;
        sampler_desc.mag_filter = SG_FILTER_LINEAR;
        sampler_desc.mipmap_filter = SG_FILTER_NEAREST;
    }

    switch (desc->wrap) {
    case PZ_WRAP_REPEAT:
        sampler_desc.wrap_u = SG_WRAP_REPEAT;
        sampler_desc.wrap_v = SG_WRAP_REPEAT;
        break;
    case PZ_WRAP_MIRROR:
        sampler_desc.wrap_u = SG_WRAP_MIRRORED_REPEAT;
        sampler_desc.wrap_v = SG_WRAP_MIRRORED_REPEAT;
        break;
    case PZ_WRAP_CLAMP:
    default:
        sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        break;
    }

    tex->sampler = sg_make_sampler(&sampler_desc);
    if (desc->data) {
        sg_image_data img_data = { 0 };
        uint8_t *mip_buffer = NULL;
        if (tex->mipmapped
            && build_mip_chain(desc->data, desc->width, desc->height,
                desc->format, &img_data, &mip_buffer)) {
            sg_update_image(tex->image, &img_data);
            pz_free(mip_buffer);
        } else {
            img_data.mip_levels[0].ptr = desc->data;
            img_data.mip_levels[0].size
                = calc_image_data_size(desc->format, desc->width, desc->height);
            sg_update_image(tex->image, &img_data);
        }
    }

    return handle;
}

static void
sokol_update_texture(pz_renderer *r, pz_texture_handle handle, int x, int y,
    int width, int height, const void *tex_data)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;

    sokol_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_TEXTURES)
        return;
    if (!data->textures[handle].used)
        return;

    sokol_texture *tex = &data->textures[handle];
    sg_image_data img_data = { 0 };
    uint8_t *mip_buffer = NULL;
    if (tex->mipmapped
        && build_mip_chain(tex_data, tex->width, tex->height, tex->format,
            &img_data, &mip_buffer)) {
        sg_update_image(tex->image, &img_data);
        pz_free(mip_buffer);
    } else {
        img_data.mip_levels[0].ptr = tex_data;
        img_data.mip_levels[0].size
            = calc_image_data_size(tex->format, tex->width, tex->height);
        sg_update_image(tex->image, &img_data);
    }
}

static void
sokol_destroy_texture(pz_renderer *r, pz_texture_handle handle)
{
    sokol_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_TEXTURES)
        return;
    if (!data->textures[handle].used)
        return;

    sg_destroy_view(data->textures[handle].view);
    sg_destroy_sampler(data->textures[handle].sampler);
    if (data->textures[handle].owns_image) {
        sg_destroy_image(data->textures[handle].image);
    }
    data->textures[handle].used = false;
}

static pz_buffer_handle
sokol_create_buffer(pz_renderer *r, const pz_buffer_desc *desc)
{
    sokol_backend_data *data = r->backend_data;

    int handle = alloc_buffer(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max buffers reached");
        return PZ_INVALID_HANDLE;
    }

    sokol_buffer *buf = &data->buffers[handle];
    memset(buf, 0, sizeof(*buf));
    buf->used = true;
    buf->type = desc->type;
    buf->size = desc->size;

    sg_buffer_desc buf_desc;
    memset(&buf_desc, 0, sizeof(buf_desc));
    buf_desc.size = desc->size;

    if (desc->usage == PZ_BUFFER_STATIC) {
        buf_desc.usage.immutable = true;
    } else if (desc->usage == PZ_BUFFER_DYNAMIC) {
        buf_desc.usage.dynamic_update = true;
        buf_desc.usage.immutable = false;
    } else {
        buf_desc.usage.stream_update = true;
        buf_desc.usage.immutable = false;
    }

    if (desc->type == PZ_BUFFER_INDEX) {
        buf_desc.usage.index_buffer = true;
        buf_desc.usage.vertex_buffer = false;
    } else {
        buf_desc.usage.vertex_buffer = true;
    }

    if (desc->data) {
        buf_desc.data.ptr = desc->data;
        buf_desc.data.size = desc->size;
    }

    buf->buffer = sg_make_buffer(&buf_desc);
    if (!buf->buffer.id) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to create buffer");
        buf->used = false;
        return PZ_INVALID_HANDLE;
    }

    return handle;
}

static void
sokol_update_buffer(pz_renderer *r, pz_buffer_handle handle, size_t offset,
    const void *buf_data, size_t size)
{
    sokol_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_BUFFERS)
        return;
    if (!data->buffers[handle].used)
        return;

    if (offset != 0) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
            "Sokol backend ignores non-zero buffer offsets");
    }

    sg_update_buffer(data->buffers[handle].buffer,
        &(sg_range) {
            .ptr = buf_data,
            .size = size,
        });
}

static void
sokol_destroy_buffer(pz_renderer *r, pz_buffer_handle handle)
{
    sokol_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_BUFFERS)
        return;
    if (!data->buffers[handle].used)
        return;

    sg_destroy_buffer(data->buffers[handle].buffer);
    data->buffers[handle].used = false;
}

static pz_pipeline_handle
sokol_create_pipeline(pz_renderer *r, const pz_pipeline_desc *desc)
{
    sokol_backend_data *data = r->backend_data;

    int handle = alloc_pipeline(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max pipelines reached");
        return PZ_INVALID_HANDLE;
    }

    if (desc->shader == PZ_INVALID_HANDLE || desc->shader >= MAX_SHADERS
        || !data->shaders[desc->shader].used) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Invalid shader handle for pipeline");
        return PZ_INVALID_HANDLE;
    }

    sokol_pipeline *pip = &data->pipelines[handle];
    memset(pip, 0, sizeof(*pip));
    pip->used = true;
    pip->shader = desc->shader;

    sg_pipeline_desc pip_desc;
    memset(&pip_desc, 0, sizeof(pip_desc));
    pip_desc.shader = data->shaders[desc->shader].shader;
    pip_desc.primitive_type = pz_to_sg_primitive(desc->primitive);
    pip_desc.cull_mode = pz_to_sg_cull(desc->cull);
    pip_desc.face_winding = SG_FACEWINDING_CCW;
    if (desc->sample_count > 0) {
        pip_desc.sample_count = desc->sample_count;
    } else {
        pip_desc.sample_count = data->sample_count > 0 ? data->sample_count : 1;
    }

    pip_desc.layout.buffers[0].stride = (int)desc->vertex_layout.stride;

    for (size_t i = 0; i < desc->vertex_layout.attr_count; i++) {
        const pz_vertex_attr *attr = &desc->vertex_layout.attrs[i];
        int attr_index
            = shader_attr_index(&data->shaders[desc->shader], attr->name);
        if (attr_index < 0 || attr_index >= SG_MAX_VERTEX_ATTRIBUTES) {
            pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
                "Shader missing attribute '%s'", attr->name);
            continue;
        }
        pip_desc.layout.attrs[attr_index].format
            = pz_to_sg_vertex_format(attr->type);
        pip_desc.layout.attrs[attr_index].offset = (int)attr->offset;
        pip_desc.layout.attrs[attr_index].buffer_index = 0;
    }

    switch (desc->depth) {
    case PZ_DEPTH_NONE:
        pip_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pip_desc.depth.write_enabled = false;
        break;
    case PZ_DEPTH_READ:
        pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pip_desc.depth.write_enabled = false;
        break;
    case PZ_DEPTH_WRITE:
        pip_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pip_desc.depth.write_enabled = true;
        break;
    case PZ_DEPTH_READ_WRITE:
        pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pip_desc.depth.write_enabled = true;
        break;
    }

    if (desc->blend != PZ_BLEND_NONE) {
        pip_desc.colors[0].blend.enabled = true;
        switch (desc->blend) {
        case PZ_BLEND_ALPHA:
            pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
            pip_desc.colors[0].blend.dst_factor_rgb
                = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
            pip_desc.colors[0].blend.dst_factor_alpha
                = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        case PZ_BLEND_ADDITIVE:
            pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
            pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
            pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
            pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE;
            break;
        case PZ_BLEND_MULTIPLY:
            pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_DST_COLOR;
            pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ZERO;
            pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
            pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ZERO;
            break;
        default:
            break;
        }
    }

    pip->pipeline = sg_make_pipeline(&pip_desc);
    if (!pip->pipeline.id) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to create pipeline");
        pip->used = false;
        return PZ_INVALID_HANDLE;
    }

    return handle;
}

static void
sokol_destroy_pipeline(pz_renderer *r, pz_pipeline_handle handle)
{
    sokol_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_PIPELINES)
        return;
    if (!data->pipelines[handle].used)
        return;

    sg_destroy_pipeline(data->pipelines[handle].pipeline);
    data->pipelines[handle].used = false;
}

static pz_render_target_handle
sokol_create_render_target(pz_renderer *r, const pz_render_target_desc *desc)
{
    sokol_backend_data *data = r->backend_data;

    int handle = alloc_render_target(data);
    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Max render targets reached");
        return PZ_INVALID_HANDLE;
    }

    sokol_render_target *rt = &data->render_targets[handle];
    memset(rt, 0, sizeof(*rt));
    rt->used = true;
    rt->width = desc->width;
    rt->height = desc->height;
    rt->has_depth = desc->has_depth;

    sg_image_desc color_desc = { 0 };
    color_desc.usage.color_attachment = true;
    color_desc.usage.immutable = true;
    color_desc.width = desc->width;
    color_desc.height = desc->height;
    color_desc.pixel_format = pz_to_sg_format(desc->color_format);
    color_desc.sample_count = 1;

    rt->color_image = sg_make_image(&color_desc);
    rt->color_view = sg_make_view(&(sg_view_desc) {
        .color_attachment = { .image = rt->color_image },
    });

    if (desc->has_depth) {
        sg_image_desc depth_desc = { 0 };
        depth_desc.usage.depth_stencil_attachment = true;
        depth_desc.usage.immutable = true;
        depth_desc.width = desc->width;
        depth_desc.height = desc->height;
        depth_desc.pixel_format
            = sg_query_desc().environment.defaults.depth_format;
        depth_desc.sample_count = 1;
        rt->depth_image = sg_make_image(&depth_desc);
        rt->depth_view = sg_make_view(&(sg_view_desc) {
            .depth_stencil_attachment = { .image = rt->depth_image },
        });
    }

    memset(&rt->pass, 0, sizeof(rt->pass));
    rt->pass.attachments.colors[0] = rt->color_view;
    if (desc->has_depth) {
        rt->pass.attachments.depth_stencil = rt->depth_view;
    }

    return handle;
}

static pz_texture_handle
sokol_get_render_target_texture(pz_renderer *r, pz_render_target_handle handle)
{
    sokol_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_RENDER_TARGETS)
        return PZ_INVALID_HANDLE;
    if (!data->render_targets[handle].used)
        return PZ_INVALID_HANDLE;

    int tex_handle = alloc_texture(data);
    if (tex_handle == PZ_INVALID_HANDLE)
        return PZ_INVALID_HANDLE;

    sokol_texture *tex = &data->textures[tex_handle];
    memset(tex, 0, sizeof(*tex));
    tex->used = true;

    sokol_render_target *rt = &data->render_targets[handle];
    tex->image = rt->color_image;
    tex->view = sg_make_view(&(sg_view_desc) {
        .texture = { .image = tex->image },
    });

    tex->width = rt->width;
    tex->height = rt->height;
    tex->format = PZ_TEXTURE_RGBA8;
    tex->mipmapped = false;
    tex->owns_image = false;

    tex->sampler = sg_make_sampler(&(sg_sampler_desc) {
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .mipmap_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

    return tex_handle;
}

static void
sokol_destroy_render_target(pz_renderer *r, pz_render_target_handle handle)
{
    sokol_backend_data *data = r->backend_data;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_RENDER_TARGETS)
        return;
    if (!data->render_targets[handle].used)
        return;

    sg_destroy_view(data->render_targets[handle].color_view);
    sg_destroy_image(data->render_targets[handle].color_image);
    if (data->render_targets[handle].has_depth) {
        sg_destroy_view(data->render_targets[handle].depth_view);
        sg_destroy_image(data->render_targets[handle].depth_image);
    }
    data->render_targets[handle].used = false;
}

static void
sokol_begin_frame(pz_renderer *r)
{
    sokol_backend_data *data = r->backend_data;
    data->current_target = 0;
    data->pass_active = false;
    init_pass_action(&data->pass_action);
}

static void
sokol_end_frame(pz_renderer *r)
{
    sokol_backend_data *data = r->backend_data;
    end_pass_if_active(data);
    sg_commit();
}

static void
sokol_set_render_target(pz_renderer *r, pz_render_target_handle handle)
{
    sokol_backend_data *data = r->backend_data;
    end_pass_if_active(data);
    data->current_target = handle == PZ_INVALID_HANDLE ? 0 : handle;
    init_pass_action(&data->pass_action);
}

static void
sokol_clear(pz_renderer *r, float cr, float g, float b, float a, float depth)
{
    sokol_backend_data *data = r->backend_data;
    end_pass_if_active(data);

    data->pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    data->pass_action.colors[0].clear_value = (sg_color) { cr, g, b, a };
    data->pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    data->pass_action.depth.clear_value = depth;

    begin_pass_if_needed(r);
}

static void
sokol_clear_color(pz_renderer *r, float cr, float g, float b, float a)
{
    sokol_backend_data *data = r->backend_data;
    end_pass_if_active(data);

    data->pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    data->pass_action.colors[0].clear_value = (sg_color) { cr, g, b, a };

    begin_pass_if_needed(r);
}

static void
sokol_clear_depth(pz_renderer *r, float depth)
{
    sokol_backend_data *data = r->backend_data;
    end_pass_if_active(data);

    data->pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    data->pass_action.depth.clear_value = depth;

    begin_pass_if_needed(r);
}

static void
sokol_set_uniform_float(pz_renderer *r, pz_shader_handle shader_handle,
    const char *name, float value)
{
    sokol_backend_data *data = r->backend_data;
    if (shader_handle == PZ_INVALID_HANDLE || shader_handle >= MAX_SHADERS)
        return;
    if (!data->shaders[shader_handle].used)
        return;

    sokol_shader *shader = &data->shaders[shader_handle];
    sokol_uniform_ref *ref = find_uniform_ref(shader, name);
    if (!ref)
        return;

    memcpy(shader->uniform_blocks[ref->block_index] + ref->offset, &value,
        sizeof(float));
}

static void
sokol_set_uniform_vec2(pz_renderer *r, pz_shader_handle shader_handle,
    const char *name, pz_vec2 value)
{
    sokol_backend_data *data = r->backend_data;
    if (shader_handle == PZ_INVALID_HANDLE || shader_handle >= MAX_SHADERS)
        return;
    if (!data->shaders[shader_handle].used)
        return;

    sokol_shader *shader = &data->shaders[shader_handle];
    sokol_uniform_ref *ref = find_uniform_ref(shader, name);
    if (!ref)
        return;

    float v[2] = { value.x, value.y };
    memcpy(
        shader->uniform_blocks[ref->block_index] + ref->offset, v, sizeof(v));
}

static void
sokol_set_uniform_vec3(pz_renderer *r, pz_shader_handle shader_handle,
    const char *name, pz_vec3 value)
{
    sokol_backend_data *data = r->backend_data;
    if (shader_handle == PZ_INVALID_HANDLE || shader_handle >= MAX_SHADERS)
        return;
    if (!data->shaders[shader_handle].used)
        return;

    sokol_shader *shader = &data->shaders[shader_handle];
    sokol_uniform_ref *ref = find_uniform_ref(shader, name);
    if (!ref)
        return;

    float v[4] = { value.x, value.y, value.z, 0.0f };
    memcpy(
        shader->uniform_blocks[ref->block_index] + ref->offset, v, sizeof(v));
}

static void
sokol_set_uniform_vec4(pz_renderer *r, pz_shader_handle shader_handle,
    const char *name, pz_vec4 value)
{
    sokol_backend_data *data = r->backend_data;
    if (shader_handle == PZ_INVALID_HANDLE || shader_handle >= MAX_SHADERS)
        return;
    if (!data->shaders[shader_handle].used)
        return;

    sokol_shader *shader = &data->shaders[shader_handle];
    sokol_uniform_ref *ref = find_uniform_ref(shader, name);
    if (!ref)
        return;

    float v[4] = { value.x, value.y, value.z, value.w };
    memcpy(
        shader->uniform_blocks[ref->block_index] + ref->offset, v, sizeof(v));
}

static void
sokol_set_uniform_mat4(pz_renderer *r, pz_shader_handle shader_handle,
    const char *name, const pz_mat4 *value)
{
    sokol_backend_data *data = r->backend_data;
    if (shader_handle == PZ_INVALID_HANDLE || shader_handle >= MAX_SHADERS)
        return;
    if (!data->shaders[shader_handle].used)
        return;

    sokol_shader *shader = &data->shaders[shader_handle];
    sokol_uniform_ref *ref = find_uniform_ref(shader, name);
    if (!ref)
        return;

    memcpy(shader->uniform_blocks[ref->block_index] + ref->offset, value->m,
        sizeof(float) * 16);
}

static void
sokol_set_uniform_int(
    pz_renderer *r, pz_shader_handle shader_handle, const char *name, int value)
{
    sokol_backend_data *data = r->backend_data;
    if (shader_handle == PZ_INVALID_HANDLE || shader_handle >= MAX_SHADERS)
        return;
    if (!data->shaders[shader_handle].used)
        return;

    sokol_shader *shader = &data->shaders[shader_handle];
    sokol_uniform_ref *ref = find_uniform_ref(shader, name);
    if (!ref)
        return;

    memcpy(shader->uniform_blocks[ref->block_index] + ref->offset, &value,
        sizeof(int));
}

static void
sokol_bind_texture(pz_renderer *r, int slot, pz_texture_handle handle)
{
    sokol_backend_data *data = r->backend_data;
    if (slot < 0 || slot >= SG_MAX_VIEW_BINDSLOTS
        || slot >= SG_MAX_SAMPLER_BINDSLOTS) {
        return;
    }

    if (handle == PZ_INVALID_HANDLE || handle >= MAX_TEXTURES
        || !data->textures[handle].used) {
        data->bindings.views[slot] = (sg_view) { 0 };
        data->bindings.samplers[slot] = (sg_sampler) { 0 };
        return;
    }

    data->bindings.views[slot] = data->textures[handle].view;
    data->bindings.samplers[slot] = data->textures[handle].sampler;
}

static void
sokol_draw(pz_renderer *r, const pz_draw_cmd *cmd)
{
    sokol_backend_data *data = r->backend_data;
    if (!cmd)
        return;

    if (cmd->pipeline == PZ_INVALID_HANDLE || cmd->pipeline >= MAX_PIPELINES)
        return;
    if (!data->pipelines[cmd->pipeline].used)
        return;

    begin_pass_if_needed(r);

    sokol_pipeline *pipeline = &data->pipelines[cmd->pipeline];
    sg_apply_pipeline(pipeline->pipeline);

    sg_bindings bindings = data->bindings;

    if (cmd->vertex_buffer != PZ_INVALID_HANDLE
        && cmd->vertex_buffer < MAX_BUFFERS
        && data->buffers[cmd->vertex_buffer].used) {
        bindings.vertex_buffers[0] = data->buffers[cmd->vertex_buffer].buffer;
        bindings.vertex_buffer_offsets[0] = 0;
    }

    if (cmd->index_buffer != PZ_INVALID_HANDLE
        && cmd->index_buffer < MAX_BUFFERS
        && data->buffers[cmd->index_buffer].used && cmd->index_count > 0) {
        bindings.index_buffer = data->buffers[cmd->index_buffer].buffer;
        bindings.index_buffer_offset = 0;
    }

    sg_apply_bindings(&bindings);

    sokol_shader *shader = &data->shaders[pipeline->shader];
    for (int i = 0; i < SG_MAX_UNIFORMBLOCK_BINDSLOTS; i++) {
        if (shader->uniform_blocks[i] && shader->uniform_block_sizes[i] > 0) {
            sg_apply_uniforms(i,
                &(sg_range) {
                    .ptr = shader->uniform_blocks[i],
                    .size = shader->uniform_block_sizes[i],
                });
        }
    }

    int base_element = 0;
    int num_elements = 0;
    if (cmd->index_count > 0) {
        base_element = (int)cmd->index_offset;
        num_elements = (int)cmd->index_count;
    } else {
        base_element = (int)cmd->vertex_offset;
        num_elements = (int)cmd->vertex_count;
    }

    sg_draw(base_element, num_elements, 1);
}

static uint8_t *
read_pixels_rgba(int width, int height)
{
#if defined(SOKOL_GLCORE)
    size_t pixel_count = (size_t)width * (size_t)height * 4;
    uint8_t *pixels = pz_alloc(pixel_count);
    if (!pixels)
        return NULL;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

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

    return pixels;
#else
    (void)width;
    (void)height;
    return NULL;
#endif
}

static uint8_t *
sokol_screenshot(pz_renderer *r, int *out_width, int *out_height)
{
    sokol_backend_data *data = r->backend_data;
    end_pass_if_active(data);

    int width = r->viewport_width;
    int height = r->viewport_height;

    uint8_t *pixels = read_pixels_rgba(width, height);
    if (!pixels)
        return NULL;

    *out_width = width;
    *out_height = height;
    return pixels;
}

uint8_t *
pz_render_sokol_read_render_target(pz_renderer *r,
    pz_render_target_handle handle, int *out_width, int *out_height)
{
#if defined(SOKOL_GLCORE)
    sokol_backend_data *data = r->backend_data;
    if (!data)
        return NULL;
    if (handle == PZ_INVALID_HANDLE || handle >= MAX_RENDER_TARGETS)
        return NULL;
    if (!data->render_targets[handle].used)
        return NULL;

    end_pass_if_active(data);

    sokol_render_target *rt = &data->render_targets[handle];
    sg_gl_image_info info = sg_gl_query_image_info(rt->color_image);
    GLuint gl_tex = info.tex[info.active_slot];
    if (gl_tex == 0)
        return NULL;

    GLint prev_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, info.tex_target, gl_tex, 0);

    uint8_t *pixels = read_pixels_rgba(rt->width, rt->height);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    glDeleteFramebuffers(1, &fbo);

    if (pixels) {
        *out_width = rt->width;
        *out_height = rt->height;
    }
    return pixels;
#else
    (void)r;
    (void)handle;
    (void)out_width;
    (void)out_height;
    return NULL;
#endif
}

/* ============================================================================
 * Public vtable
 * ============================================================================
 */

const pz_render_backend_vtable *
pz_render_backend_sokol_vtable(void)
{
    static pz_render_backend_vtable vtable = {
        .init = sokol_init,
        .shutdown = sokol_shutdown,
        .get_viewport = sokol_get_viewport,
        .set_viewport = sokol_set_viewport,
        .get_dpi_scale = sokol_get_dpi_scale,
        .create_shader = sokol_create_shader,
        .destroy_shader = sokol_destroy_shader,
        .create_texture = sokol_create_texture,
        .update_texture = sokol_update_texture,
        .destroy_texture = sokol_destroy_texture,
        .create_buffer = sokol_create_buffer,
        .update_buffer = sokol_update_buffer,
        .destroy_buffer = sokol_destroy_buffer,
        .create_pipeline = sokol_create_pipeline,
        .destroy_pipeline = sokol_destroy_pipeline,
        .create_render_target = sokol_create_render_target,
        .get_render_target_texture = sokol_get_render_target_texture,
        .destroy_render_target = sokol_destroy_render_target,
        .begin_frame = sokol_begin_frame,
        .end_frame = sokol_end_frame,
        .set_render_target = sokol_set_render_target,
        .clear = sokol_clear,
        .clear_color = sokol_clear_color,
        .clear_depth = sokol_clear_depth,
        .set_uniform_float = sokol_set_uniform_float,
        .set_uniform_vec2 = sokol_set_uniform_vec2,
        .set_uniform_vec3 = sokol_set_uniform_vec3,
        .set_uniform_vec4 = sokol_set_uniform_vec4,
        .set_uniform_mat4 = sokol_set_uniform_mat4,
        .set_uniform_int = sokol_set_uniform_int,
        .bind_texture = sokol_bind_texture,
        .draw = sokol_draw,
        .screenshot = sokol_screenshot,
    };

    return &vtable;
}
