/*
 * Map System
 *
 * Defines the map structure with terrain grid, height map for walls,
 * spawn points, and other game objects.
 *
 * Map Format v2:
 * - Combined height+terrain cells in a single grid
 * - Height determines collision (>0 = wall, <0 = pit/submerged)
 * - Tile types reference tile definitions in assets/tiles/
 * - Inline object placement with tags
 */

#ifndef PZ_MAP_H
#define PZ_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"
#include "pz_toxic_cloud.h"

// Forward declaration
typedef struct pz_tile_registry pz_tile_registry;

// Maximum map dimensions
#define PZ_MAP_MAX_SIZE 64
#define PZ_MAP_MAX_SPAWNS 32
#define PZ_MAP_MAX_ENEMIES 16
#define PZ_MAP_MAX_POWERUPS 16
#define PZ_MAP_MAX_BARRIERS 32
#define PZ_MAP_MAX_TILE_DEFS 32

// Tile definition - maps a symbol to a tile name
// Symbols are defined per-map in the map file
// Actual tile properties come from the tile registry
typedef struct pz_tile_def {
    char symbol; // Single char used in map grid (e.g., '.', '#')
    char name[32]; // Tile name (e.g., "wood_oak_brown") - looked up in registry
} pz_tile_def;

// Spawn point data
typedef struct pz_spawn_point {
    pz_vec2 pos;
    float angle; // Facing direction in radians
    int team; // 0 = FFA, 1+ = team number
    bool team_spawn; // true = team mode only, false = FFA
} pz_spawn_point;

// Enemy spawn data (for AI-controlled tanks)
typedef struct pz_enemy_spawn {
    pz_vec2 pos;
    float angle; // Facing direction in radians
    int level; // Enemy type (named in map files, stored as enum value)
} pz_enemy_spawn;

// Powerup spawn data
// Type names: "machine_gun", "ricochet"
typedef struct pz_powerup_spawn {
    pz_vec2 pos;
    char type_name[32]; // Powerup type name (resolved at runtime)
    float respawn_time; // Time to respawn after collection (default: 15)
} pz_powerup_spawn;

// Barrier spawn data (destructible obstacles)
// Barriers block movement, projectiles, and light until destroyed
typedef struct pz_barrier_spawn {
    pz_vec2 pos;
    char tile_name[32]; // Which tile's textures to use (e.g., "cobble")
    float health; // Starting health (default: 20)
} pz_barrier_spawn;

// Map lighting settings
typedef struct pz_map_lighting {
    bool has_sun; // Whether sun lighting is enabled
    pz_vec3 sun_direction; // Normalized direction FROM sun (toward scene)
    pz_vec3 sun_color; // Sun light color (RGB 0-1)
    pz_vec3 ambient_color; // Ambient light color when sun is off
    float ambient_darkness; // 0 = full ambient, 1 = pitch black (default 0.85)
} pz_map_lighting;

// Background type
typedef enum pz_background_type {
    PZ_BACKGROUND_COLOR, // Solid color
    PZ_BACKGROUND_GRADIENT, // Two-color gradient
    PZ_BACKGROUND_TEXTURE, // Texture (future)
} pz_background_type;

// Gradient direction
typedef enum pz_gradient_direction {
    PZ_GRADIENT_VERTICAL, // Top to bottom
    PZ_GRADIENT_RADIAL, // Center outward
} pz_gradient_direction;

// Map background settings
typedef struct pz_map_background {
    pz_background_type type;
    pz_vec3 color; // For COLOR type, or gradient start color
    pz_vec3 color_end; // For GRADIENT: end color (bottom/outer)
    pz_gradient_direction gradient_dir; // For GRADIENT: direction
    char texture_path[64]; // For TEXTURE: path to texture file (future)
} pz_map_background;

// Map cell (combined height + tile)
typedef struct pz_map_cell {
    int8_t height; // Height level: >0 = wall, 0 = ground, <0 = pit
    uint8_t tile_index; // Index into tile_defs array
} pz_map_cell;

// Map structure
typedef struct pz_map {
    char name[64];
    int version;
    int width;
    int height;
    float tile_size;

    // Tile definitions (symbol -> tile name mapping)
    pz_tile_def tile_defs[PZ_MAP_MAX_TILE_DEFS];
    int tile_def_count;

    // Tile registry reference (owned externally, used for lookups)
    const pz_tile_registry *tile_registry;

    // Grid data (width * height cells)
    pz_map_cell *cells;

    // Water level (height at which water surface renders)
    // Tiles at or below this height are submerged
    int water_level;
    bool has_water;
    pz_vec3 water_color; // Base water color (RGB 0-1)
    float wave_strength; // Wave amplitude multiplier (default 1.0)
    float wind_direction; // Wind direction in radians (0 = +X, PI/2 = +Z)
    float wind_strength; // Wind strength multiplier (default 1.0)

    // Ground fog level (height at which fog plane renders)
    // Tiles at or below this height receive fog
    int fog_level;
    bool has_fog;
    pz_vec3 fog_color; // Base fog color (RGB 0-1)

    // Spawn points (for player)
    pz_spawn_point spawns[PZ_MAP_MAX_SPAWNS];
    int spawn_count;

    // Enemy spawns (for AI-controlled tanks)
    pz_enemy_spawn enemies[PZ_MAP_MAX_ENEMIES];
    int enemy_count;

    // Powerup spawns
    pz_powerup_spawn powerups[PZ_MAP_MAX_POWERUPS];
    int powerup_count;

    // Barrier spawns (destructible obstacles)
    pz_barrier_spawn barriers[PZ_MAP_MAX_BARRIERS];
    int barrier_count;

    // Lighting settings
    pz_map_lighting lighting;

    // Background settings
    pz_map_background background;

    // Music settings
    char music_name[64];
    bool has_music;

    // Toxic cloud settings
    bool has_toxic_cloud;
    pz_toxic_cloud_config toxic_config;

    // Bounds (in world units)
    float world_width;
    float world_height;
} pz_map;

