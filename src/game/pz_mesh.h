/*
 * Tank Game - Simple 3D Mesh System
 *
 * Handles mesh data for game entities (tanks, projectiles, etc.)
 */

#ifndef PZ_MESH_H
#define PZ_MESH_H

#include "../engine/render/pz_renderer.h"
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * Mesh Vertex Format
 * ============================================================================
 */

// Standard vertex: position (3) + normal (3) + texcoord (2) = 8 floats
typedef struct pz_mesh_vertex {
    float x, y, z; // Position
    float nx, ny, nz; // Normal
    float u, v; // Texture coordinate
} pz_mesh_vertex;

#define PZ_MESH_VERTEX_SIZE (sizeof(pz_mesh_vertex) / sizeof(float))

/* ============================================================================
 * Mesh Structure
 * ============================================================================
 */

typedef struct pz_mesh {
    pz_mesh_vertex *vertices; // Vertex data (owned)
    int vertex_count;

    pz_buffer_handle buffer; // GPU buffer (created on upload)
    bool uploaded; // True if GPU buffer is valid
} pz_mesh;

/* ============================================================================
 * Mesh API
 * ============================================================================
 */

// Create an empty mesh
pz_mesh *pz_mesh_create(void);

// Create a mesh from vertex data (copies the data)
pz_mesh *pz_mesh_create_from_data(const pz_mesh_vertex *vertices, int count);

// Destroy mesh and free resources
void pz_mesh_destroy(pz_mesh *mesh, pz_renderer *renderer);

// Upload mesh data to GPU (call before rendering)
void pz_mesh_upload(pz_mesh *mesh, pz_renderer *renderer);

/* ============================================================================
 * Built-in Mesh Generators
 * ============================================================================
 */

// Create a unit cube (1x1x1) centered at origin
pz_mesh *pz_mesh_create_cube(void);

// Create a box with specified dimensions, centered at origin
pz_mesh *pz_mesh_create_box(float width, float height, float depth);

// Create tank body mesh
pz_mesh *pz_mesh_create_tank_body(void);

// Create tank turret mesh (barrel included)
pz_mesh *pz_mesh_create_tank_turret(void);

// Create a simple projectile mesh (small elongated shape)
pz_mesh *pz_mesh_create_projectile(void);

// Create a powerup mesh (floating crate/box shape)
pz_mesh *pz_mesh_create_powerup(void);

// Create a mine mesh (flat disc with spikes)
pz_mesh *pz_mesh_create_mine(void);

/* ============================================================================
 * Vertex Layout Helper
 * ============================================================================
 */

// Get the vertex layout for mesh vertices (for pipeline creation)
pz_vertex_layout pz_mesh_get_vertex_layout(void);

#endif // PZ_MESH_H
