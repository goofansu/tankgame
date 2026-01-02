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
    v = emit_quad(v, -hw, -hh, -hd, -hw, -hh, hd, hw, -hh, hd, hw, -hh, -hd, 0,
        -1, 0, 0, 1, 1, 0);

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
    float hh = body_height * 0.5f; // Half height

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

    // Barrel dimensions
    float barrel_length = 1.2f;
    float barrel_radius = 0.06f;
    float barrel_y = base_height * 0.7f; // Barrel height on turret

    // Half dimensions
    float hw = base_width * 0.5f;
    float hd = base_depth * 0.5f;
    float hh = base_height * 0.5f;

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
 * Small elongated capsule/bullet shape
 * ============================================================================
 */

pz_mesh *
pz_mesh_create_projectile(void)
{
    pz_mesh *mesh = pz_mesh_create();
    mesh->vertices = pz_alloc(128 * sizeof(pz_mesh_vertex));

    // Projectile dimensions
    float length = 0.3f;
    float radius = 0.05f;

    float hl = length * 0.5f;

    pz_mesh_vertex *v = mesh->vertices;

    // Create 6-sided cylinder
    int sides = 6;
    for (int i = 0; i < sides; i++) {
        float angle0 = (float)i / sides * 6.28318f;
        float angle1 = (float)(i + 1) / sides * 6.28318f;

        float c0 = cosf(angle0);
        float s0 = sinf(angle0);
        float c1 = cosf(angle1);
        float s1 = sinf(angle1);

        float x0 = radius * c0;
        float y0 = radius * s0;
        float x1 = radius * c1;
        float y1 = radius * s1;

        float nx = (c0 + c1) * 0.5f;
        float ny = (s0 + s1) * 0.5f;

        // Side face
        v = emit_quad(v, x0, y0, -hl, x0, y0, hl, x1, y1, hl, x1, y1, -hl, nx,
            ny, 0, 0, 1, 1, 0);

        // Front cap triangle
        pz_mesh_vertex *vt = v;
        vt->x = 0;
        vt->y = 0;
        vt->z = hl;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = 1;
        vt->u = 0.5f;
        vt->v = 0.5f;
        v++;

        vt = v;
        vt->x = x0;
        vt->y = y0;
        vt->z = hl;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = 1;
        vt->u = 0.5f + c0 * 0.5f;
        vt->v = 0.5f + s0 * 0.5f;
        v++;

        vt = v;
        vt->x = x1;
        vt->y = y1;
        vt->z = hl;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = 1;
        vt->u = 0.5f + c1 * 0.5f;
        vt->v = 0.5f + s1 * 0.5f;
        v++;

        // Back cap triangle
        vt = v;
        vt->x = 0;
        vt->y = 0;
        vt->z = -hl;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = -1;
        vt->u = 0.5f;
        vt->v = 0.5f;
        v++;

        vt = v;
        vt->x = x1;
        vt->y = y1;
        vt->z = -hl;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = -1;
        vt->u = 0.5f + c1 * 0.5f;
        vt->v = 0.5f + s1 * 0.5f;
        v++;

        vt = v;
        vt->x = x0;
        vt->y = y0;
        vt->z = -hl;
        vt->nx = 0;
        vt->ny = 0;
        vt->nz = -1;
        vt->u = 0.5f + c0 * 0.5f;
        vt->v = 0.5f + s0 * 0.5f;
        v++;
    }

    mesh->vertex_count = (int)(v - mesh->vertices);

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Projectile mesh: %d vertices",
        mesh->vertex_count);

    return mesh;
}

#include <math.h>
