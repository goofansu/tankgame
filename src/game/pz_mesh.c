/*
 * Tank Game - Simple 3D Mesh System Implementation
 */

#include "pz_mesh.h"

#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

/* ============================================================================
 * Mesh API Implementation
 * ============================================================================
 */

pz_mesh *
pz_mesh_create(void)
{
    pz_mesh *mesh = pz_alloc(sizeof(pz_mesh));
    mesh->vertices = NULL;
    mesh->vertex_count = 0;
    mesh->buffer = PZ_INVALID_HANDLE;
    mesh->uploaded = false;
    return mesh;
}

pz_mesh *
pz_mesh_create_from_data(const pz_mesh_vertex *vertices, int count)
{
    pz_mesh *mesh = pz_mesh_create();
    if (count > 0 && vertices) {
        size_t size = count * sizeof(pz_mesh_vertex);
        mesh->vertices = pz_alloc(size);
        memcpy(mesh->vertices, vertices, size);
        mesh->vertex_count = count;
    }
    return mesh;
}

void
pz_mesh_destroy(pz_mesh *mesh, pz_renderer *renderer)
{
    if (!mesh)
        return;

    if (mesh->buffer != PZ_INVALID_HANDLE && renderer) {
        pz_renderer_destroy_buffer(renderer, mesh->buffer);
    }

    if (mesh->vertices) {
        pz_free(mesh->vertices);
    }

    pz_free(mesh);
}

void
pz_mesh_upload(pz_mesh *mesh, pz_renderer *renderer)
{
    if (!mesh || !renderer || mesh->vertex_count == 0)
        return;

    if (mesh->uploaded) {
        // Already uploaded - update existing buffer
        pz_renderer_update_buffer(renderer, mesh->buffer, 0, mesh->vertices,
            mesh->vertex_count * sizeof(pz_mesh_vertex));
        return;
    }

    // Create new buffer
    pz_buffer_desc desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STATIC,
        .data = mesh->vertices,
        .size = mesh->vertex_count * sizeof(pz_mesh_vertex),
    };

    mesh->buffer = pz_renderer_create_buffer(renderer, &desc);
    if (mesh->buffer != PZ_INVALID_HANDLE) {
        mesh->uploaded = true;
    } else {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to upload mesh buffer");
    }
}

/* ============================================================================
 * Vertex Layout
 * ============================================================================
 */

static pz_vertex_attr mesh_attrs[] = {
    { "a_position", PZ_ATTR_FLOAT3, offsetof(pz_mesh_vertex, x) },
    { "a_normal", PZ_ATTR_FLOAT3, offsetof(pz_mesh_vertex, nx) },
    { "a_texcoord", PZ_ATTR_FLOAT2, offsetof(pz_mesh_vertex, u) },
};

pz_vertex_layout
pz_mesh_get_vertex_layout(void)
{
    return (pz_vertex_layout) {
        .attrs = mesh_attrs,
        .attr_count = 3,
        .stride = sizeof(pz_mesh_vertex),
    };
}

/* ============================================================================
 * Mesh Generator Helpers
 * ============================================================================
 */

// Emit a quad face (2 triangles, 6 vertices) with given corners and normal
// Vertices are wound CCW for front-facing
static pz_mesh_vertex *
emit_quad(pz_mesh_vertex *v, float x0, float y0, float z0, float x1, float y1,
    float z1, float x2, float y2, float z2, float x3, float y3, float z3,
    float nx, float ny, float nz, float u0, float v0_tex, float u1,
    float v1_tex)
{
    // Triangle 1: v0, v1, v2
    v->x = x0;
    v->y = y0;
    v->z = z0;
    v->nx = nx;
    v->ny = ny;
    v->nz = nz;
    v->u = u0;
    v->v = v0_tex;
    v++;

    v->x = x1;
    v->y = y1;
    v->z = z1;
    v->nx = nx;
    v->ny = ny;
    v->nz = nz;
    v->u = u0;
    v->v = v1_tex;
    v++;

    v->x = x2;
    v->y = y2;
    v->z = z2;
    v->nx = nx;
    v->ny = ny;
    v->nz = nz;
    v->u = u1;
    v->v = v1_tex;
    v++;

    // Triangle 2: v0, v2, v3
    v->x = x0;
    v->y = y0;
    v->z = z0;
    v->nx = nx;
    v->ny = ny;
    v->nz = nz;
    v->u = u0;
    v->v = v0_tex;
    v++;

    v->x = x2;
    v->y = y2;
    v->z = z2;
    v->nx = nx;
    v->ny = ny;
    v->nz = nz;
    v->u = u1;
    v->v = v1_tex;
    v++;

    v->x = x3;
    v->y = y3;
    v->z = z3;
    v->nx = nx;
    v->ny = ny;
    v->nz = nz;
    v->u = u1;
    v->v = v0_tex;
    v++;

    return v;
}

