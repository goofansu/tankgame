/*
 * Map System Implementation
 */

#include "pz_map.h"

#include <stdio.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

pz_map *
pz_map_create(int width, int height, float tile_size)
{
    if (width <= 0 || height <= 0 || width > PZ_MAP_MAX_SIZE
        || height > PZ_MAP_MAX_SIZE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
            "Invalid map size: %dx%d (max %d)", width, height, PZ_MAP_MAX_SIZE);
        return NULL;
    }

    pz_map *map = pz_calloc(1, sizeof(pz_map));
    if (!map) {
        return NULL;
    }

    map->width = width;
    map->height = height;
    map->tile_size = tile_size;
    map->world_width = width * tile_size;
    map->world_height = height * tile_size;

    // Allocate terrain and height arrays
    int num_tiles = width * height;
    map->terrain = pz_calloc(num_tiles, sizeof(pz_tile_type));
    map->height_map = pz_calloc(num_tiles, sizeof(uint8_t));

    if (!map->terrain || !map->height_map) {
        pz_map_destroy(map);
        return NULL;
    }

    // Default all tiles to ground
    for (int i = 0; i < num_tiles; i++) {
        map->terrain[i] = PZ_TILE_GROUND;
        map->height_map[i] = 0;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Created map %dx%d (%.1f unit tiles)",
        width, height, tile_size);

    return map;
}

void
pz_map_destroy(pz_map *map)
{
    if (!map) {
        return;
    }

    pz_free(map->terrain);
    pz_free(map->height_map);
    pz_free(map);
}

// Build a hardcoded 16x16 test map
pz_map *
pz_map_create_test(void)
{
    // 16x16 map with 2.0 unit tiles = 32x32 world units
    pz_map *map = pz_map_create(16, 16, 2.0f);
    if (!map) {
        return NULL;
    }

    snprintf(map->name, sizeof(map->name), "Test Arena");

    // Define the map layout
    // . = ground, # = wall, ~ = water, : = mud, * = ice
    const char *layout[] = {
        "################",
        "#..............#",
        "#..##....##....#",
        "#..##....##..*.#",
        "#..........***.#",
        "#....::.....*..#",
        "#....::........#",
        "#..............#",
        "#..............#",
        "#........~~....#",
        "#........~~....#",
        "#..##..........#",
        "#..##....##....#",
        "#........##....#",
        "#..............#",
        "################",
    };

    // Parse layout into terrain
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            char c = layout[y][x];
            pz_tile_type tile = PZ_TILE_GROUND;

            switch (c) {
            case '#':
                tile = PZ_TILE_WALL;
                break;
            case '~':
                tile = PZ_TILE_WATER;
                break;
            case ':':
                tile = PZ_TILE_MUD;
                break;
            case '*':
                tile = PZ_TILE_ICE;
                break;
            case '.':
            default:
                tile = PZ_TILE_GROUND;
                break;
            }

            pz_map_set_tile(map, x, y, tile);

            // Walls get height 2
            if (tile == PZ_TILE_WALL) {
                pz_map_set_height(map, x, y, 2);
            }
        }
    }

    // Add spawn points (4 corners for FFA)
    map->spawn_count = 4;

    // Bottom-left
    map->spawns[0] = (pz_spawn_point) {
        .pos = { 4.0f, 4.0f },
        .angle = 0.785f, // 45 degrees (NE)
        .team = 0,
        .team_spawn = false,
    };

    // Bottom-right
    map->spawns[1] = (pz_spawn_point) {
        .pos = { 28.0f, 4.0f },
        .angle = 2.356f, // 135 degrees (NW)
        .team = 0,
        .team_spawn = false,
    };

    // Top-left
    map->spawns[2] = (pz_spawn_point) {
        .pos = { 4.0f, 28.0f },
        .angle = -0.785f, // -45 degrees (SE)
        .team = 0,
        .team_spawn = false,
    };

    // Top-right
    map->spawns[3] = (pz_spawn_point) {
        .pos = { 28.0f, 28.0f },
        .angle = -2.356f, // -135 degrees (SW)
        .team = 0,
        .team_spawn = false,
    };

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Created test map: %s", map->name);

    return map;
}

pz_tile_type
pz_map_get_tile(const pz_map *map, int x, int y)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        return PZ_TILE_WALL; // Out of bounds is solid
    }
    return map->terrain[y * map->width + x];
}

