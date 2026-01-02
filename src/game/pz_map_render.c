/*
 * Map Rendering System Implementation
 *
 * Renders terrain tiles and 3D wall geometry from map data.
 */

#include "pz_map_render.h"

#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"
#include "../core/pz_str.h"

// Texture paths for each terrain type
static const char *terrain_textures[PZ_TILE_COUNT] = {
    [PZ_TILE_GROUND] = "assets/textures/ground.png",
    [PZ_TILE_WALL] = "assets/textures/wall.png",
    [PZ_TILE_WATER] = "assets/textures/water.png",
    [PZ_TILE_MUD] = "assets/textures/mud.png",
    [PZ_TILE_ICE] = "assets/textures/ice.png",
};

// Wall textures
static const char *wall_top_texture = "assets/textures/wall.png";
static const char *wall_side_texture = "assets/textures/wall_side.png";

// Wall height unit (in world units per height level)
#define WALL_HEIGHT_UNIT 1.5f

struct pz_map_renderer {
    pz_renderer *renderer;
    pz_texture_manager *tex_manager;

    // Loaded textures for each terrain type
    pz_texture_handle textures[PZ_TILE_COUNT];

    // Wall textures
    pz_texture_handle wall_top_tex;
    pz_texture_handle wall_side_tex;

    // Ground shader and pipeline
    pz_shader_handle ground_shader;
    pz_pipeline_handle ground_pipeline;

    // Wall shader and pipeline
    pz_shader_handle wall_shader;
    pz_pipeline_handle wall_pipeline;

    // Ground vertex buffers (one per terrain type)
    pz_buffer_handle ground_buffers[PZ_TILE_COUNT];
    int ground_vertex_counts[PZ_TILE_COUNT];

    // Wall vertex buffer (all walls combined)
    pz_buffer_handle wall_buffer;
    int wall_vertex_count;

    // Current map
    const pz_map *map;
};

// ============================================================================
// Ground Mesh Generation
// ============================================================================

// Ground plane Y offset - slightly below walls
#define GROUND_Y_OFFSET -0.01f
// Ground shrink amount - shrink tiles slightly to avoid z-fighting with wall
// sides
#define GROUND_SHRINK 0.001f

// Create vertices for a single tile quad on ground plane
// Returns pointer past the last written float
static float *
emit_ground_quad(float *v, float x0, float z0, float x1, float z1)
{
    // Shrink the tile slightly
    x0 += GROUND_SHRINK;
    z0 += GROUND_SHRINK;
    x1 -= GROUND_SHRINK;
    z1 -= GROUND_SHRINK;

    // Y = GROUND_Y_OFFSET (slightly below 0)
    // Tile UVs: 0-1 per tile

    // Triangle 1 (CCW when viewed from above +Y)
    // Bottom-left
    *v++ = x0;
    *v++ = GROUND_Y_OFFSET;
    *v++ = z0;
    *v++ = 0.0f;
    *v++ = 1.0f;
    // Top-left
    *v++ = x0;
    *v++ = GROUND_Y_OFFSET;
    *v++ = z1;
    *v++ = 0.0f;
    *v++ = 0.0f;
    // Top-right
    *v++ = x1;
    *v++ = GROUND_Y_OFFSET;
    *v++ = z1;
    *v++ = 1.0f;
    *v++ = 0.0f;

    // Triangle 2
    // Bottom-left
    *v++ = x0;
    *v++ = GROUND_Y_OFFSET;
    *v++ = z0;
    *v++ = 0.0f;
    *v++ = 1.0f;
    // Top-right
    *v++ = x1;
    *v++ = GROUND_Y_OFFSET;
    *v++ = z1;
    *v++ = 1.0f;
    *v++ = 0.0f;
    // Bottom-right
    *v++ = x1;
    *v++ = GROUND_Y_OFFSET;
    *v++ = z0;
    *v++ = 1.0f;
    *v++ = 1.0f;

    return v;
}

// ============================================================================
// Wall Mesh Generation
// ============================================================================

