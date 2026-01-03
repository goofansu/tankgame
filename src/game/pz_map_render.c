/*
 * Map Rendering System Implementation
 *
 * Renders terrain tiles and 3D wall geometry from map data.
 * Uses tile definitions from the map for textures and properties.
 */

#include "pz_map_render.h"

#include <stdio.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"
#include "../core/pz_str.h"

// Wall height unit (in world units per height level)
#define WALL_HEIGHT_UNIT 1.5f

// Maximum number of tile textures we can cache
#define MAX_TILE_TEXTURES 32

// Ground mesh batch (one per unique texture)
typedef struct ground_batch {
    pz_texture_handle texture;
    pz_buffer_handle buffer;
    int vertex_count;
} ground_batch;

struct pz_map_renderer {
    pz_renderer *renderer;
    pz_texture_manager *tex_manager;

    // Tile texture cache (indexed by tile_def index)
    pz_texture_handle tile_textures[MAX_TILE_TEXTURES];

    // Wall textures
    pz_texture_handle wall_top_tex;
    pz_texture_handle wall_side_tex;

    // Ground shader and pipeline
    pz_shader_handle ground_shader;
    pz_pipeline_handle ground_pipeline;

    // Wall shader and pipeline
    pz_shader_handle wall_shader;
    pz_pipeline_handle wall_pipeline;

    // Water shader and pipeline
    pz_shader_handle water_shader;
    pz_pipeline_handle water_pipeline;
    pz_texture_handle water_caustic_texture;

    // Ground batches (one per tile type with geometry)
    ground_batch ground_batches[MAX_TILE_TEXTURES];
    int ground_batch_count;

    // Wall vertex buffer (all walls combined)
    pz_buffer_handle wall_buffer;
    int wall_vertex_count;

    // Water vertex buffer
    pz_buffer_handle water_buffer;
    int water_vertex_count;

    // Current map
    const pz_map *map;
};

// ============================================================================
// Ground Mesh Generation
// ============================================================================

// Ground plane Y offset - slightly below walls
#define GROUND_Y_OFFSET -0.01f
// Ground shrink amount - shrink tiles slightly to avoid z-fighting
#define GROUND_SHRINK 0.001f
// Water plane Y offset - water surface is at this Y level relative to ground
#define WATER_Y_OFFSET -0.5f

// Create vertices for a single tile quad on water plane
static float *
emit_water_quad(float *v, float x0, float z0, float x1, float z1, float y)
{
    // Triangle 1 (CCW when viewed from above +Y)
    *v++ = x0;
    *v++ = y;
    *v++ = z0;
    *v++ = 0.0f;
    *v++ = 1.0f;

    *v++ = x0;
    *v++ = y;
    *v++ = z1;
    *v++ = 0.0f;
    *v++ = 0.0f;

    *v++ = x1;
    *v++ = y;
    *v++ = z1;
    *v++ = 1.0f;
    *v++ = 0.0f;

    // Triangle 2
    *v++ = x0;
    *v++ = y;
    *v++ = z0;
    *v++ = 0.0f;
    *v++ = 1.0f;

    *v++ = x1;
    *v++ = y;
    *v++ = z1;
    *v++ = 1.0f;
    *v++ = 0.0f;

    *v++ = x1;
    *v++ = y;
    *v++ = z0;
    *v++ = 1.0f;
    *v++ = 1.0f;

    return v;
}

