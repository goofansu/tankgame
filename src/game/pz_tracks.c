/*
 * Tank Track Accumulation System Implementation
 */

#include "pz_tracks.h"

#include <math.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_math.h"
#include "../core/pz_mem.h"

// Maximum number of track marks to batch before rendering
#define MAX_PENDING_MARKS 256

// Track mark dimensions (in world units)
#define TRACK_MARK_LENGTH 0.26f // Length of each track segment
#define TRACK_MARK_WIDTH 0.13f // Width of a single tread mark

// Minimum distance before placing another track mark (must be > LENGTH for
// gaps)
#define TRACK_MIN_DISTANCE 0.39f

// Track mark vertex: position (2) + texcoord (2) = 4 floats
#define TRACK_VERTEX_FLOATS 4
#define TRACK_VERTEX_SIZE (TRACK_VERTEX_FLOATS * sizeof(float))

// Each mark is a quad = 6 vertices (2 triangles)
#define VERTICES_PER_MARK 6

// A single pending track mark
typedef struct track_mark {
    float x, z; // World position
    float angle; // Rotation angle
} track_mark;

struct pz_tracks {
    pz_renderer *renderer;
    pz_texture_manager *tex_manager;

    // World dimensions
    float world_width;
    float world_height;
    int texture_size;

    // Accumulation render target
    pz_render_target_handle render_target;
    pz_texture_handle accumulation_texture; // Retrieved from RT

    // Track mark texture (the stamp we use)
    pz_texture_handle track_texture;

    // Shader and pipeline for rendering tracks
    pz_shader_handle track_shader;
    pz_pipeline_handle track_pipeline;

    // Dynamic vertex buffer for track marks
    pz_buffer_handle vertex_buffer;

    // Pending track marks to render
    track_mark pending_marks[MAX_PENDING_MARKS];
    int pending_count;

    // Last track position (center of tank)
    float last_x, last_z;
    bool has_last_position;

    // Whether we need to clear on next update
    bool needs_clear;
    bool first_update;
};

// ============================================================================
// Creation / Destruction
// ============================================================================

pz_tracks *
pz_tracks_create(pz_renderer *renderer, pz_texture_manager *tex_manager,
    const pz_tracks_config *config)
{
    pz_tracks *tracks = pz_calloc(1, sizeof(pz_tracks));
    if (!tracks)
        return NULL;

    tracks->renderer = renderer;
    tracks->tex_manager = tex_manager;
    tracks->world_width = config->world_width;
    tracks->world_height = config->world_height;
    tracks->texture_size = config->texture_size;
    tracks->first_update = true;

    // Create render target for accumulation
    // Use a simple grayscale format - R8 is enough since tracks are just
    // darkness
    pz_render_target_desc rt_desc = {
        .width = config->texture_size,
        .height = config->texture_size,
        .color_format = PZ_TEXTURE_RGBA8, // Use RGBA for simplicity
        .has_depth = false,
    };
    tracks->render_target
        = pz_renderer_create_render_target(renderer, &rt_desc);
    if (tracks->render_target == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
            "Failed to create track accumulation render target");
        pz_free(tracks);
        return NULL;
    }

    // Get the color texture from the render target
    tracks->accumulation_texture = pz_renderer_get_render_target_texture(
        renderer, tracks->render_target);

    // Load track mark texture
    tracks->track_texture
        = pz_texture_load(tex_manager, "assets/textures/track.png");
    if (tracks->track_texture == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Track texture not found, using solid color");
    }

    // Load shader for rendering track marks
    tracks->track_shader = pz_renderer_load_shader(
        renderer, "shaders/track.vert", "shaders/track.frag", "track");
    if (tracks->track_shader == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to load track shader");
        pz_renderer_destroy_render_target(renderer, tracks->render_target);
        pz_free(tracks);
        return NULL;
    }

    // Create pipeline for track rendering (alpha blending to darken)
    pz_vertex_attr attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT2, .offset = 0 },
        { .name = "a_texcoord",
            .type = PZ_ATTR_FLOAT2,
            .offset = 2 * sizeof(float) },
    };
    pz_vertex_layout layout = {
        .attrs = attrs,
        .attr_count = 2,
        .stride = TRACK_VERTEX_SIZE,
    };
    pz_pipeline_desc pipe_desc = {
        .shader = tracks->track_shader,
        .vertex_layout = layout,
        .blend = PZ_BLEND_ALPHA, // Use alpha blending
        .depth = PZ_DEPTH_NONE, // No depth test for 2D texture rendering
        .cull = PZ_CULL_NONE,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    tracks->track_pipeline = pz_renderer_create_pipeline(renderer, &pipe_desc);

    // Create dynamic vertex buffer
    size_t buffer_size
        = MAX_PENDING_MARKS * 2 * VERTICES_PER_MARK * TRACK_VERTEX_SIZE;
    pz_buffer_desc buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_DYNAMIC,
        .data = NULL,
        .size = buffer_size,
    };
    tracks->vertex_buffer = pz_renderer_create_buffer(renderer, &buf_desc);

    tracks->needs_clear = true;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
        "Track system created: %dx%d texture, world %.1fx%.1f",
        config->texture_size, config->texture_size, config->world_width,
        config->world_height);

    return tracks;
}