// Wall vertex: position (3) + normal (3) + texcoord (2) = 8 floats
#define WALL_VERTEX_SIZE 8

// Emit a single quad face for walls (with normal)
static float *
emit_wall_face(float *v, float x0, float y0, float z0, float x1, float y1,
    float z1, float x2, float y2, float z2, float x3, float y3, float z3,
    float nx, float ny, float nz, float u0, float v0_uv, float u1, float v1_uv)
{
    // CCW winding for visible face
    // Triangle 1: v0, v1, v2
    *v++ = x0;
    *v++ = y0;
    *v++ = z0;
    *v++ = nx;
    *v++ = ny;
    *v++ = nz;
    *v++ = u0;
    *v++ = v1_uv;

    *v++ = x1;
    *v++ = y1;
    *v++ = z1;
    *v++ = nx;
    *v++ = ny;
    *v++ = nz;
    *v++ = u0;
    *v++ = v0_uv;

    *v++ = x2;
    *v++ = y2;
    *v++ = z2;
    *v++ = nx;
    *v++ = ny;
    *v++ = nz;
    *v++ = u1;
    *v++ = v0_uv;

    // Triangle 2: v0, v2, v3
    *v++ = x0;
    *v++ = y0;
    *v++ = z0;
    *v++ = nx;
    *v++ = ny;
    *v++ = nz;
    *v++ = u0;
    *v++ = v1_uv;

    *v++ = x2;
    *v++ = y2;
    *v++ = z2;
    *v++ = nx;
    *v++ = ny;
    *v++ = nz;
    *v++ = u1;
    *v++ = v0_uv;

    *v++ = x3;
    *v++ = y3;
    *v++ = z3;
    *v++ = nx;
    *v++ = ny;
    *v++ = nz;
    *v++ = u1;
    *v++ = v1_uv;

    return v;
}