/* ============================================================================
 * Box/Cube Generation
 * ============================================================================
 */

pz_mesh *
pz_mesh_create_cube(void)
{
    return pz_mesh_create_box(1.0f, 1.0f, 1.0f);
}

pz_mesh *
pz_mesh_create_box(float width, float height, float depth)
{
    // 6 faces * 2 triangles * 3 vertices = 36 vertices
    pz_mesh *mesh = pz_mesh_create();
    mesh->vertex_count = 36;
    mesh->vertices = pz_alloc(36 * sizeof(pz_mesh_vertex));

    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float hd = depth * 0.5f;

    pz_mesh_vertex *v = mesh->vertices;

    // Front face (+Z)
    v = emit_quad(v, -hw, -hh, hd, -hw, hh, hd, hw, hh, hd, hw, -hh, hd, 0, 0,
        1, 0, 1, 1, 0);

    // Back face (-Z)
    v = emit_quad(v, hw, -hh, -hd, hw, hh, -hd, -hw, hh, -hd, -hw, -hh, -hd, 0,
        0, -1, 0, 1, 1, 0);

    // Right face (+X)
    v = emit_quad(v, hw, -hh, hd, hw, hh, hd, hw, hh, -hd, hw, -hh, -hd, 1, 0,
        0, 0, 1, 1, 0);

    // Left face (-X)
    v = emit_quad(v, -hw, -hh, -hd, -hw, hh, -hd, -hw, hh, hd, -hw, -hh, hd, -1,
        0, 0, 0, 1, 1, 0);

    // Top face (+Y)
    v = emit_quad(v, -hw, hh, hd, -hw, hh, -hd, hw, hh, -hd, hw, hh, hd, 0, 1,
        0, 0, 1, 1, 0);

    // Bottom face (-Y)
    (void)emit_quad(v, -hw, -hh, -hd, -hw, -hh, hd, hw, -hh, hd, hw, -hh, -hd,
        0, -1, 0, 0, 1, 1, 0);

    return mesh;
}

/* ============================================================================
 * Tank Body Mesh
 *
 * Tank body dimensions (in world units):
 * - Length (Z): 2.0  (front to back)
 * - Width (X): 1.4   (left to right)
 * - Height (Y): 0.6  (body height)
 *
 * Features:
 * - Main body box
 * - Track housings on sides
 * - Slightly sloped front
 * ============================================================================
 */

