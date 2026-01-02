/*
 * Map Rendering System Implementation
 */

#include "pz_map_render.h"

#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

// Texture paths for each terrain type
static const char *terrain_textures[PZ_TILE_COUNT] = {
    [PZ_TILE_GROUND] = "assets/textures/ground.png",
    [PZ_TILE_WALL] = "assets/textures/wall.png",
    [PZ_TILE_WATER] = "assets/textures/water.png",
    [PZ_TILE_MUD] = "assets/textures/mud.png",
    [PZ_TILE_ICE] = "assets/textures/ice.png",
};

struct pz_map_renderer {
    pz_renderer *renderer;
    pz_texture_manager *tex_manager;

    // Loaded textures for each terrain type
    pz_texture_handle textures[PZ_TILE_COUNT];

    // Shader
    pz_shader_handle shader;

    // Pipeline
    pz_pipeline_handle pipeline;

    // Vertex buffer (one per terrain type to batch by texture)
    pz_buffer_handle vertex_buffers[PZ_TILE_COUNT];
    int vertex_counts[PZ_TILE_COUNT];

    // Current map
    const pz_map *map;
};

// Create vertices for a single tile quad
// Returns pointer past the last written float
static float *
emit_tile_quad(float *v, float x0, float z0, float x1, float z1)
{
    // Y = 0 (ground plane)
    // Tile UVs: 0-1 per tile (texture will repeat)

    // Triangle 1 (CCW when viewed from above +Y)
    // Bottom-left
    *v++ = x0;
    *v++ = 0.0f;
    *v++ = z0;
    *v++ = 0.0f;
    *v++ = 1.0f;
    // Top-left
    *v++ = x0;
    *v++ = 0.0f;
    *v++ = z1;
    *v++ = 0.0f;
    *v++ = 0.0f;
    // Top-right
    *v++ = x1;
    *v++ = 0.0f;
    *v++ = z1;
    *v++ = 1.0f;
    *v++ = 0.0f;

    // Triangle 2
    // Bottom-left
    *v++ = x0;
    *v++ = 0.0f;
    *v++ = z0;
    *v++ = 0.0f;
    *v++ = 1.0f;
    // Top-right
    *v++ = x1;
    *v++ = 0.0f;
    *v++ = z1;
    *v++ = 1.0f;
    *v++ = 0.0f;
    // Bottom-right
    *v++ = x1;
    *v++ = 0.0f;
    *v++ = z0;
    *v++ = 1.0f;
    *v++ = 1.0f;

    return v;
}

pz_map_renderer *
pz_map_renderer_create(pz_renderer *renderer, pz_texture_manager *tex_manager)
{
    pz_map_renderer *mr = pz_calloc(1, sizeof(pz_map_renderer));
    if (!mr) {
        return NULL;
    }

    mr->renderer = renderer;
    mr->tex_manager = tex_manager;

    // Load textures
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        mr->textures[i] = pz_texture_load(tex_manager, terrain_textures[i]);
        if (mr->textures[i] == PZ_INVALID_HANDLE) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
                "Failed to load terrain texture: %s", terrain_textures[i]);
        }
    }

    // Load shader
    mr->shader = pz_renderer_load_shader(
        renderer, "shaders/textured.vert", "shaders/textured.frag", "map");
    if (mr->shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to load map shader");
        pz_map_renderer_destroy(mr);
        return NULL;
    }

    // Create pipeline
    pz_vertex_attr attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
        { .name = "a_texcoord",
            .type = PZ_ATTR_FLOAT2,
            .offset = 3 * sizeof(float) },
    };

    pz_pipeline_desc pipeline_desc = {
        .shader = mr->shader,
        .vertex_layout = {
            .attrs = attrs,
            .attr_count = 2,
            .stride = 5 * sizeof(float),
        },
        .blend = PZ_BLEND_NONE,
        .depth = PZ_DEPTH_READ_WRITE,
        .cull = PZ_CULL_BACK,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    mr->pipeline = pz_renderer_create_pipeline(renderer, &pipeline_desc);

    // Initialize buffer handles
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        mr->vertex_buffers[i] = PZ_INVALID_HANDLE;
        mr->vertex_counts[i] = 0;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Map renderer created");
    return mr;
}

void
pz_map_renderer_destroy(pz_map_renderer *mr)
{
    if (!mr) {
        return;
    }

    // Destroy vertex buffers
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        if (mr->vertex_buffers[i] != PZ_INVALID_HANDLE) {
            pz_renderer_destroy_buffer(mr->renderer, mr->vertex_buffers[i]);
        }
    }

    // Pipeline and shader
    if (mr->pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(mr->renderer, mr->pipeline);
    }
    if (mr->shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(mr->renderer, mr->shader);
    }

    // Textures are managed by texture manager

    pz_free(mr);
}