void
pz_tracks_destroy(pz_tracks *tracks)
{
    if (!tracks)
        return;

    if (tracks->vertex_buffer != PZ_INVALID_HANDLE)
        pz_renderer_destroy_buffer(tracks->renderer, tracks->vertex_buffer);
    if (tracks->track_pipeline != PZ_INVALID_HANDLE)
        pz_renderer_destroy_pipeline(tracks->renderer, tracks->track_pipeline);
    if (tracks->track_shader != PZ_INVALID_HANDLE)
        pz_renderer_destroy_shader(tracks->renderer, tracks->track_shader);
    if (tracks->render_target != PZ_INVALID_HANDLE)
        pz_renderer_destroy_render_target(
            tracks->renderer, tracks->render_target);

    pz_free(tracks);
}

// ============================================================================
// Track Mark Generation
// ============================================================================

// Add a single track mark at the specified position
static void
add_single_mark(pz_tracks *tracks, float x, float z, float angle)
{
    if (tracks->pending_count >= MAX_PENDING_MARKS) {
        // Buffer full, marks will be dropped until next update
        return;
    }

    track_mark *mark = &tracks->pending_marks[tracks->pending_count++];
    mark->x = x;
    mark->z = z;
    mark->angle = angle;
}

void
pz_tracks_add_mark(pz_tracks *tracks, float pos_x, float pos_z, float angle,
    float tread_offset)
{
    if (!tracks)
        return;

    if (!tracks->has_last_position) {
        // First position, just record it
        tracks->last_x = pos_x;
        tracks->last_z = pos_z;
        tracks->has_last_position = true;
        return;
    }

    // Check if tank center has moved far enough
    float dx = pos_x - tracks->last_x;
    float dz = pos_z - tracks->last_z;
    float dist = sqrtf(dx * dx + dz * dz);

    if (dist >= TRACK_MIN_DISTANCE) {
        // Direction of movement
        float move_angle = atan2f(dx, dz);

        // Perpendicular to movement direction (for tread offset)
        float perp_x = cosf(move_angle);
        float perp_z = -sinf(move_angle);

        float left_x = pos_x + perp_x * tread_offset;
        float left_z = pos_z + perp_z * tread_offset;
        float right_x = pos_x - perp_x * tread_offset;
        float right_z = pos_z - perp_z * tread_offset;

        // Add both tread marks oriented along direction of movement
        add_single_mark(tracks, left_x, left_z, move_angle);
        add_single_mark(tracks, right_x, right_z, move_angle);

        tracks->last_x = pos_x;
        tracks->last_z = pos_z;
    }
}

// ============================================================================
// Rendering
// ============================================================================

// Generate vertex data for a single track mark quad
// Returns pointer past the last written float
static float *
emit_track_quad(float *v, float x, float z, float angle, float world_width,
    float world_height)
{
    // Convert world coordinates to UV space (0-1)
    // We'll transform to NDC (-1 to 1) in the shader, but store UV coords

    // Half dimensions
    float hw = TRACK_MARK_WIDTH * 0.5f;
    float hl = TRACK_MARK_LENGTH * 0.5f;

    // Rotation
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    // Four corners of the quad (in local space, then rotated)
    // Local: x along width, z along length
    float corners[4][2] = {
        { -hw, -hl }, // bottom-left
        { -hw, hl }, // top-left
        { hw, hl }, // top-right
        { hw, -hl }, // bottom-right
    };

    // Transform corners to world space
    float world_corners[4][2];
    for (int i = 0; i < 4; i++) {
        float lx = corners[i][0];
        float lz = corners[i][1];
        // Rotate by angle
        float rx = lx * cos_a - lz * sin_a;
        float rz = lx * sin_a + lz * cos_a;
        // Translate to world position
        world_corners[i][0] = x + rx;
        world_corners[i][1] = z + rz;
    }

    // Convert to UV coordinates for the render target
    // World coordinates are centered: (-half_w, -half_h) to (+half_w, +half_h)
    // UV should be (0,0) to (1,1)
    // UV = (world + half) / world_size = world / world_size + 0.5
    float uv_corners[4][2];
    for (int i = 0; i < 4; i++) {
        uv_corners[i][0] = world_corners[i][0] / world_width + 0.5f;
        uv_corners[i][1] = world_corners[i][1] / world_height + 0.5f;
    }

    // Texture coordinates for the track texture
    float tex_coords[4][2] = {
        { 0.0f, 1.0f }, // bottom-left
        { 0.0f, 0.0f }, // top-left
        { 1.0f, 0.0f }, // top-right
        { 1.0f, 1.0f }, // bottom-right
    };

    // Emit two triangles (0,1,2) and (0,2,3)
    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    for (int i = 0; i < 6; i++) {
        int idx = indices[i];
        // Position in UV space (will be converted to NDC in shader)
        *v++ = uv_corners[idx][0];
        *v++ = uv_corners[idx][1];
        // Texture coordinate
        *v++ = tex_coords[idx][0];
        *v++ = tex_coords[idx][1];
    }

    return v;
}