pz_mesh *
pz_mesh_create_tank_body(void)
{
    // Allocate generously - we'll count actual vertices
    pz_mesh *mesh = pz_mesh_create();
    mesh->vertices = pz_alloc(256 * sizeof(pz_mesh_vertex));

    // Tank body dimensions
    float body_length = 2.0f;
    float body_width = 1.4f;
    float body_height = 0.6f;

    // Track housing dimensions
    float track_width = 0.2f;
    float track_height = 0.35f;

    // Half dimensions for centered positioning
    float hl = body_length * 0.5f; // Half length
    float hw = body_width * 0.5f; // Half width (full body)

    // Inner body (between tracks)
    float inner_hw = hw - track_width;

    // Track outer edge
    float track_outer = hw;

    pz_mesh_vertex *v = mesh->vertices;

    // ========================================================================
    // Main body (central part, sits on top of tracks)
    // ========================================================================

    float body_base = track_height * 0.5f; // Body sits on track housings
    float body_top = body_base + body_height;

    // Front slope: front of body angles down slightly
    float front_slope = 0.15f;
    float front_top = body_top - front_slope;

    // Top face (flat back portion + sloped front)
    // Back top (flat)
    v = emit_quad(v, -inner_hw, body_top, -hl, -inner_hw, body_top, 0, inner_hw,
        body_top, 0, inner_hw, body_top, -hl, 0, 1, 0, 0, 1, 1, 0);

    // Front top (sloped)
    float slope_nx = 0, slope_ny = 0.98f, slope_nz = 0.2f; // Approximate normal
    v = emit_quad(v, -inner_hw, body_top, 0, -inner_hw, front_top, hl, inner_hw,
        front_top, hl, inner_hw, body_top, 0, slope_nx, slope_ny, slope_nz, 0,
        1, 1, 0);

    // Bottom face
    v = emit_quad(v, -inner_hw, body_base, -hl, inner_hw, body_base, -hl,
        inner_hw, body_base, hl, -inner_hw, body_base, hl, 0, -1, 0, 0, 1, 1,
        0);

    // Front face
    v = emit_quad(v, -inner_hw, body_base, hl, inner_hw, body_base, hl,
        inner_hw, front_top, hl, -inner_hw, front_top, hl, 0, 0, 1, 0, 1, 1, 0);

    // Back face
    v = emit_quad(v, inner_hw, body_base, -hl, -inner_hw, body_base, -hl,
        -inner_hw, body_top, -hl, inner_hw, body_top, -hl, 0, 0, -1, 0, 1, 1,
        0);

    // Left side
    v = emit_quad(v, -inner_hw, body_base, -hl, -inner_hw, body_base, hl,
        -inner_hw, front_top, hl, -inner_hw, body_top, -hl, -1, 0, 0, 0, 1, 1,
        0);
    // Left side upper triangle (connect slope)
    v = emit_quad(v, -inner_hw, body_top, -hl, -inner_hw, front_top, hl,
        -inner_hw, body_top, 0, -inner_hw, body_top, -hl, -1, 0, 0, 0, 0.5f,
        0.5f, 0);

    // Right side
    v = emit_quad(v, inner_hw, body_base, hl, inner_hw, body_base, -hl,
        inner_hw, body_top, -hl, inner_hw, front_top, hl, 1, 0, 0, 0, 1, 1, 0);
    // Right side upper triangle
    v = emit_quad(v, inner_hw, front_top, hl, inner_hw, body_top, -hl, inner_hw,
        body_top, 0, inner_hw, front_top, hl, 1, 0, 0, 0, 0.5f, 0.5f, 0);

    // ========================================================================
    // Track housings (left and right)
    // ========================================================================

    float track_base = 0;
    float track_top = track_height;

    // Left track housing
    float left_inner = -inner_hw;
    float left_outer = -track_outer;

    // Top
    v = emit_quad(v, left_outer, track_top, -hl, left_outer, track_top, hl,
        left_inner, track_top, hl, left_inner, track_top, -hl, 0, 1, 0, 0, 1, 1,
        0);
    // Outer side
    v = emit_quad(v, left_outer, track_base, hl, left_outer, track_base, -hl,
        left_outer, track_top, -hl, left_outer, track_top, hl, -1, 0, 0, 0, 1,
        1, 0);
    // Front
    v = emit_quad(v, left_inner, track_base, hl, left_outer, track_base, hl,
        left_outer, track_top, hl, left_inner, track_top, hl, 0, 0, 1, 0, 1, 1,
        0);
    // Back
    v = emit_quad(v, left_outer, track_base, -hl, left_inner, track_base, -hl,
        left_inner, track_top, -hl, left_outer, track_top, -hl, 0, 0, -1, 0, 1,
        1, 0);
    // Bottom
    v = emit_quad(v, left_outer, track_base, -hl, left_outer, track_base, hl,
        left_inner, track_base, hl, left_inner, track_base, -hl, 0, -1, 0, 0, 1,
        1, 0);

    // Right track housing
    float right_inner = inner_hw;
    float right_outer = track_outer;

    // Top
    v = emit_quad(v, right_inner, track_top, -hl, right_inner, track_top, hl,
        right_outer, track_top, hl, right_outer, track_top, -hl, 0, 1, 0, 0, 1,
        1, 0);
    // Outer side
    v = emit_quad(v, right_outer, track_base, -hl, right_outer, track_base, hl,
        right_outer, track_top, hl, right_outer, track_top, -hl, 1, 0, 0, 0, 1,
        1, 0);
    // Front
    v = emit_quad(v, right_outer, track_base, hl, right_inner, track_base, hl,
        right_inner, track_top, hl, right_outer, track_top, hl, 0, 0, 1, 0, 1,
        1, 0);
    // Back
    v = emit_quad(v, right_inner, track_base, -hl, right_outer, track_base, -hl,
        right_outer, track_top, -hl, right_inner, track_top, -hl, 0, 0, -1, 0,
        1, 1, 0);
    // Bottom
    v = emit_quad(v, right_inner, track_base, -hl, right_inner, track_base, hl,
        right_outer, track_base, hl, right_outer, track_base, -hl, 0, -1, 0, 0,
        1, 1, 0);

    // Calculate actual vertex count
    mesh->vertex_count = (int)(v - mesh->vertices);

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Tank body mesh: %d vertices",
        mesh->vertex_count);

    return mesh;
}