// Emit a wall box (5 faces - no bottom)
// Returns pointer to next vertex
static float *
emit_wall_box(float *v, float x0, float z0, float x1, float z1, float height,
    int tile_x, int tile_y, const pz_map *map)
{
    float y0 = 0.0f;
    float y1 = height;

    // Check adjacent tiles to determine which side faces to render
    // Only render faces that are exposed (adjacent to non-wall or lower wall)
    uint8_t h = (uint8_t)(height / WALL_HEIGHT_UNIT);
    bool left_exposed = !pz_map_in_bounds(map, tile_x - 1, tile_y)
        || pz_map_get_height(map, tile_x - 1, tile_y) < h;
    bool right_exposed = !pz_map_in_bounds(map, tile_x + 1, tile_y)
        || pz_map_get_height(map, tile_x + 1, tile_y) < h;
    bool front_exposed = !pz_map_in_bounds(map, tile_x, tile_y + 1)
        || pz_map_get_height(map, tile_x, tile_y + 1) < h;
    bool back_exposed = !pz_map_in_bounds(map, tile_x, tile_y - 1)
        || pz_map_get_height(map, tile_x, tile_y - 1) < h;

    // Top face (always visible, normal up +Y)
    // Looking from above (+Y looking down): x increases right, z increases away
    // CCW from above: (x0,z0) -> (x0,z1) -> (x1,z1) -> (x1,z0)
    v = emit_wall_face(v, x0, y1, z0, // v0: near-left
        x0, y1, z1, // v1: far-left
        x1, y1, z1, // v2: far-right
        x1, y1, z0, // v3: near-right
        0.0f, 1.0f, 0.0f, // normal up
        0.0f, 0.0f, 1.0f, 1.0f);

    // Back face (-Z, at z0, visible from camera at -Z looking toward +Z)
    // This is the face we see when looking at the wall from the "front" of map
    if (back_exposed) {
        v = emit_wall_face(v, x0, y0, z0, // v0: bottom-left
            x0, y1, z0, // v1: top-left
            x1, y1, z0, // v2: top-right
            x1, y0, z0, // v3: bottom-right
            0.0f, 0.0f, -1.0f, // normal -Z (toward camera)
            0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Front face (+Z, at z1, visible from inside the map looking out)
    if (front_exposed) {
        v = emit_wall_face(v, x1, y0, z1, // v0: bottom-right (from inside view)
            x1, y1, z1, // v1: top-right
            x0, y1, z1, // v2: top-left
            x0, y0, z1, // v3: bottom-left
            0.0f, 0.0f, 1.0f, // normal +Z
            0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Left face (-X, at x0)
    if (left_exposed) {
        v = emit_wall_face(v, x0, y0, z1, // v0: bottom-far
            x0, y1, z1, // v1: top-far
            x0, y1, z0, // v2: top-near
            x0, y0, z0, // v3: bottom-near
            -1.0f, 0.0f, 0.0f, // normal -X
            0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Right face (+X, at x1)
    if (right_exposed) {
        v = emit_wall_face(v, x1, y0, z0, // v0: bottom-near
            x1, y1, z0, // v1: top-near
            x1, y1, z1, // v2: top-far
            x1, y0, z1, // v3: bottom-far
            1.0f, 0.0f, 0.0f, // normal +X
            0.0f, 0.0f, 1.0f, 1.0f);
    }

    return v;
}

// Count how many faces a wall tile will need
static int
count_wall_faces(int tile_x, int tile_y, float height, const pz_map *map)
{
    int count = 1; // Top face always
    uint8_t h = (uint8_t)(height / WALL_HEIGHT_UNIT);

    if (!pz_map_in_bounds(map, tile_x - 1, tile_y)
        || pz_map_get_height(map, tile_x - 1, tile_y) < h)
        count++; // left
    if (!pz_map_in_bounds(map, tile_x + 1, tile_y)
        || pz_map_get_height(map, tile_x + 1, tile_y) < h)
        count++; // right
    if (!pz_map_in_bounds(map, tile_x, tile_y + 1)
        || pz_map_get_height(map, tile_x, tile_y + 1) < h)
        count++; // front
    if (!pz_map_in_bounds(map, tile_x, tile_y - 1)
        || pz_map_get_height(map, tile_x, tile_y - 1) < h)
        count++; // back

    return count;
}

// ============================================================================
// Map Renderer Implementation
// ============================================================================

pz_map_renderer *
pz_map_renderer_create(pz_renderer *renderer, pz_texture_manager *tex_manager)
{
    pz_map_renderer *mr = pz_calloc(1, sizeof(pz_map_renderer));
    if (!mr) {
        return NULL;
    }

    mr->renderer = renderer;
    mr->tex_manager = tex_manager;

    // Load terrain textures with mipmapping for better quality at distance
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        mr->textures[i] = pz_texture_load_ex(tex_manager, terrain_textures[i],
            PZ_FILTER_LINEAR_MIPMAP, PZ_WRAP_REPEAT);
        if (mr->textures[i] == PZ_INVALID_HANDLE) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
                "Failed to load terrain texture: %s", terrain_textures[i]);
        }
    }

    // Load wall textures with mipmapping
    mr->wall_top_tex = pz_texture_load_ex(
        tex_manager, wall_top_texture, PZ_FILTER_LINEAR_MIPMAP, PZ_WRAP_REPEAT);
    mr->wall_side_tex = pz_texture_load_ex(tex_manager, wall_side_texture,
        PZ_FILTER_LINEAR_MIPMAP, PZ_WRAP_REPEAT);
    if (mr->wall_side_tex == PZ_INVALID_HANDLE) {
        // Fall back to top texture if side isn't available
        mr->wall_side_tex = mr->wall_top_tex;
    }

    // ==== Ground shader ====
    // Use ground shader that supports track texture overlay
    mr->ground_shader = pz_renderer_load_shader(
        renderer, "shaders/ground.vert", "shaders/ground.frag", "ground");
    if (mr->ground_shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to load ground shader");
        pz_map_renderer_destroy(mr);
        return NULL;
    }

    // Ground pipeline (position + texcoord)
    pz_vertex_attr ground_attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
        { .name = "a_texcoord",
            .type = PZ_ATTR_FLOAT2,
            .offset = 3 * sizeof(float) },
    };

    pz_pipeline_desc ground_desc = {
        .shader = mr->ground_shader,
        .vertex_layout = {
            .attrs = ground_attrs,
            .attr_count = 2,
            .stride = 5 * sizeof(float),
        },
        .blend = PZ_BLEND_NONE,
        .depth = PZ_DEPTH_READ_WRITE,
        .cull = PZ_CULL_BACK,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    mr->ground_pipeline = pz_renderer_create_pipeline(renderer, &ground_desc);

    // ==== Wall shader ====
    mr->wall_shader = pz_renderer_load_shader(
        renderer, "shaders/wall.vert", "shaders/wall.frag", "wall");
    if (mr->wall_shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to load wall shader");
        pz_map_renderer_destroy(mr);
        return NULL;
    }

    // Wall pipeline (position + normal + texcoord)
    pz_vertex_attr wall_attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
        { .name = "a_normal",
            .type = PZ_ATTR_FLOAT3,
            .offset = 3 * sizeof(float) },
        { .name = "a_texcoord",
            .type = PZ_ATTR_FLOAT2,
            .offset = 6 * sizeof(float) },
    };

    pz_pipeline_desc wall_desc = {
        .shader = mr->wall_shader,
        .vertex_layout = {
            .attrs = wall_attrs,
            .attr_count = 3,
            .stride = WALL_VERTEX_SIZE * sizeof(float),
        },
        .blend = PZ_BLEND_NONE,
        .depth = PZ_DEPTH_READ_WRITE,
        .cull = PZ_CULL_BACK,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    mr->wall_pipeline = pz_renderer_create_pipeline(renderer, &wall_desc);

    // Initialize buffer handles
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        mr->ground_buffers[i] = PZ_INVALID_HANDLE;
        mr->ground_vertex_counts[i] = 0;
    }
    mr->wall_buffer = PZ_INVALID_HANDLE;
    mr->wall_vertex_count = 0;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Map renderer created");
    return mr;
}

void
pz_map_renderer_destroy(pz_map_renderer *mr)
{
    if (!mr) {
        return;
    }

    // Destroy ground vertex buffers
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        if (mr->ground_buffers[i] != PZ_INVALID_HANDLE) {
            pz_renderer_destroy_buffer(mr->renderer, mr->ground_buffers[i]);
        }
    }

    // Destroy wall buffer
    if (mr->wall_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(mr->renderer, mr->wall_buffer);
    }

    // Pipelines
    if (mr->ground_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(mr->renderer, mr->ground_pipeline);
    }
    if (mr->wall_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(mr->renderer, mr->wall_pipeline);
    }

    // Shaders
    if (mr->ground_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(mr->renderer, mr->ground_shader);
    }
    if (mr->wall_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(mr->renderer, mr->wall_shader);
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

    // ==== Generate ground mesh ====

    // Destroy old ground buffers
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        if (mr->ground_buffers[i] != PZ_INVALID_HANDLE) {
            pz_renderer_destroy_buffer(mr->renderer, mr->ground_buffers[i]);
            mr->ground_buffers[i] = PZ_INVALID_HANDLE;
        }
        mr->ground_vertex_counts[i] = 0;
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

    float half_w = map->world_width / 2.0f;
    float half_h = map->world_height / 2.0f;

    // Generate ground vertices
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            pz_tile_type type = pz_map_get_tile(map, x, y);

            float x0 = x * map->tile_size - half_w;
            float x1 = (x + 1) * map->tile_size - half_w;
            float z0 = y * map->tile_size - half_h;
            float z1 = (y + 1) * map->tile_size - half_h;

            vertex_ptrs[type]
                = emit_ground_quad(vertex_ptrs[type], x0, z0, x1, z1);
        }
    }

    // Create GPU buffers for ground
    for (int i = 0; i < PZ_TILE_COUNT; i++) {
        if (tile_counts[i] > 0 && vertices[i]) {
            int num_verts = tile_counts[i] * 6;
            mr->ground_vertex_counts[i] = num_verts;

            pz_buffer_desc desc = {
                .type = PZ_BUFFER_VERTEX,
                .usage = PZ_BUFFER_STATIC,
                .data = vertices[i],
                .size = num_verts * 5 * sizeof(float),
            };
            mr->ground_buffers[i]
                = pz_renderer_create_buffer(mr->renderer, &desc);

            pz_free(vertices[i]);
        }
    }

    // ==== Generate wall mesh ====

    // Destroy old wall buffer
    if (mr->wall_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(mr->renderer, mr->wall_buffer);
        mr->wall_buffer = PZ_INVALID_HANDLE;
    }
    mr->wall_vertex_count = 0;

    // Count total wall faces (only walls with height > 0)
    int total_wall_faces = 0;
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            uint8_t h = pz_map_get_height(map, x, y);
            if (h > 0) {
                float height = h * WALL_HEIGHT_UNIT;
                total_wall_faces += count_wall_faces(x, y, height, map);
            }
        }
    }

    if (total_wall_faces > 0) {
        // 6 vertices per face, 8 floats per vertex
        int total_verts = total_wall_faces * 6;
        float *wall_verts
            = pz_alloc(total_verts * WALL_VERTEX_SIZE * sizeof(float));
        float *wall_ptr = wall_verts;

        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                uint8_t h = pz_map_get_height(map, x, y);
                if (h > 0) {
                    float height = h * WALL_HEIGHT_UNIT;

                    float x0 = x * map->tile_size - half_w;
                    float x1 = (x + 1) * map->tile_size - half_w;
                    float z0 = y * map->tile_size - half_h;
                    float z1 = (y + 1) * map->tile_size - half_h;

                    wall_ptr = emit_wall_box(
                        wall_ptr, x0, z0, x1, z1, height, x, y, map);
                }
            }
        }

        // Calculate actual vertex count (wall_ptr moved past all data)
        int actual_floats = (int)(wall_ptr - wall_verts);
        int actual_verts = actual_floats / WALL_VERTEX_SIZE;
        mr->wall_vertex_count = actual_verts;

        pz_buffer_desc desc = {
            .type = PZ_BUFFER_VERTEX,
            .usage = PZ_BUFFER_STATIC,
            .data = wall_verts,
            .size = actual_verts * WALL_VERTEX_SIZE * sizeof(float),
        };
        mr->wall_buffer = pz_renderer_create_buffer(mr->renderer, &desc);

        pz_free(wall_verts);
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER,
        "Map mesh generated: ground=%d, wall=%d, water=%d, mud=%d, ice=%d "
        "tiles, %d wall verts",
        tile_counts[PZ_TILE_GROUND], tile_counts[PZ_TILE_WALL],
        tile_counts[PZ_TILE_WATER], tile_counts[PZ_TILE_MUD],
        tile_counts[PZ_TILE_ICE], mr->wall_vertex_count);
}