void
pz_tracks_update(pz_tracks *tracks)
{
    if (!tracks)
        return;

    // Bind render target
    pz_renderer_set_render_target(tracks->renderer, tracks->render_target);

    // Clear on first frame (to white - no tracks)
    if (tracks->first_update || tracks->needs_clear) {
        // Clear to white (1,1,1,1) - tracks will darken this
        pz_renderer_clear_color(tracks->renderer, 1.0f, 1.0f, 1.0f, 1.0f);
        tracks->needs_clear = false;
        tracks->first_update = false;
    }

    // If no pending marks, just return
    if (tracks->pending_count == 0) {
        // Reset to default framebuffer
        pz_renderer_set_render_target(tracks->renderer, 0);
        return;
    }

    // Generate vertex data for all pending marks
    size_t vertex_count = tracks->pending_count * VERTICES_PER_MARK;
    size_t buffer_size = vertex_count * TRACK_VERTEX_SIZE;
    float *vertices = pz_alloc(buffer_size);
    if (!vertices) {
        pz_renderer_set_render_target(tracks->renderer, 0);
        return;
    }

    float *v = vertices;
    for (int i = 0; i < tracks->pending_count; i++) {
        track_mark *mark = &tracks->pending_marks[i];
        v = emit_track_quad(v, mark->x, mark->z, mark->angle,
            tracks->world_width, tracks->world_height);
    }

    // Upload to buffer
    pz_renderer_update_buffer(
        tracks->renderer, tracks->vertex_buffer, 0, vertices, buffer_size);
    pz_free(vertices);

    // Set up uniforms
    // u_color: subtle darkening that accumulates with multiple passes
    // RGB = how dark (0 = black, 1 = white), A = opacity per mark
    pz_renderer_set_uniform_vec4(tracks->renderer, tracks->track_shader,
        "u_color", (pz_vec4) { 0.4f, 0.35f, 0.3f, 0.425f });

    // Use solid color rectangles, no texture
    pz_renderer_set_uniform_int(
        tracks->renderer, tracks->track_shader, "u_use_texture", 0);

    // Draw all track marks
    pz_draw_cmd cmd = {
        .pipeline = tracks->track_pipeline,
        .vertex_buffer = tracks->vertex_buffer,
        .index_buffer = PZ_INVALID_HANDLE,
        .vertex_count = vertex_count,
        .index_count = 0,
        .vertex_offset = 0,
        .index_offset = 0,
    };
    pz_renderer_draw(tracks->renderer, &cmd);

    // Clear pending marks
    tracks->pending_count = 0;

    // Reset to default framebuffer
    pz_renderer_set_render_target(tracks->renderer, 0);
}

// ============================================================================
// Accessors
// ============================================================================

pz_texture_handle
pz_tracks_get_texture(pz_tracks *tracks)
{
    if (!tracks)
        return PZ_INVALID_HANDLE;
    return tracks->accumulation_texture;
}

void
pz_tracks_get_uv_transform(pz_tracks *tracks, float *out_scale_x,
    float *out_scale_z, float *out_offset_x, float *out_offset_z)
{
    if (!tracks) {
        *out_scale_x = 1.0f;
        *out_scale_z = 1.0f;
        *out_offset_x = 0.0f;
        *out_offset_z = 0.0f;
        return;
    }

    // World coordinates are centered (range [-half, +half])
    // Need to transform to UV (range [0, 1])
    // UV = (world + half) / world_size = world / world_size + 0.5
    *out_scale_x = 1.0f / tracks->world_width;
    *out_scale_z = 1.0f / tracks->world_height;
    *out_offset_x = 0.5f; // Add half to center
    *out_offset_z = 0.5f;
}

void
pz_tracks_clear(pz_tracks *tracks)
{
    if (!tracks)
        return;

    tracks->pending_count = 0;
    tracks->has_last_position = false;
    tracks->needs_clear = true;
}