/* ============================================================================
 * Tank Turret Mesh
 *
 * Turret dimensions:
 * - Base diameter: ~0.8 (octagonal approximation)
 * - Height: 0.35
 * - Barrel length: 1.2
 * - Barrel diameter: 0.12
 *
 * Origin is at the rotation center (bottom center of turret base)
 * Barrel points in +Z direction
 * ============================================================================
 */

pz_mesh *
pz_mesh_create_tank_turret(void)
{
    pz_mesh *mesh = pz_mesh_create();
    mesh->vertices = pz_alloc(256 * sizeof(pz_mesh_vertex));

    // Turret base (simplified as a box for now)
    float base_width = 0.8f;
    float base_depth = 0.9f;
    float base_height = 0.35f;

    // Barrel dimensions (thicker to hold large projectiles)
    float barrel_length = 1.2f;
    float barrel_radius = 0.18f; // Much thicker barrel
    float barrel_y
        = base_height + barrel_radius; // Barrel sits on top of turret

    // Half dimensions
    float hw = base_width * 0.5f;
    float hd = base_depth * 0.5f;

    pz_mesh_vertex *v = mesh->vertices;

    // ========================================================================
    // Turret base (box)
    // ========================================================================

    // Top
    v = emit_quad(v, -hw, base_height, -hd, -hw, base_height, hd, hw,
        base_height, hd, hw, base_height, -hd, 0, 1, 0, 0, 1, 1, 0);

    // Bottom
    v = emit_quad(v, -hw, 0, -hd, hw, 0, -hd, hw, 0, hd, -hw, 0, hd, 0, -1, 0,
        0, 1, 1, 0);

    // Front
    v = emit_quad(v, -hw, 0, hd, hw, 0, hd, hw, base_height, hd, -hw,
        base_height, hd, 0, 0, 1, 0, 1, 1, 0);

    // Back
    v = emit_quad(v, hw, 0, -hd, -hw, 0, -hd, -hw, base_height, -hd, hw,
        base_height, -hd, 0, 0, -1, 0, 1, 1, 0);

    // Left
    v = emit_quad(v, -hw, 0, -hd, -hw, 0, hd, -hw, base_height, hd, -hw,
        base_height, -hd, -1, 0, 0, 0, 1, 1, 0);

    // Right
    v = emit_quad(v, hw, 0, hd, hw, 0, -hd, hw, base_height, -hd, hw,
        base_height, hd, 1, 0, 0, 0, 1, 1, 0);

    // ========================================================================
    // Barrel (hexagonal prism for simplicity - 6 sides)
    // ========================================================================

    float barrel_start_z = hd;
    float barrel_end_z = hd + barrel_length;

    // Create 8-sided barrel (octagon)
    int barrel_sides = 8;
    for (int i = 0; i < barrel_sides; i++) {
        float angle0 = (float)i / barrel_sides * 6.28318f;
        float angle1 = (float)(i + 1) / barrel_sides * 6.28318f;

        float c0 = cosf(angle0);
        float s0 = sinf(angle0);
        float c1 = cosf(angle1);
        float s1 = sinf(angle1);

        float x0 = barrel_radius * c0;
        float y0 = barrel_y + barrel_radius * s0;
        float x1 = barrel_radius * c1;
        float y1 = barrel_y + barrel_radius * s1;

        // Normal for this face (average of two corner normals)
        float nx = (c0 + c1) * 0.5f;
        float ny = (s0 + s1) * 0.5f;

        // Barrel side face
        v = emit_quad(v, x0, y0, barrel_start_z, x0, y0, barrel_end_z, x1, y1,
            barrel_end_z, x1, y1, barrel_start_z, nx, ny, 0, 0, 1, 1, 0);
    }

    // Barrel front cap (end of barrel)
    // Simplified as a few triangles
    for (int i = 0; i < barrel_sides; i++) {
        float angle0 = (float)i / barrel_sides * 6.28318f;
        float angle1 = (float)(i + 1) / barrel_sides * 6.28318f;

        float x0 = barrel_radius * cosf(angle0);
        float y0 = barrel_y + barrel_radius * sinf(angle0);
        float x1 = barrel_radius * cosf(angle1);
        float y1 = barrel_y + barrel_radius * sinf(angle1);

        // Triangle fan from center
        pz_mesh_vertex *vt = v;
        vt->x = 0;
        vt->y = barrel_y;
        vt->z = barrel_end_z;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = 1;
        vt->u = 0.5f;
        vt->v = 0.5f;
        v++;

        vt = v;
        vt->x = x0;
        vt->y = y0;
        vt->z = barrel_end_z;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = 1;
        vt->u = 0.5f + cosf(angle0) * 0.5f;
        vt->v = 0.5f + sinf(angle0) * 0.5f;
        v++;

        vt = v;
        vt->x = x1;
        vt->y = y1;
        vt->z = barrel_end_z;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = 1;
        vt->u = 0.5f + cosf(angle1) * 0.5f;
        vt->v = 0.5f + sinf(angle1) * 0.5f;
        v++;
    }

    mesh->vertex_count = (int)(v - mesh->vertices);

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Tank turret mesh: %d vertices",
        mesh->vertex_count);

    return mesh;
}

