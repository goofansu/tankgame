/*
 * Map System
 *
 * Defines the map structure with terrain grid, height map for walls,
 * spawn points, and other game objects.
 */

#ifndef PZ_MAP_H
#define PZ_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/pz_math.h"

// Maximum map dimensions
#define PZ_MAP_MAX_SIZE 64
#define PZ_MAP_MAX_SPAWNS 32
#define PZ_MAP_MAX_ENEMIES 16

// Terrain tile types
typedef enum pz_tile_type {
    PZ_TILE_GROUND = 0, // Normal movement
    PZ_TILE_WALL, // Impassable, rendered as 3D
    PZ_TILE_WATER, // Impassable
    PZ_TILE_MUD, // Slow movement (50%)
    PZ_TILE_ICE, // Reduced friction (drift)
    PZ_TILE_COUNT
} pz_tile_type;

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
    int level; // Enemy level (1, 2, or 3)
} pz_enemy_spawn;

// Map lighting settings
typedef struct pz_map_lighting {
    bool has_sun; // Whether sun lighting is enabled
    pz_vec3 sun_direction; // Normalized direction FROM sun (toward scene)
    pz_vec3 sun_color; // Sun light color (RGB 0-1)
    pz_vec3 ambient_color; // Ambient light color when sun is off
    float ambient_darkness; // 0 = full ambient, 1 = pitch black (default 0.85)
} pz_map_lighting;

// Map structure
typedef struct pz_map {
    char name[64];
    int width;
    int height;
    float tile_size;

    // Terrain data (width * height)
    pz_tile_type *terrain;

    // Height map for walls (0 = floor, 1-9 = wall height)
    uint8_t *height_map;

    // Spawn points (for player)
    pz_spawn_point spawns[PZ_MAP_MAX_SPAWNS];
    int spawn_count;

    // Enemy spawns (for AI-controlled tanks)
    pz_enemy_spawn enemies[PZ_MAP_MAX_ENEMIES];
    int enemy_count;

    // Lighting settings
    pz_map_lighting lighting;

    // Bounds (in world units)
    float world_width;
    float world_height;
} pz_map;

// Creation/destruction
pz_map *pz_map_create(int width, int height, float tile_size);
void pz_map_destroy(pz_map *map);

// Build a hardcoded test map
pz_map *pz_map_create_test(void);

// Tile access
pz_tile_type pz_map_get_tile(const pz_map *map, int x, int y);
void pz_map_set_tile(pz_map *map, int x, int y, pz_tile_type type);

// Height access
uint8_t pz_map_get_height(const pz_map *map, int x, int y);
void pz_map_set_height(pz_map *map, int x, int y, uint8_t height);

// World coordinate queries
pz_tile_type pz_map_get_tile_at(const pz_map *map, pz_vec2 world_pos);
bool pz_map_is_solid(const pz_map *map, pz_vec2 world_pos);
bool pz_map_is_passable(const pz_map *map, pz_vec2 world_pos);

// Get movement speed multiplier for terrain at position
float pz_map_get_speed_multiplier(const pz_map *map, pz_vec2 world_pos);

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

// Lighting helpers
const pz_map_lighting *pz_map_get_lighting(const pz_map *map);

// Debug: print map to console
void pz_map_print(const pz_map *map);

// Raycast from start position in direction until hitting a solid tile or
// max_dist Returns the hit position (either wall hit or max distance) If hit is
// not NULL, sets *hit to true if a wall was hit
pz_vec2 pz_map_raycast(const pz_map *map, pz_vec2 start, pz_vec2 direction,
    float max_dist, bool *hit);

// ============================================================================
// Map Loading/Saving
// ============================================================================

// Load map from file (returns NULL on failure)
pz_map *pz_map_load(const char *path);

// Save map to file (returns true on success)
bool pz_map_save(const pz_map *map, const char *path);

// Get the file modification time of a map file
// Returns 0 if the file doesn't exist
int64_t pz_map_file_mtime(const char *path);

#endif // PZ_MAP_H