// Create vertices for a single tile quad on ground plane (at custom Y height)
static float *
emit_ground_quad_at_height(
    float *v, float x0, float z0, float x1, float z1, float y)
{
    x0 += GROUND_SHRINK;
    z0 += GROUND_SHRINK;
    x1 -= GROUND_SHRINK;
    z1 -= GROUND_SHRINK;

    // Triangle 1 (CCW when viewed from above +Y)
    *v++ = x0;
    *v++ = y;
    *v++ = z0;
    *v++ = 0.0f;
    *v++ = 1.0f;

    *v++ = x0;
    *v++ = y;
    *v++ = z1;
    *v++ = 0.0f;
    *v++ = 0.0f;

    *v++ = x1;
    *v++ = y;
    *v++ = z1;
    *v++ = 1.0f;
    *v++ = 0.0f;

    // Triangle 2
    *v++ = x0;
    *v++ = y;
    *v++ = z0;
    *v++ = 0.0f;
    *v++ = 1.0f;

    *v++ = x1;
    *v++ = y;
    *v++ = z1;
    *v++ = 1.0f;
    *v++ = 0.0f;

    *v++ = x1;
    *v++ = y;
    *v++ = z0;
    *v++ = 1.0f;
    *v++ = 1.0f;

    return v;
}

// Create vertices for a single tile quad on ground plane
static float *
emit_ground_quad(float *v, float x0, float z0, float x1, float z1)
{
    return emit_ground_quad_at_height(v, x0, z0, x1, z1, GROUND_Y_OFFSET);
}

// ============================================================================
// Wall Mesh Generation
// ============================================================================

#define WALL_VERTEX_SIZE 8