void
pz_map_renderer_draw_ground(pz_map_renderer *mr, const pz_mat4 *view_projection,
    const pz_map_render_params *params)
{
    if (!mr || !mr->map) {
        return;
    }

    // Set common uniforms
    pz_renderer_set_uniform_mat4(
        mr->renderer, mr->ground_shader, "u_mvp", view_projection);
    pz_renderer_set_uniform_int(
        mr->renderer, mr->ground_shader, "u_texture", 0);

    // Set track texture uniforms
    if (params && params->track_texture != PZ_INVALID_HANDLE
        && params->track_texture != 0) {
        pz_renderer_bind_texture(mr->renderer, 1, params->track_texture);
        pz_renderer_set_uniform_int(
            mr->renderer, mr->ground_shader, "u_track_texture", 1);
        pz_renderer_set_uniform_int(
            mr->renderer, mr->ground_shader, "u_use_tracks", 1);
        pz_renderer_set_uniform_vec2(mr->renderer, mr->ground_shader,
            "u_track_scale",
            (pz_vec2) { params->track_scale_x, params->track_scale_z });
        pz_renderer_set_uniform_vec2(mr->renderer, mr->ground_shader,
            "u_track_offset",
            (pz_vec2) { params->track_offset_x, params->track_offset_z });
    } else {
        pz_renderer_set_uniform_int(
            mr->renderer, mr->ground_shader, "u_use_tracks", 0);
    }

    // Set light map uniforms
    if (params && params->light_texture != PZ_INVALID_HANDLE
        && params->light_texture != 0) {
        pz_renderer_bind_texture(mr->renderer, 2, params->light_texture);
        pz_renderer_set_uniform_int(
            mr->renderer, mr->ground_shader, "u_light_texture", 2);
        pz_renderer_set_uniform_int(
            mr->renderer, mr->ground_shader, "u_use_lighting", 1);
        pz_renderer_set_uniform_vec2(mr->renderer, mr->ground_shader,
            "u_light_scale",
            (pz_vec2) { params->light_scale_x, params->light_scale_z });
        pz_renderer_set_uniform_vec2(mr->renderer, mr->ground_shader,
            "u_light_offset",
            (pz_vec2) { params->light_offset_x, params->light_offset_z });
    } else {
        pz_renderer_set_uniform_int(
            mr->renderer, mr->ground_shader, "u_use_lighting", 0);
    }

    // Draw each terrain type
    // Skip walls - they have 3D geometry and drawing floor causes z-fighting
    pz_tile_type draw_order[] = {
        PZ_TILE_GROUND,
        PZ_TILE_MUD,
        PZ_TILE_ICE,
        PZ_TILE_WATER,
    };

    for (int i = 0; i < 4; i++) {
        pz_tile_type type = draw_order[i];

        if (mr->ground_vertex_counts[type] > 0
            && mr->ground_buffers[type] != PZ_INVALID_HANDLE
            && mr->textures[type] != PZ_INVALID_HANDLE) {

            pz_renderer_bind_texture(mr->renderer, 0, mr->textures[type]);

            pz_draw_cmd cmd = {
                .pipeline = mr->ground_pipeline,
                .vertex_buffer = mr->ground_buffers[type],
                .vertex_count = mr->ground_vertex_counts[type],
            };
            pz_renderer_draw(mr->renderer, &cmd);
        }
    }
}