/* ============================================================================
 * Projectile Mesh
 *
 * Bullet shape: cylinder body with spherical nose and flat back
 * ============================================================================
 */

pz_mesh *
pz_mesh_create_projectile(void)
{
    pz_mesh *mesh = pz_mesh_create();
    mesh->vertices = pz_alloc(512 * sizeof(pz_mesh_vertex));

    // Projectile dimensions
    float radius = 0.2f;
    float body_length = 0.4f; // Cylinder part
    float nose_length = 0.3f; // Spherical nose part

    // Body goes from z=0 to z=-body_length (back)
    // Nose goes from z=0 to z=+nose_length (front, tapered)

    pz_mesh_vertex *v = mesh->vertices;

    int sides = 12; // Smoother bullet
    int nose_rings = 4; // Rings for the nose sphere

    for (int i = 0; i < sides; i++) {
        float angle0 = (float)i / sides * 2.0f * PZ_PI;
        float angle1 = (float)(i + 1) / sides * 2.0f * PZ_PI;

        float c0 = cosf(angle0);
        float s0 = sinf(angle0);
        float c1 = cosf(angle1);
        float s1 = sinf(angle1);

        float x0 = radius * c0;
        float y0 = radius * s0;
        float x1 = radius * c1;
        float y1 = radius * s1;

        // ====================================================================
        // Cylinder body (from z=0 to z=-body_length)
        // ====================================================================
        float nx = (c0 + c1) * 0.5f;
        float ny = (s0 + s1) * 0.5f;

        v = emit_quad(v, x0, y0, 0, // front-left
            x0, y0, -body_length, // back-left
            x1, y1, -body_length, // back-right
            x1, y1, 0, // front-right
            nx, ny, 0, 0, 1, 1, 0);

        // ====================================================================
        // Back cap (flat, at z=-body_length)
        // ====================================================================
        pz_mesh_vertex *vt = v;
        vt->x = 0;
        vt->y = 0;
        vt->z = -body_length;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = -1;
        vt->u = 0.5f;
        vt->v = 0.5f;
        v++;

        vt = v;
        vt->x = x1;
        vt->y = y1;
        vt->z = -body_length;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = -1;
        vt->u = 0.5f + c1 * 0.5f;
        vt->v = 0.5f + s1 * 0.5f;
        v++;

        vt = v;
        vt->x = x0;
        vt->y = y0;
        vt->z = -body_length;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = -1;
        vt->u = 0.5f + c0 * 0.5f;
        vt->v = 0.5f + s0 * 0.5f;
        v++;

        // ====================================================================
        // Spherical nose (from z=0 to z=+nose_length)
        // ====================================================================
        for (int r = 0; r < nose_rings; r++) {
            // Latitude angles (0 = equator at z=0, PI/2 = tip at z=nose_length)
            float lat0 = (float)r / nose_rings * (PZ_PI * 0.5f);
            float lat1 = (float)(r + 1) / nose_rings * (PZ_PI * 0.5f);

            float cos_lat0 = cosf(lat0);
            float sin_lat0 = sinf(lat0);
            float cos_lat1 = cosf(lat1);
            float sin_lat1 = sinf(lat1);

            // Ring radii
            float r0 = radius * cos_lat0;
            float r1 = radius * cos_lat1;

            // Z positions
            float z0 = nose_length * sin_lat0;
            float z1 = nose_length * sin_lat1;

            // Four corners of quad on sphere
            float qx00 = r0 * c0, qy00 = r0 * s0;
            float qx01 = r0 * c1, qy01 = r0 * s1;
            float qx10 = r1 * c0, qy10 = r1 * s0;
            float qx11 = r1 * c1, qy11 = r1 * s1;

            // Normals point outward from sphere center
            float nx00 = cos_lat0 * c0, ny00 = cos_lat0 * s0, nz00 = sin_lat0;
            float nx01 = cos_lat0 * c1, ny01 = cos_lat0 * s1, nz01 = sin_lat0;
            float nx10 = cos_lat1 * c0, ny10 = cos_lat1 * s0, nz10 = sin_lat1;
            float nx11 = cos_lat1 * c1, ny11 = cos_lat1 * s1, nz11 = sin_lat1;

            // Emit two triangles for this quad
            // Triangle 1: 00, 10, 11
            vt = v;
            vt->x = qx00;
            vt->y = qy00;
            vt->z = z0;
            vt->nx = nx00;
            vt->ny = ny00;
            vt->nz = nz00;
            vt->u = 0;
            vt->v = 0;
            v++;

            vt = v;
            vt->x = qx10;
            vt->y = qy10;
            vt->z = z1;
            vt->nx = nx10;
            vt->ny = ny10;
            vt->nz = nz10;
            vt->u = 0;
            vt->v = 1;
            v++;

            vt = v;
            vt->x = qx11;
            vt->y = qy11;
            vt->z = z1;
            vt->nx = nx11;
            vt->ny = ny11;
            vt->nz = nz11;
            vt->u = 1;
            vt->v = 1;
            v++;

            // Triangle 2: 00, 11, 01
            vt = v;
            vt->x = qx00;
            vt->y = qy00;
            vt->z = z0;
            vt->nx = nx00;
            vt->ny = ny00;
            vt->nz = nz00;
            vt->u = 0;
            vt->v = 0;
            v++;

            vt = v;
            vt->x = qx11;
            vt->y = qy11;
            vt->z = z1;
            vt->nx = nx11;
            vt->ny = ny11;
            vt->nz = nz11;
            vt->u = 1;
            vt->v = 1;
            v++;

            vt = v;
            vt->x = qx01;
            vt->y = qy01;
            vt->z = z0;
            vt->nx = nx01;
            vt->ny = ny01;
            vt->nz = nz01;
            vt->u = 1;
            vt->v = 0;
            v++;
        }
    }

    mesh->vertex_count = (int)(v - mesh->vertices);

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Projectile mesh: %d vertices",
        mesh->vertex_count);

    return mesh;
}

