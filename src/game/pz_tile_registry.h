/*
 * Tile Registry System
 *
 * Loads tile definitions from .tile files and provides lookup by name.
 * Each tile defines textures for ground/wall rendering and movement properties.
 *
 * Tile files are loaded from assets/tiles/ at startup.
 * Maps reference tiles by name (e.g., "oak_dark") rather than texture paths.
 */

#ifndef PZ_TILE_REGISTRY_H
#define PZ_TILE_REGISTRY_H

#include <stdbool.h>

#include "../engine/render/pz_texture.h"

// Maximum number of tiles that can be registered
#define PZ_TILE_REGISTRY_MAX_TILES 64

// Tile configuration - loaded from .tile files
typedef struct pz_tile_config {
    char name[32]; // Semantic name (e.g., "oak_dark")

    // Texture paths (from .tile file)
    char ground_texture_path[128]; // Ground texture path
    char wall_texture_path[128]; // Wall top texture (defaults to ground)
    char wall_side_texture_path[128]; // Wall side texture (defaults to wall)

    // Loaded texture handles (populated by registry after loading)
    pz_texture_handle ground_texture;
    pz_texture_handle wall_texture;
    pz_texture_handle wall_side_texture;

    // Movement properties
    float speed_multiplier; // Movement speed modifier (default 1.0)
    float friction; // Friction coefficient (default 1.0)

    // Texture scale (how many tiles the texture spans)
    // A scale of 6 means the texture covers a 6x6 tile area
    int ground_texture_scale; // Ground texture scale (default 1)
    int wall_texture_scale; // Wall texture scale (default 1)

    // Status
    bool valid; // False if tile failed to load properly
} pz_tile_config;

// Opaque tile registry type
typedef struct pz_tile_registry pz_tile_registry;

// Create a new tile registry
// Does not load any tiles - call pz_tile_registry_load_all() after creation
pz_tile_registry *pz_tile_registry_create(void);

// Destroy the tile registry
void pz_tile_registry_destroy(pz_tile_registry *registry);

// Load all .tile files from assets/tiles/
// Must be called after texture manager is available
// Returns number of tiles loaded successfully
int pz_tile_registry_load_all(pz_tile_registry *registry,
    pz_texture_manager *tex_manager, const char *tiles_dir);

// Load textures for all registered tiles
// Call this after load_all to resolve texture paths to handles
void pz_tile_registry_load_textures(
    pz_tile_registry *registry, pz_texture_manager *tex_manager);

// Get a tile configuration by name
// Returns NULL if tile not found (and logs warning)
const pz_tile_config *pz_tile_registry_get(
    const pz_tile_registry *registry, const char *name);

// Get the fallback tile (orange checkerboard for missing/invalid tiles)
const pz_tile_config *pz_tile_registry_get_fallback(
    const pz_tile_registry *registry);

// Get number of registered tiles
int pz_tile_registry_count(const pz_tile_registry *registry);

// Get tile by index (for iteration)
const pz_tile_config *pz_tile_registry_get_by_index(
    const pz_tile_registry *registry, int index);

// Debug: print all registered tiles
void pz_tile_registry_print(const pz_tile_registry *registry);

#endif // PZ_TILE_REGISTRY_H