// Creation/destruction
pz_map *pz_map_create(int width, int height, float tile_size);
void pz_map_destroy(pz_map *map);

// Set the tile registry for the map (must be called before using tile lookups)
void pz_map_set_tile_registry(pz_map *map, const pz_tile_registry *registry);

// Build a hardcoded test map
pz_map *pz_map_create_test(void);

// Cell access (new API)
pz_map_cell pz_map_get_cell(const pz_map *map, int x, int y);
void pz_map_set_cell(pz_map *map, int x, int y, pz_map_cell cell);

// Height access
int8_t pz_map_get_height(const pz_map *map, int x, int y);
void pz_map_set_height(pz_map *map, int x, int y, int8_t height);

// Tile access
uint8_t pz_map_get_tile_index(const pz_map *map, int x, int y);
const pz_tile_def *pz_map_get_tile_def(const pz_map *map, int x, int y);
const pz_tile_def *pz_map_get_tile_def_by_index(
    const pz_map *map, uint8_t index);

// Add a tile definition, returns index or -1 on failure
int pz_map_add_tile_def(pz_map *map, char symbol, const char *name);

// Find tile definition by symbol, returns index or -1 if not found
int pz_map_find_tile_def(const pz_map *map, char symbol);

// World coordinate queries
bool pz_map_is_solid(const pz_map *map, pz_vec2 world_pos);
bool pz_map_is_passable(const pz_map *map, pz_vec2 world_pos);
bool pz_map_blocks_bullets(const pz_map *map, pz_vec2 world_pos);

// Get movement speed multiplier for terrain at position
// Requires tile registry to be set on the map
float pz_map_get_speed_multiplier(const pz_map *map, pz_vec2 world_pos);

// Get friction for terrain at position
// Requires tile registry to be set on the map
float pz_map_get_friction(const pz_map *map, pz_vec2 world_pos);

// Convert between tile and world coordinates
pz_vec2 pz_map_tile_to_world(const pz_map *map, int tile_x, int tile_y);
void pz_map_world_to_tile(
    const pz_map *map, pz_vec2 world_pos, int *tile_x, int *tile_y);

// Check if coordinates are within map bounds
bool pz_map_in_bounds(const pz_map *map, int tile_x, int tile_y);
bool pz_map_in_bounds_world(const pz_map *map, pz_vec2 world_pos);

// Spawn point helpers
const pz_spawn_point *pz_map_get_spawn(const pz_map *map, int index);
int pz_map_get_spawn_count(const pz_map *map);

// Enemy spawn helpers
const pz_enemy_spawn *pz_map_get_enemy(const pz_map *map, int index);
int pz_map_get_enemy_count(const pz_map *map);

// Powerup spawn helpers
const pz_powerup_spawn *pz_map_get_powerup(const pz_map *map, int index);
int pz_map_get_powerup_count(const pz_map *map);

// Barrier spawn helpers
const pz_barrier_spawn *pz_map_get_barrier(const pz_map *map, int index);
int pz_map_get_barrier_count(const pz_map *map);

// Lighting helpers
const pz_map_lighting *pz_map_get_lighting(const pz_map *map);

// Background helpers
const pz_map_background *pz_map_get_background(const pz_map *map);

// Debug: print map to console
void pz_map_print(const pz_map *map);

// Raycast from start position in direction until hitting a solid tile or
// max_dist. Returns the hit position (either wall hit or max distance).
// If hit is not NULL, sets *hit to true if a wall was hit.
pz_vec2 pz_map_raycast(const pz_map *map, pz_vec2 start, pz_vec2 direction,
    float max_dist, bool *hit);

// Extended raycast result with wall normal
typedef struct pz_raycast_result {
    bool hit; // True if a wall was hit
    pz_vec2 point; // Hit point (or end point if no hit)
    pz_vec2 normal; // Wall normal at hit point (zero if no hit)
    float distance; // Distance traveled
} pz_raycast_result;

// Raycast with DDA algorithm - returns exact hit point and wall normal.
// Uses proper grid traversal to never miss walls.
// This is the preferred function for projectile collision.
pz_raycast_result pz_map_raycast_ex(
    const pz_map *map, pz_vec2 start, pz_vec2 end);

// ============================================================================
// Map Loading/Saving
// ============================================================================

// Load map from file (returns NULL on failure)
// Supports v2 format only
// If registry is provided, it will be set on the map for tile lookups
pz_map *pz_map_load(const char *path);
pz_map *pz_map_load_with_registry(
    const char *path, const pz_tile_registry *registry);

// Save map to file in v2 format (returns true on success)
bool pz_map_save(const pz_map *map, const char *path);

// Get the file modification time of a map file
// Returns 0 if the file doesn't exist
int64_t pz_map_file_mtime(const char *path);

#endif // PZ_MAP_H