/* ============================================================================
 * Powerup Mesh
 * ============================================================================
 */

pz_mesh *
pz_mesh_create_powerup(void)
{
    pz_mesh *mesh = pz_mesh_create();

    // Powerup is a small crate/box with a slight bevel
    // Dimensions: 0.6 x 0.4 x 0.6 (width x height x depth)
    float hw = 0.3f; // Half width (X)
    float hh = 0.2f; // Half height (Y)
    float hd = 0.3f; // Half depth (Z)

    // 6 faces * 2 triangles * 3 vertices = 36 vertices
    mesh->vertices = pz_alloc(36 * sizeof(pz_mesh_vertex));
    pz_mesh_vertex *v = mesh->vertices;

    // Top face (+Y)
    v = emit_quad(v, -hw, hh, -hd, -hw, hh, hd, hw, hh, hd, hw, hh, -hd, 0, 1,
        0, 0, 0, 1, 1);

    // Bottom face (-Y)
    v = emit_quad(v, -hw, -hh, hd, -hw, -hh, -hd, hw, -hh, -hd, hw, -hh, hd, 0,
        -1, 0, 0, 0, 1, 1);

    // Front face (+Z)
    v = emit_quad(v, -hw, -hh, hd, hw, -hh, hd, hw, hh, hd, -hw, hh, hd, 0, 0,
        1, 0, 0, 1, 1);

    // Back face (-Z)
    v = emit_quad(v, hw, -hh, -hd, -hw, -hh, -hd, -hw, hh, -hd, hw, hh, -hd, 0,
        0, -1, 0, 0, 1, 1);

    // Right face (+X)
    v = emit_quad(v, hw, -hh, hd, hw, -hh, -hd, hw, hh, -hd, hw, hh, hd, 1, 0,
        0, 0, 0, 1, 1);

    // Left face (-X)
    v = emit_quad(v, -hw, -hh, -hd, -hw, -hh, hd, -hw, hh, hd, -hw, hh, -hd, -1,
        0, 0, 0, 0, 1, 1);

    mesh->vertex_count = (int)(v - mesh->vertices);

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Powerup mesh: %d vertices",
        mesh->vertex_count);

    return mesh;
}