void
pz_map_renderer_draw_walls(pz_map_renderer *mr, const pz_mat4 *view_projection,
    const pz_map_render_params *params)
{
    if (!mr || !mr->map || mr->wall_vertex_count == 0) {
        return;
    }

    // Identity model matrix (walls are in world space)
    pz_mat4 model = pz_mat4_identity();

    // Set wall shader uniforms
    pz_renderer_set_uniform_mat4(
        mr->renderer, mr->wall_shader, "u_mvp", view_projection);
    pz_renderer_set_uniform_mat4(
        mr->renderer, mr->wall_shader, "u_model", &model);

    // Simple directional light (from above-right-front) for top faces
    pz_vec3 light_dir = { 0.4f, 0.8f, 0.3f };
    pz_vec3 light_color = { 0.6f, 0.6f, 0.55f };
    pz_vec3 ambient = { 0.15f, 0.15f, 0.18f };

    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->wall_shader, "u_light_dir", light_dir);
    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->wall_shader, "u_light_color", light_color);
    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->wall_shader, "u_ambient", ambient);

    // Set light map uniforms for side faces
    if (params && params->light_texture != PZ_INVALID_HANDLE
        && params->light_texture != 0) {
        pz_renderer_bind_texture(mr->renderer, 2, params->light_texture);
        pz_renderer_set_uniform_int(
            mr->renderer, mr->wall_shader, "u_light_texture", 2);
        pz_renderer_set_uniform_int(
            mr->renderer, mr->wall_shader, "u_use_lighting", 1);
        pz_renderer_set_uniform_vec2(mr->renderer, mr->wall_shader,
            "u_light_scale",
            (pz_vec2) { params->light_scale_x, params->light_scale_z });
        pz_renderer_set_uniform_vec2(mr->renderer, mr->wall_shader,
            "u_light_offset",
            (pz_vec2) { params->light_offset_x, params->light_offset_z });
    } else {
        pz_renderer_set_uniform_int(
            mr->renderer, mr->wall_shader, "u_use_lighting", 0);
    }

    // Bind textures
    pz_renderer_set_uniform_int(
        mr->renderer, mr->wall_shader, "u_texture_top", 0);
    pz_renderer_set_uniform_int(
        mr->renderer, mr->wall_shader, "u_texture_side", 1);

    pz_renderer_bind_texture(mr->renderer, 0, mr->wall_top_tex);
    pz_renderer_bind_texture(mr->renderer, 1, mr->wall_side_tex);

    pz_draw_cmd cmd = {
        .pipeline = mr->wall_pipeline,
        .vertex_buffer = mr->wall_buffer,
        .vertex_count = mr->wall_vertex_count,
    };
    pz_renderer_draw(mr->renderer, &cmd);
}