static float *
emit_wall_face(float *v, float x0, float y0, float z0, float x1, float y1,
    float z1, float x2, float y2, float z2, float x3, float y3, float z3,
    float nx, float ny, float nz, float u0, float v0_uv, float u1, float v1_uv)
{
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

static float *
emit_wall_box(float *v, float x0, float z0, float x1, float z1, float height,
    int tile_x, int tile_y, const pz_map *map)
{
    float y0 = 0.0f;
    float y1 = height;

    int8_t h = (int8_t)(height / WALL_HEIGHT_UNIT);
    bool left_exposed = !pz_map_in_bounds(map, tile_x - 1, tile_y)
        || pz_map_get_height(map, tile_x - 1, tile_y) < h;
    bool right_exposed = !pz_map_in_bounds(map, tile_x + 1, tile_y)
        || pz_map_get_height(map, tile_x + 1, tile_y) < h;
    bool front_exposed = !pz_map_in_bounds(map, tile_x, tile_y + 1)
        || pz_map_get_height(map, tile_x, tile_y + 1) < h;
    bool back_exposed = !pz_map_in_bounds(map, tile_x, tile_y - 1)
        || pz_map_get_height(map, tile_x, tile_y - 1) < h;

    // Top face (always visible)
    v = emit_wall_face(v, x0, y1, z0, x0, y1, z1, x1, y1, z1, x1, y1, z0, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    // Back face (-Z)
    if (back_exposed) {
        v = emit_wall_face(v, x0, y0, z0, x0, y1, z0, x1, y1, z0, x1, y0, z0,
            0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Front face (+Z)
    if (front_exposed) {
        v = emit_wall_face(v, x1, y0, z1, x1, y1, z1, x0, y1, z1, x0, y0, z1,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Left face (-X)
    if (left_exposed) {
        v = emit_wall_face(v, x0, y0, z1, x0, y1, z1, x0, y1, z0, x0, y0, z0,
            -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Right face (+X)
    if (right_exposed) {
        v = emit_wall_face(v, x1, y0, z0, x1, y1, z0, x1, y1, z1, x1, y0, z1,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    return v;
}

static int
count_wall_faces(int tile_x, int tile_y, float height, const pz_map *map)
{
    int count = 1; // Top face always
    int8_t h = (int8_t)(height / WALL_HEIGHT_UNIT);

    if (!pz_map_in_bounds(map, tile_x - 1, tile_y)
        || pz_map_get_height(map, tile_x - 1, tile_y) < h)
        count++;
    if (!pz_map_in_bounds(map, tile_x + 1, tile_y)
        || pz_map_get_height(map, tile_x + 1, tile_y) < h)
        count++;
    if (!pz_map_in_bounds(map, tile_x, tile_y + 1)
        || pz_map_get_height(map, tile_x, tile_y + 1) < h)
        count++;
    if (!pz_map_in_bounds(map, tile_x, tile_y - 1)
        || pz_map_get_height(map, tile_x, tile_y - 1) < h)
        count++;

    return count;
}

// ============================================================================
// Pit Mesh Generation (negative height = below ground level)
// ============================================================================

// Emit a pit box - walls going DOWN from ground level
// depth is a positive value representing how deep the pit goes
// Pit walls face INWARD (toward center of pit) so they're visible from above
static float *
emit_pit_box(float *v, float x0, float z0, float x1, float z1, float depth,
    int tile_x, int tile_y, const pz_map *map)
{
    float y1 = GROUND_Y_OFFSET - depth; // Bottom of pit

    int8_t h = pz_map_get_height(map, tile_x, tile_y); // negative

    // Check neighbors - we need walls where adjacent tile is higher (less
    // negative)
    int8_t left_h = pz_map_in_bounds(map, tile_x - 1, tile_y)
        ? pz_map_get_height(map, tile_x - 1, tile_y)
        : 0;
    int8_t right_h = pz_map_in_bounds(map, tile_x + 1, tile_y)
        ? pz_map_get_height(map, tile_x + 1, tile_y)
        : 0;
    int8_t front_h = pz_map_in_bounds(map, tile_x, tile_y + 1)
        ? pz_map_get_height(map, tile_x, tile_y + 1)
        : 0;
    int8_t back_h = pz_map_in_bounds(map, tile_x, tile_y - 1)
        ? pz_map_get_height(map, tile_x, tile_y - 1)
        : 0;

    // Pit walls face INWARD so they're visible when looking down into the pit
    // This is opposite to regular walls which face outward

    // Back wall (-Z edge of pit): faces +Z (into pit)
    // Visible when looking from +Z side into the pit
    if (back_h > h) {
        float neighbor_y
            = GROUND_Y_OFFSET - (back_h < 0 ? -back_h : 0) * WALL_HEIGHT_UNIT;
        // Use same winding as regular front face (+Z normal)
        v = emit_wall_face(v, x1, y1, z0, // bottom right
            x1, neighbor_y, z0, // top right
            x0, neighbor_y, z0, // top left
            x0, y1, z0, // bottom left
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Front wall (+Z edge of pit): faces -Z (into pit)
    // Visible when looking from -Z side into the pit
    if (front_h > h) {
        float neighbor_y
            = GROUND_Y_OFFSET - (front_h < 0 ? -front_h : 0) * WALL_HEIGHT_UNIT;
        // Use same winding as regular back face (-Z normal)
        v = emit_wall_face(v, x0, y1, z1, // bottom left
            x0, neighbor_y, z1, // top left
            x1, neighbor_y, z1, // top right
            x1, y1, z1, // bottom right
            0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Left wall (-X edge of pit): faces +X (into pit)
    // Visible when looking from +X side into the pit
    if (left_h > h) {
        float neighbor_y
            = GROUND_Y_OFFSET - (left_h < 0 ? -left_h : 0) * WALL_HEIGHT_UNIT;
        // Use same winding as regular right face (+X normal)
        v = emit_wall_face(v, x0, y1, z0, // bottom back
            x0, neighbor_y, z0, // top back
            x0, neighbor_y, z1, // top front
            x0, y1, z1, // bottom front
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Right wall (+X edge of pit): faces -X (into pit)
    // Visible when looking from -X side into the pit
    if (right_h > h) {
        float neighbor_y
            = GROUND_Y_OFFSET - (right_h < 0 ? -right_h : 0) * WALL_HEIGHT_UNIT;
        // Use same winding as regular left face (-X normal)
        v = emit_wall_face(v, x1, y1, z1, // bottom front
            x1, neighbor_y, z1, // top front
            x1, neighbor_y, z0, // top back
            x1, y1, z0, // bottom back
            -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    return v;
}

// Count pit wall faces for a given pit tile
static int
count_pit_faces(int tile_x, int tile_y, const pz_map *map)
{
    int count = 0;
    int8_t h = pz_map_get_height(map, tile_x, tile_y);

    int8_t left_h = pz_map_in_bounds(map, tile_x - 1, tile_y)
        ? pz_map_get_height(map, tile_x - 1, tile_y)
        : 0;
    int8_t right_h = pz_map_in_bounds(map, tile_x + 1, tile_y)
        ? pz_map_get_height(map, tile_x + 1, tile_y)
        : 0;
    int8_t front_h = pz_map_in_bounds(map, tile_x, tile_y + 1)
        ? pz_map_get_height(map, tile_x, tile_y + 1)
        : 0;
    int8_t back_h = pz_map_in_bounds(map, tile_x, tile_y - 1)
        ? pz_map_get_height(map, tile_x, tile_y - 1)
        : 0;

    if (back_h > h)
        count++;
    if (front_h > h)
        count++;
    if (left_h > h)
        count++;
    if (right_h > h)
        count++;

    return count;
}

// ============================================================================
// Texture Loading
// ============================================================================

static pz_texture_handle
load_tile_texture(pz_map_renderer *mr, const pz_tile_def *def)
{
    if (!def || !def->texture[0]) {
        return PZ_INVALID_HANDLE;
    }

    // Try to load the texture
    pz_texture_handle tex = pz_texture_load_ex(
        mr->tex_manager, def->texture, PZ_FILTER_LINEAR_MIPMAP, PZ_WRAP_REPEAT);

    if (tex == PZ_INVALID_HANDLE) {
        // Try fallback textures based on name
        char fallback[128];

        // Try assets/textures/<name>.png
        snprintf(
            fallback, sizeof(fallback), "assets/textures/%s.png", def->name);
        tex = pz_texture_load_ex(
            mr->tex_manager, fallback, PZ_FILTER_LINEAR_MIPMAP, PZ_WRAP_REPEAT);

        if (tex == PZ_INVALID_HANDLE) {
            // Last resort: use wood_oak_brown as default ground texture
            tex = pz_texture_load_ex(mr->tex_manager,
                "assets/textures/wood_oak_brown.png", PZ_FILTER_LINEAR_MIPMAP,
                PZ_WRAP_REPEAT);
        }
    }

    return tex;
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

    // Initialize texture handles
    for (int i = 0; i < MAX_TILE_TEXTURES; i++) {
        mr->tile_textures[i] = PZ_INVALID_HANDLE;
    }

    // Load default wall textures (using wood textures)
    mr->wall_top_tex = pz_texture_load_ex(tex_manager,
        "assets/textures/wood_rustic_dark.png", PZ_FILTER_LINEAR_MIPMAP,
        PZ_WRAP_REPEAT);
    mr->wall_side_tex
        = pz_texture_load_ex(tex_manager, "assets/textures/wood_walnut.png",
            PZ_FILTER_LINEAR_MIPMAP, PZ_WRAP_REPEAT);
    if (mr->wall_side_tex == PZ_INVALID_HANDLE) {
        mr->wall_side_tex = mr->wall_top_tex;
    }

    // Ground shader
    mr->ground_shader = pz_renderer_load_shader(
        renderer, "shaders/ground.vert", "shaders/ground.frag", "ground");
    if (mr->ground_shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to load ground shader");
        pz_map_renderer_destroy(mr);
        return NULL;
    }

    // Ground pipeline
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

    // Wall shader
    mr->wall_shader = pz_renderer_load_shader(
        renderer, "shaders/wall.vert", "shaders/wall.frag", "wall");
    if (mr->wall_shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to load wall shader");
        pz_map_renderer_destroy(mr);
        return NULL;
    }

    // Wall pipeline
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

    // Water shader
    mr->water_shader = pz_renderer_load_shader(
        renderer, "shaders/water.vert", "shaders/water.frag", "water");
    if (mr->water_shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to load water shader");
        pz_map_renderer_destroy(mr);
        return NULL;
    }

    // Water pipeline (same vertex layout as ground)
    pz_pipeline_desc water_desc = {
        .shader = mr->water_shader,
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
    mr->water_pipeline = pz_renderer_create_pipeline(renderer, &water_desc);

    // Load water caustic texture
    mr->water_caustic_texture
        = pz_texture_load(tex_manager, "assets/textures/water_caustic.png");
    if (mr->water_caustic_texture == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
            "Failed to load water caustic texture, water effect degraded");
    }

    // Initialize batch handles
    for (int i = 0; i < MAX_TILE_TEXTURES; i++) {
        mr->ground_batches[i].buffer = PZ_INVALID_HANDLE;
        mr->ground_batches[i].texture = PZ_INVALID_HANDLE;
        mr->ground_batches[i].vertex_count = 0;
    }
    mr->ground_batch_count = 0;

    mr->wall_buffer = PZ_INVALID_HANDLE;
    mr->wall_vertex_count = 0;

    mr->water_buffer = PZ_INVALID_HANDLE;
    mr->water_vertex_count = 0;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Map renderer created");
    return mr;
}

void
pz_map_renderer_destroy(pz_map_renderer *mr)
{
    if (!mr) {
        return;
    }

    // Destroy ground batches
    for (int i = 0; i < MAX_TILE_TEXTURES; i++) {
        if (mr->ground_batches[i].buffer != PZ_INVALID_HANDLE) {
            pz_renderer_destroy_buffer(
                mr->renderer, mr->ground_batches[i].buffer);
        }
    }

    // Destroy wall buffer
    if (mr->wall_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(mr->renderer, mr->wall_buffer);
    }

    // Destroy water buffer
    if (mr->water_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(mr->renderer, mr->water_buffer);
    }

    // Pipelines
    if (mr->ground_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(mr->renderer, mr->ground_pipeline);
    }
    if (mr->wall_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(mr->renderer, mr->wall_pipeline);
    }
    if (mr->water_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(mr->renderer, mr->water_pipeline);
    }

    // Shaders
    if (mr->ground_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(mr->renderer, mr->ground_shader);
    }
    if (mr->wall_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(mr->renderer, mr->wall_shader);
    }
    if (mr->water_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(mr->renderer, mr->water_shader);
    }

    pz_free(mr);
}

void
pz_map_renderer_set_map(pz_map_renderer *mr, const pz_map *map)
{
    if (!mr || !map) {
        return;
    }

    mr->map = map;

    // Load textures for all tile definitions
    for (int i = 0; i < map->tile_def_count && i < MAX_TILE_TEXTURES; i++) {
        mr->tile_textures[i] = load_tile_texture(mr, &map->tile_defs[i]);
    }

    // Destroy old ground batches
    for (int i = 0; i < MAX_TILE_TEXTURES; i++) {
        if (mr->ground_batches[i].buffer != PZ_INVALID_HANDLE) {
            pz_renderer_destroy_buffer(
                mr->renderer, mr->ground_batches[i].buffer);
            mr->ground_batches[i].buffer = PZ_INVALID_HANDLE;
        }
        mr->ground_batches[i].vertex_count = 0;
        mr->ground_batches[i].texture = PZ_INVALID_HANDLE;
    }
    mr->ground_batch_count = 0;

    // Count tiles per tile_def index
    int tile_counts[MAX_TILE_TEXTURES] = { 0 };
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            uint8_t idx = pz_map_get_tile_index(map, x, y);
            if (idx < MAX_TILE_TEXTURES) {
                tile_counts[idx]++;
            }
        }
    }

    // Allocate vertex arrays for each tile type
    float *vertices[MAX_TILE_TEXTURES] = { NULL };
    float *vertex_ptrs[MAX_TILE_TEXTURES] = { NULL };

    for (int i = 0; i < map->tile_def_count && i < MAX_TILE_TEXTURES; i++) {
        if (tile_counts[i] > 0) {
            vertices[i] = pz_alloc(tile_counts[i] * 6 * 5 * sizeof(float));
            vertex_ptrs[i] = vertices[i];
        }
    }

    float half_w = map->world_width / 2.0f;
    float half_h = map->world_height / 2.0f;

    // Generate ground vertices (lowered for pits)
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            uint8_t idx = pz_map_get_tile_index(map, x, y);
            if (idx >= MAX_TILE_TEXTURES || !vertex_ptrs[idx]) {
                continue;
            }

            float x0 = x * map->tile_size - half_w;
            float x1 = (x + 1) * map->tile_size - half_w;
            float z0 = y * map->tile_size - half_h;
            float z1 = (y + 1) * map->tile_size - half_h;

            // Calculate Y position based on height
            // Positive heights are walls (ground at 0)
            // Negative heights are pits (ground lowered)
            int8_t h = pz_map_get_height(map, x, y);
            float ground_y = GROUND_Y_OFFSET;
            if (h < 0) {
                // Lower ground for pits
                ground_y = GROUND_Y_OFFSET + h * WALL_HEIGHT_UNIT;
            }

            vertex_ptrs[idx] = emit_ground_quad_at_height(
                vertex_ptrs[idx], x0, z0, x1, z1, ground_y);
        }
    }

    // Create GPU buffers for ground batches
    mr->ground_batch_count = 0;
    for (int i = 0; i < map->tile_def_count && i < MAX_TILE_TEXTURES; i++) {
        if (tile_counts[i] > 0 && vertices[i]) {
            int num_verts = tile_counts[i] * 6;

            ground_batch *batch = &mr->ground_batches[mr->ground_batch_count++];
            batch->vertex_count = num_verts;
            batch->texture = mr->tile_textures[i];

            pz_buffer_desc desc = {
                .type = PZ_BUFFER_VERTEX,
                .usage = PZ_BUFFER_STATIC,
                .data = vertices[i],
                .size = num_verts * 5 * sizeof(float),
            };
            batch->buffer = pz_renderer_create_buffer(mr->renderer, &desc);

            pz_free(vertices[i]);
        }
    }

    // Generate wall mesh (includes both positive walls and pit walls)
    if (mr->wall_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(mr->renderer, mr->wall_buffer);
        mr->wall_buffer = PZ_INVALID_HANDLE;
    }
    mr->wall_vertex_count = 0;

    // Count wall faces (height > 0) and pit faces (height < 0)
    int total_wall_faces = 0;
    int total_pit_faces = 0;
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int8_t h = pz_map_get_height(map, x, y);
            if (h > 0) {
                float height = h * WALL_HEIGHT_UNIT;
                total_wall_faces += count_wall_faces(x, y, height, map);
            } else if (h < 0) {
                total_pit_faces += count_pit_faces(x, y, map);
            }
        }
    }

    int total_faces = total_wall_faces + total_pit_faces;
    if (total_faces > 0) {
        int total_verts = total_faces * 6;
        float *wall_verts
            = pz_alloc(total_verts * WALL_VERTEX_SIZE * sizeof(float));
        float *wall_ptr = wall_verts;

        // Generate positive walls
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                int8_t h = pz_map_get_height(map, x, y);
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

        // Generate pit walls
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                int8_t h = pz_map_get_height(map, x, y);
                if (h < 0) {
                    float depth = -h * WALL_HEIGHT_UNIT;

                    float x0 = x * map->tile_size - half_w;
                    float x1 = (x + 1) * map->tile_size - half_w;
                    float z0 = y * map->tile_size - half_h;
                    float z1 = (y + 1) * map->tile_size - half_h;

                    wall_ptr = emit_pit_box(
                        wall_ptr, x0, z0, x1, z1, depth, x, y, map);
                }
            }
        }

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

    // Generate water mesh (for tiles at or below water_level)
    if (mr->water_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(mr->renderer, mr->water_buffer);
        mr->water_buffer = PZ_INVALID_HANDLE;
    }
    mr->water_vertex_count = 0;

    if (map->has_water) {
        // Count water tiles (tiles BELOW water level - submerged areas)
        // Only tiles strictly below water_level get water rendered
        int water_tile_count = 0;
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                int8_t h = pz_map_get_height(map, x, y);
                if (h < map->water_level) {
                    water_tile_count++;
                }
            }
        }

        if (water_tile_count > 0) {
            int num_verts = water_tile_count * 6;
            float *water_verts = pz_alloc(num_verts * 5 * sizeof(float));
            float *water_ptr = water_verts;

            // Water surface Y position: at the water_level height, offset down
            // to create a visible rim/inset effect around the water
            float water_y = GROUND_Y_OFFSET
                + map->water_level * WALL_HEIGHT_UNIT + WATER_Y_OFFSET;

            for (int y = 0; y < map->height; y++) {
                for (int x = 0; x < map->width; x++) {
                    int8_t h = pz_map_get_height(map, x, y);
                    if (h < map->water_level) {
                        float x0 = x * map->tile_size - half_w;
                        float x1 = (x + 1) * map->tile_size - half_w;
                        float z0 = y * map->tile_size - half_h;
                        float z1 = (y + 1) * map->tile_size - half_h;

                        water_ptr = emit_water_quad(
                            water_ptr, x0, z0, x1, z1, water_y);
                    }
                }
            }

            mr->water_vertex_count = num_verts;

            pz_buffer_desc desc = {
                .type = PZ_BUFFER_VERTEX,
                .usage = PZ_BUFFER_STATIC,
                .data = water_verts,
                .size = num_verts * 5 * sizeof(float),
            };
            mr->water_buffer = pz_renderer_create_buffer(mr->renderer, &desc);

            pz_free(water_verts);
        }
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER,
        "Map mesh generated: %d ground batches, %d wall verts, %d water verts",
        mr->ground_batch_count, mr->wall_vertex_count, mr->water_vertex_count);
}

void
pz_map_renderer_draw_ground(pz_map_renderer *mr, const pz_mat4 *view_projection,
    const pz_map_render_params *params)
{
    if (!mr || !mr->map) {
        return;
    }

    pz_renderer_set_uniform_mat4(
        mr->renderer, mr->ground_shader, "u_mvp", view_projection);
    pz_renderer_set_uniform_int(
        mr->renderer, mr->ground_shader, "u_texture", 0);

    // Track texture
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

    // Light map
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

    // Sun lighting
    if (params && params->has_sun) {
        pz_renderer_set_uniform_int(
            mr->renderer, mr->ground_shader, "u_has_sun", 1);
        pz_renderer_set_uniform_vec3(mr->renderer, mr->ground_shader,
            "u_sun_direction", params->sun_direction);
        pz_renderer_set_uniform_vec3(
            mr->renderer, mr->ground_shader, "u_sun_color", params->sun_color);
    } else {
        pz_renderer_set_uniform_int(
            mr->renderer, mr->ground_shader, "u_has_sun", 0);
    }

    // Draw all ground batches
    for (int i = 0; i < mr->ground_batch_count; i++) {
        ground_batch *batch = &mr->ground_batches[i];

        if (batch->vertex_count > 0 && batch->buffer != PZ_INVALID_HANDLE
            && batch->texture != PZ_INVALID_HANDLE) {

            pz_renderer_bind_texture(mr->renderer, 0, batch->texture);

            pz_draw_cmd cmd = {
                .pipeline = mr->ground_pipeline,
                .vertex_buffer = batch->buffer,
                .vertex_count = batch->vertex_count,
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

    pz_mat4 model = pz_mat4_identity();

    pz_renderer_set_uniform_mat4(
        mr->renderer, mr->wall_shader, "u_mvp", view_projection);
    pz_renderer_set_uniform_mat4(
        mr->renderer, mr->wall_shader, "u_model", &model);

    pz_vec3 light_dir = { 0.4f, 0.8f, 0.3f };
    pz_vec3 light_color = { 0.6f, 0.6f, 0.55f };
    pz_vec3 ambient = { 0.15f, 0.15f, 0.18f };

    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->wall_shader, "u_light_dir", light_dir);
    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->wall_shader, "u_light_color", light_color);
    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->wall_shader, "u_ambient", ambient);

    // Light map
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

    // Sun lighting
    if (params && params->has_sun) {
        pz_renderer_set_uniform_int(
            mr->renderer, mr->wall_shader, "u_has_sun", 1);
        pz_renderer_set_uniform_vec3(mr->renderer, mr->wall_shader,
            "u_sun_direction", params->sun_direction);
        pz_renderer_set_uniform_vec3(
            mr->renderer, mr->wall_shader, "u_sun_color", params->sun_color);
    } else {
        pz_renderer_set_uniform_int(
            mr->renderer, mr->wall_shader, "u_has_sun", 0);
    }

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

static void
pz_map_renderer_draw_water(pz_map_renderer *mr, const pz_mat4 *view_projection,
    const pz_map_render_params *params)
{
    if (!mr || !mr->map || mr->water_vertex_count == 0) {
        return;
    }

    pz_renderer_set_uniform_mat4(
        mr->renderer, mr->water_shader, "u_mvp", view_projection);

    // Time for animation
    float time = params ? params->time : 0.0f;
    pz_renderer_set_uniform_float(
        mr->renderer, mr->water_shader, "u_time", time);

    // Water colors - calculate dark and highlight from base color
    pz_vec3 base_color = mr->map->water_color;
    pz_vec3 dark_color = pz_color_darken(base_color, 0.6f);
    pz_vec3 highlight_color = pz_color_lighten(base_color, 0.5f);

    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->water_shader, "u_water_color", base_color);
    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->water_shader, "u_water_dark", dark_color);
    pz_renderer_set_uniform_vec3(
        mr->renderer, mr->water_shader, "u_water_highlight", highlight_color);

    // Caustic texture
    if (mr->water_caustic_texture != PZ_INVALID_HANDLE) {
        pz_renderer_bind_texture(mr->renderer, 1, mr->water_caustic_texture);
    }

    // Light map
    if (params && params->light_texture != PZ_INVALID_HANDLE
        && params->light_texture != 0) {
        pz_renderer_bind_texture(mr->renderer, 0, params->light_texture);
        pz_renderer_set_uniform_int(
            mr->renderer, mr->water_shader, "u_water_light_texture", 0);
        pz_renderer_set_uniform_int(
            mr->renderer, mr->water_shader, "u_use_lighting", 1);
        pz_renderer_set_uniform_vec2(mr->renderer, mr->water_shader,
            "u_light_scale",
            (pz_vec2) { params->light_scale_x, params->light_scale_z });
        pz_renderer_set_uniform_vec2(mr->renderer, mr->water_shader,
            "u_light_offset",
            (pz_vec2) { params->light_offset_x, params->light_offset_z });
    } else {
        pz_renderer_set_uniform_int(
            mr->renderer, mr->water_shader, "u_use_lighting", 0);
    }

    pz_draw_cmd cmd = {
        .pipeline = mr->water_pipeline,
        .vertex_buffer = mr->water_buffer,
        .vertex_count = mr->water_vertex_count,
    };
    pz_renderer_draw(mr->renderer, &cmd);
}

void
pz_map_renderer_draw(pz_map_renderer *mr, const pz_mat4 *view_projection,
    const pz_map_render_params *params)
{
    pz_map_renderer_draw_walls(mr, view_projection, params);
    pz_map_renderer_draw_ground(mr, view_projection, params);
    pz_map_renderer_draw_water(mr, view_projection, params);
}

void
pz_map_renderer_check_hot_reload(pz_map_renderer *mr)
{
    (void)mr;
}

// ============================================================================
// Map Hot-Reload
// ============================================================================

struct pz_map_hot_reload {
    char *path;
    pz_map **map_ptr;
    pz_map_renderer *renderer;
    int64_t last_mtime;
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
    if (mtime == 0 || mtime == hr->last_mtime) {
        return false;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Reloading map: %s", hr->path);

    pz_map *new_map = pz_map_load(hr->path);
    if (!new_map) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to reload map: %s",
            hr->path);
        hr->last_mtime = mtime;
        return false;
    }

    if (*hr->map_ptr) {
        pz_map_destroy(*hr->map_ptr);
    }

    *hr->map_ptr = new_map;
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