pz_mesh *
pz_mesh_create_mine(void)
{
    pz_mesh *mesh = pz_mesh_create();

    // Mine is a dome (half-sphere) sitting on a flat bottom
    float radius = 0.3f;
    int slices = 16; // Around the dome
    int stacks = 8; // Up the dome (half sphere)

    // Calculate vertices needed:
    // Dome: slices * stacks * 6 (2 triangles per quad)
    // Bottom cap: slices * 3
    int dome_verts = slices * stacks * 6;
    int bottom_verts = slices * 3;
    int total_verts = dome_verts + bottom_verts;

    mesh->vertices = pz_alloc(total_verts * sizeof(pz_mesh_vertex));
    pz_mesh_vertex *v = mesh->vertices;

    // Generate dome (upper hemisphere)
    for (int i = 0; i < stacks; i++) {
        // Phi goes from 0 (top) to PI/2 (equator)
        float phi0 = (float)i / stacks * (PZ_PI / 2.0f);
        float phi1 = (float)(i + 1) / stacks * (PZ_PI / 2.0f);

        float y0 = cosf(phi0) * radius;
        float y1 = cosf(phi1) * radius;
        float r0 = sinf(phi0) * radius;
        float r1 = sinf(phi1) * radius;

        for (int j = 0; j < slices; j++) {
            float theta0 = (float)j / slices * 2.0f * PZ_PI;
            float theta1 = (float)(j + 1) / slices * 2.0f * PZ_PI;

            // Four corners of the quad
            float x00 = r0 * cosf(theta0);
            float z00 = r0 * sinf(theta0);
            float x01 = r0 * cosf(theta1);
            float z01 = r0 * sinf(theta1);
            float x10 = r1 * cosf(theta0);
            float z10 = r1 * sinf(theta0);
            float x11 = r1 * cosf(theta1);
            float z11 = r1 * sinf(theta1);

            // Normals point outward (same as position normalized)
            float nx00 = sinf(phi0) * cosf(theta0);
            float ny00 = cosf(phi0);
            float nz00 = sinf(phi0) * sinf(theta0);
            float nx01 = sinf(phi0) * cosf(theta1);
            float ny01 = cosf(phi0);
            float nz01 = sinf(phi0) * sinf(theta1);
            float nx10 = sinf(phi1) * cosf(theta0);
            float ny10 = cosf(phi1);
            float nz10 = sinf(phi1) * sinf(theta0);
            float nx11 = sinf(phi1) * cosf(theta1);
            float ny11 = cosf(phi1);
            float nz11 = sinf(phi1) * sinf(theta1);

            // Triangle 1
            v->x = x00;
            v->y = y0;
            v->z = z00;
            v->nx = nx00;
            v->ny = ny00;
            v->nz = nz00;
            v->u = 0;
            v->v = 0;
            v++;

            v->x = x10;
            v->y = y1;
            v->z = z10;
            v->nx = nx10;
            v->ny = ny10;
            v->nz = nz10;
            v->u = 0;
            v->v = 1;
            v++;

            v->x = x11;
            v->y = y1;
            v->z = z11;
            v->nx = nx11;
            v->ny = ny11;
            v->nz = nz11;
            v->u = 1;
            v->v = 1;
            v++;

            // Triangle 2
            v->x = x00;
            v->y = y0;
            v->z = z00;
            v->nx = nx00;
            v->ny = ny00;
            v->nz = nz00;
            v->u = 0;
            v->v = 0;
            v++;

            v->x = x11;
            v->y = y1;
            v->z = z11;
            v->nx = nx11;
            v->ny = ny11;
            v->nz = nz11;
            v->u = 1;
            v->v = 1;
            v++;

            v->x = x01;
            v->y = y0;
            v->z = z01;
            v->nx = nx01;
            v->ny = ny01;
            v->nz = nz01;
            v->u = 1;
            v->v = 0;
            v++;
        }
    }

    // Bottom cap (flat circle at y=0)
    float bottom_y = cosf(PZ_PI / 2.0f) * radius; // Should be ~0
    float bottom_r = sinf(PZ_PI / 2.0f) * radius; // Should be radius
    for (int j = 0; j < slices; j++) {
        float theta0 = (float)j / slices * 2.0f * PZ_PI;
        float theta1 = (float)(j + 1) / slices * 2.0f * PZ_PI;

        float x0 = bottom_r * cosf(theta0);
        float z0 = bottom_r * sinf(theta0);
        float x1 = bottom_r * cosf(theta1);
        float z1 = bottom_r * sinf(theta1);

        // Center
        v->x = 0;
        v->y = bottom_y;
        v->z = 0;
        v->nx = 0;
        v->ny = -1;
        v->nz = 0;
        v->u = 0.5f;
        v->v = 0.5f;
        v++;

        // Edge vertices (reversed winding for bottom face)
        v->x = x1;
        v->y = bottom_y;
        v->z = z1;
        v->nx = 0;
        v->ny = -1;
        v->nz = 0;
        v->u = 0.5f + cosf(theta1) * 0.5f;
        v->v = 0.5f + sinf(theta1) * 0.5f;
        v++;

        v->x = x0;
        v->y = bottom_y;
        v->z = z0;
        v->nx = 0;
        v->ny = -1;
        v->nz = 0;
        v->u = 0.5f + cosf(theta0) * 0.5f;
        v->v = 0.5f + sinf(theta0) * 0.5f;
        v++;
    }

    mesh->vertex_count = (int)(v - mesh->vertices);

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Mine mesh: %d vertices",
        mesh->vertex_count);

    return mesh;
}