void
pz_map_set_tile(pz_map *map, int x, int y, pz_tile_type type)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        return;
    }
    map->terrain[y * map->width + x] = type;
}

uint8_t
pz_map_get_height(const pz_map *map, int x, int y)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        return 0;
    }
    return map->height_map[y * map->width + x];
}

void
pz_map_set_height(pz_map *map, int x, int y, uint8_t height)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        return;
    }
    map->height_map[y * map->width + x] = height;
}

pz_tile_type
pz_map_get_tile_at(const pz_map *map, pz_vec2 world_pos)
{
    int tx, ty;
    pz_map_world_to_tile(map, world_pos, &tx, &ty);
    return pz_map_get_tile(map, tx, ty);
}

bool
pz_map_is_solid(const pz_map *map, pz_vec2 world_pos)
{
    pz_tile_type tile = pz_map_get_tile_at(map, world_pos);
    return tile == PZ_TILE_WALL || tile == PZ_TILE_WATER;
}

bool
pz_map_is_passable(const pz_map *map, pz_vec2 world_pos)
{
    return !pz_map_is_solid(map, world_pos);
}

float
pz_map_get_speed_multiplier(const pz_map *map, pz_vec2 world_pos)
{
    pz_tile_type tile = pz_map_get_tile_at(map, world_pos);

    switch (tile) {
    case PZ_TILE_MUD:
        return 0.5f; // 50% speed
    case PZ_TILE_ICE:
        return 1.2f; // Slightly faster but will have different friction
    case PZ_TILE_WALL:
    case PZ_TILE_WATER:
        return 0.0f; // Impassable
    case PZ_TILE_GROUND:
    default:
        return 1.0f;
    }
}

pz_vec2
pz_map_tile_to_world(const pz_map *map, int tile_x, int tile_y)
{
    // Center of the map is at world origin (0,0)
    // Tile (0,0) is at bottom-left
    float half_w = map->world_width / 2.0f;
    float half_h = map->world_height / 2.0f;

    pz_vec2 result = {
        .x = tile_x * map->tile_size + map->tile_size / 2.0f - half_w,
        .y = tile_y * map->tile_size + map->tile_size / 2.0f - half_h,
    };

    return result;
}

void
pz_map_world_to_tile(
    const pz_map *map, pz_vec2 world_pos, int *tile_x, int *tile_y)
{
    float half_w = map->world_width / 2.0f;
    float half_h = map->world_height / 2.0f;

    *tile_x = (int)((world_pos.x + half_w) / map->tile_size);
    *tile_y = (int)((world_pos.y + half_h) / map->tile_size);
}

bool
pz_map_in_bounds(const pz_map *map, int tile_x, int tile_y)
{
    return tile_x >= 0 && tile_x < map->width && tile_y >= 0
        && tile_y < map->height;
}

bool
pz_map_in_bounds_world(const pz_map *map, pz_vec2 world_pos)
{
    float half_w = map->world_width / 2.0f;
    float half_h = map->world_height / 2.0f;

    return world_pos.x >= -half_w && world_pos.x < half_w
        && world_pos.y >= -half_h && world_pos.y < half_h;
}

const pz_spawn_point *
pz_map_get_spawn(const pz_map *map, int index)
{
    if (!map || index < 0 || index >= map->spawn_count) {
        return NULL;
    }
    return &map->spawns[index];
}

int
pz_map_get_spawn_count(const pz_map *map)
{
    return map ? map->spawn_count : 0;
}

void
pz_map_print(const pz_map *map)
{
    if (!map) {
        return;
    }

    printf("Map: %s (%dx%d, tile_size=%.1f)\n", map->name, map->width,
        map->height, map->tile_size);
    printf("World size: %.1f x %.1f\n", map->world_width, map->world_height);
    printf("Spawns: %d\n", map->spawn_count);

    printf("\nTerrain:\n");
    for (int y = map->height - 1; y >= 0; y--) {
        printf("  ");
        for (int x = 0; x < map->width; x++) {
            pz_tile_type t = pz_map_get_tile(map, x, y);
            char c = '.';
            switch (t) {
            case PZ_TILE_WALL:
                c = '#';
                break;
            case PZ_TILE_WATER:
                c = '~';
                break;
            case PZ_TILE_MUD:
                c = ':';
                break;
            case PZ_TILE_ICE:
                c = '*';
                break;
            default:
                c = '.';
                break;
            }
            printf("%c", c);
        }
        printf("\n");
    }
}