void
pz_map_renderer_set_map(pz_map_renderer *mr, const pz_map *map)
{
    if (!mr || !map) {
        return;
    }

    mr->map = map;

    // Destroy old buffers
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        if (mr->vertex_buffers[i] != PZ_INVALID_HANDLE) {
            pz_renderer_destroy_buffer(mr->renderer, mr->vertex_buffers[i]);
            mr->vertex_buffers[i] = PZ_INVALID_HANDLE;
        }
        mr->vertex_counts[i] = 0;
    }

    // Count tiles of each type
    int tile_counts[PZ_TILE_COUNT] = { 0 };
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            pz_tile_type type = pz_map_get_tile(map, x, y);
            tile_counts[type]++;
        }
    }

    // Allocate vertex arrays for each type
    float *vertices[PZ_TILE_COUNT] = { NULL };
    float *vertex_ptrs[PZ_TILE_COUNT] = { NULL };

    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        if (tile_counts[i] > 0) {
            // 6 vertices per tile, 5 floats per vertex
            vertices[i] = pz_alloc(tile_counts[i] * 6 * 5 * sizeof(float));
            vertex_ptrs[i] = vertices[i];
        }
    }

    // Generate vertices for each tile
    float half_w = map->world_width / 2.0f;
    float half_h = map->world_height / 2.0f;

    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            pz_tile_type type = pz_map_get_tile(map, x, y);

            // Calculate world coordinates
            // Note: we use Z for the "y" map coordinate (Y is up in 3D)
            float x0 = x * map->tile_size - half_w;
            float x1 = (x + 1) * map->tile_size - half_w;
            float z0 = y * map->tile_size - half_h;
            float z1 = (y + 1) * map->tile_size - half_h;

            // Emit the quad
            vertex_ptrs[type]
                = emit_tile_quad(vertex_ptrs[type], x0, z0, x1, z1);
        }
    }

    // Create GPU buffers
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        if (tile_counts[i] > 0 && vertices[i]) {
            int num_verts = tile_counts[i] * 6;
            mr->vertex_counts[i] = num_verts;

            pz_buffer_desc desc = {
                .type = PZ_BUFFER_VERTEX,
                .usage = PZ_BUFFER_STATIC,
                .data = vertices[i],
                .size = num_verts * 5 * sizeof(float),
            };
            mr->vertex_buffers[i]
                = pz_renderer_create_buffer(mr->renderer, &desc);

            pz_free(vertices[i]);
        }
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER,
        "Map mesh generated: ground=%d, wall=%d, water=%d, mud=%d, ice=%d "
        "tiles",
        tile_counts[PZ_TILE_GROUND], tile_counts[PZ_TILE_WALL],
        tile_counts[PZ_TILE_WATER], tile_counts[PZ_TILE_MUD],
        tile_counts[PZ_TILE_ICE]);
}

void
pz_map_renderer_draw(pz_map_renderer *mr, const pz_mat4 *view_projection)
{
    if (!mr || !mr->map) {
        return;
    }

    // Set common uniforms
    pz_renderer_set_uniform_mat4(
        mr->renderer, mr->shader, "u_mvp", view_projection);
    pz_renderer_set_uniform_int(mr->renderer, mr->shader, "u_texture", 0);

    // Draw each terrain type
    // Order: ground first, then special terrains on top
    // We include walls here as floor tiles - they'll get 3D blocks in M3.4
    pz_tile_type draw_order[] = {
        PZ_TILE_GROUND,
        PZ_TILE_WALL, // Render wall floor (will have 3D blocks on top later)
        PZ_TILE_MUD,
        PZ_TILE_ICE,
        PZ_TILE_WATER,
    };

    for (int i = 0; i < 5; i++) {
        pz_tile_type type = draw_order[i];

        if (mr->vertex_counts[type] > 0
            && mr->vertex_buffers[type] != PZ_INVALID_HANDLE
            && mr->textures[type] != PZ_INVALID_HANDLE) {

            pz_renderer_bind_texture(mr->renderer, 0, mr->textures[type]);

            pz_draw_cmd cmd = {
                .pipeline = mr->pipeline,
                .vertex_buffer = mr->vertex_buffers[type],
                .vertex_count = mr->vertex_counts[type],
            };
            pz_renderer_draw(mr->renderer, &cmd);
        }
    }
}

void
pz_map_renderer_check_hot_reload(pz_map_renderer *mr)
{
    // Texture hot-reload is handled by the texture manager
    (void)mr;
}