void
pz_map_renderer_draw(pz_map_renderer *mr, const pz_mat4 *view_projection,
    const pz_map_render_params *params)
{
    // Draw walls first, then ground - depth test will prevent ground from
    // overwriting wall sides
    pz_map_renderer_draw_walls(mr, view_projection, params);
    pz_map_renderer_draw_ground(mr, view_projection, params);
}

void
pz_map_renderer_check_hot_reload(pz_map_renderer *mr)
{
    // Texture hot-reload is handled by the texture manager
    (void)mr;
}

// ============================================================================
// Map Hot-Reload Implementation
// ============================================================================

struct pz_map_hot_reload {
    char *path; // Path to map file
    pz_map **map_ptr; // Pointer to the map pointer (for swapping)
    pz_map_renderer *renderer; // Renderer to update on reload
    int64_t last_mtime; // Last modification time
};

pz_map_hot_reload *
pz_map_hot_reload_create(
    const char *path, pz_map **map_ptr, pz_map_renderer *renderer)
{
    if (!path || !map_ptr || !renderer) {
        return NULL;
    }

    pz_map_hot_reload *hr = pz_calloc(1, sizeof(pz_map_hot_reload));
    if (!hr) {
        return NULL;
    }

    hr->path = pz_str_dup(path);
    hr->map_ptr = map_ptr;
    hr->renderer = renderer;
    hr->last_mtime = pz_map_file_mtime(path);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Map hot-reload watching: %s", path);

    return hr;
}

void
pz_map_hot_reload_destroy(pz_map_hot_reload *hr)
{
    if (!hr) {
        return;
    }

    pz_free(hr->path);
    pz_free(hr);
}

bool
pz_map_hot_reload_check(pz_map_hot_reload *hr)
{
    if (!hr || !hr->path) {
        return false;
    }

    int64_t mtime = pz_map_file_mtime(hr->path);
    if (mtime == 0) {
        // File doesn't exist or can't be read
        return false;
    }

    if (mtime == hr->last_mtime) {
        // No change
        return false;
    }

    // File changed - reload
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Reloading map: %s", hr->path);

    pz_map *new_map = pz_map_load(hr->path);
    if (!new_map) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to reload map: %s",
            hr->path);
        // Update mtime anyway to avoid spamming reload attempts
        hr->last_mtime = mtime;
        return false;
    }

    // Destroy old map
    if (*hr->map_ptr) {
        pz_map_destroy(*hr->map_ptr);
    }

    // Set new map
    *hr->map_ptr = new_map;

    // Update renderer
    pz_map_renderer_set_map(hr->renderer, new_map);

    hr->last_mtime = mtime;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Map reloaded successfully");

    return true;
}

const char *
pz_map_hot_reload_get_path(const pz_map_hot_reload *hr)
{
    return hr ? hr->path : NULL;
}
