/*
 * Map System Implementation
 */

#include "pz_map.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"
#include "../core/pz_str.h"

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

    // Auto-set height for walls (1 = standard wall height)
    // This makes pz_map_is_solid() consistent with terrain type
    if (type == PZ_TILE_WALL) {
        map->height_map[y * map->width + x] = 1;
    } else if (map->height_map[y * map->width + x] > 0) {
        // Clear height for non-wall tiles that had height
        map->height_map[y * map->width + x] = 0;
    }
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
    // Use height map for collision - this matches the rendered wall geometry
    int tx, ty;
    pz_map_world_to_tile(map, world_pos, &tx, &ty);

    // Out of bounds is solid
    if (!pz_map_in_bounds(map, tx, ty)) {
        return true;
    }

    // Check height (walls have height > 0)
    if (pz_map_get_height(map, tx, ty) > 0) {
        return true;
    }

    // Also check terrain for water (which has no height but is impassable)
    pz_tile_type tile = pz_map_get_tile(map, tx, ty);
    return tile == PZ_TILE_WATER;
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

const pz_enemy_spawn *
pz_map_get_enemy(const pz_map *map, int index)
{
    if (!map || index < 0 || index >= map->enemy_count) {
        return NULL;
    }
    return &map->enemies[index];
}

int
pz_map_get_enemy_count(const pz_map *map)
{
    return map ? map->enemy_count : 0;
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

// ============================================================================
// Map Loading/Saving
// ============================================================================

/*
 * Map file format (text-based):
 *
 * # Comment lines start with #
 * version 1
 * name My Map Name
 * size 16 16
 * tile_size 2.0
 *
 * terrain
 * ################
 * #..............#
 * #..##....##....#
 * ...
 * ################
 *
 * heights
 * 2222222222222222
 * 2000000000000002
 * ...
 *
 * spawn 4.0 4.0 0.785 0 0
 * spawn 28.0 4.0 2.356 0 0
 * ...
 *
 * Terrain chars: . = ground, # = wall, ~ = water, : = mud, * = ice
 * Heights: 0-9 (height level per tile)
 * Spawn: x y angle team team_spawn
 */

// Helper to convert tile char to type
static pz_tile_type
char_to_tile(char c)
{
    switch (c) {
    case '#':
        return PZ_TILE_WALL;
    case '~':
        return PZ_TILE_WATER;
    case ':':
        return PZ_TILE_MUD;
    case '*':
        return PZ_TILE_ICE;
    case '.':
    default:
        return PZ_TILE_GROUND;
    }
}

// Helper to convert tile type to char
static char
tile_to_char(pz_tile_type type)
{
    switch (type) {
    case PZ_TILE_WALL:
        return '#';
    case PZ_TILE_WATER:
        return '~';
    case PZ_TILE_MUD:
        return ':';
    case PZ_TILE_ICE:
        return '*';
    case PZ_TILE_GROUND:
    default:
        return '.';
    }
}

// Skip whitespace
static const char *
skip_whitespace(const char *s)
{
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

// Read a line from text buffer, advance pointer
// Returns NULL if no more lines
static const char *
read_line(const char **text, char *buf, size_t buf_size)
{
    const char *p = *text;
    if (!p || !*p) {
        return NULL;
    }

    size_t i = 0;
    while (*p && *p != '\n' && *p != '\r' && i < buf_size - 1) {
        buf[i++] = *p++;
    }
    buf[i] = '\0';

    // Skip newline(s)
    while (*p && (*p == '\n' || *p == '\r')) {
        p++;
    }

    *text = p;
    return buf;
}

pz_map *
pz_map_load(const char *path)
{
    char *file_data = pz_file_read_text(path);
    if (!file_data) {
        pz_log(
            PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to read map file: %s", path);
        return NULL;
    }

    pz_map *map = NULL;
    const char *text = file_data;
    char line[512];

    int version = 0;
    char name[64] = "Unnamed";
    int width = 0, height = 0;
    float tile_size = 2.0f;

    // Parse header
    while (read_line(&text, line, sizeof(line))) {
        const char *p = skip_whitespace(line);

        // Skip empty lines and comments
        if (!*p || *p == '#') {
            continue;
        }

        if (strncmp(p, "version ", 8) == 0) {
            version = atoi(p + 8);
        } else if (strncmp(p, "name ", 5) == 0) {
            strncpy(name, p + 5, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        } else if (strncmp(p, "size ", 5) == 0) {
            sscanf(p + 5, "%d %d", &width, &height);
        } else if (strncmp(p, "tile_size ", 10) == 0) {
            tile_size = (float)atof(p + 10);
        } else if (strcmp(p, "terrain") == 0) {
            // Start of terrain data
            break;
        }
    }

    if (version != 1) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
            "Unknown map version: %d (expected 1)", version);
        pz_free(file_data);
        return NULL;
    }

    if (width <= 0 || height <= 0 || width > PZ_MAP_MAX_SIZE
        || height > PZ_MAP_MAX_SIZE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Invalid map size: %dx%d", width,
            height);
        pz_free(file_data);
        return NULL;
    }

    // Create the map
    map = pz_map_create(width, height, tile_size);
    if (!map) {
        pz_free(file_data);
        return NULL;
    }
    strncpy(map->name, name, sizeof(map->name) - 1);

    // Read terrain data (height rows, from top to bottom in file)
    int terrain_rows_read = 0;
    while (read_line(&text, line, sizeof(line)) && terrain_rows_read < height) {
        const char *p = skip_whitespace(line);

        // Skip empty lines only (NOT lines starting with #, those are wall
        // tiles!)
        if (!*p) {
            continue;
        }

        // Check for end of terrain section
        if (strcmp(p, "heights") == 0) {
            break;
        }

        // File row 0 is the top of the map (y = height-1)
        int y = height - 1 - terrain_rows_read;
        for (int x = 0; x < width && p[x]; x++) {
            pz_map_set_tile(map, x, y, char_to_tile(p[x]));
        }
        terrain_rows_read++;
    }

    // Read height data
    int height_rows_read = 0;
    while (read_line(&text, line, sizeof(line)) && height_rows_read < height) {
        const char *p = skip_whitespace(line);

        // Skip empty lines and comments
        if (!*p || *p == '#') {
            continue;
        }

        // Check for spawn section
        if (strncmp(p, "spawn ", 6) == 0) {
            // Parse spawn and continue
            float sx, sy, angle;
            int team, team_spawn;
            if (sscanf(p + 6, "%f %f %f %d %d", &sx, &sy, &angle, &team,
                    &team_spawn)
                == 5) {
                if (map->spawn_count < PZ_MAP_MAX_SPAWNS) {
                    map->spawns[map->spawn_count++] = (pz_spawn_point) {
                        .pos = { sx, sy },
                        .angle = angle,
                        .team = team,
                        .team_spawn = team_spawn != 0,
                    };
                }
            }
            continue;
        }

        // Check for enemy spawn
        if (strncmp(p, "enemy ", 6) == 0) {
            float ex, ey, angle;
            int level;
            if (sscanf(p + 6, "%f %f %f %d", &ex, &ey, &angle, &level) == 4) {
                if (map->enemy_count < PZ_MAP_MAX_ENEMIES) {
                    map->enemies[map->enemy_count++] = (pz_enemy_spawn) {
                        .pos = { ex, ey },
                        .angle = angle,
                        .level = level,
                    };
                }
            }
            continue;
        }

        // Parse height row
        int y = height - 1 - height_rows_read;
        for (int x = 0; x < width && p[x]; x++) {
            if (p[x] >= '0' && p[x] <= '9') {
                pz_map_set_height(map, x, y, (uint8_t)(p[x] - '0'));
            }
        }
        height_rows_read++;
    }

    // Continue reading for any remaining spawns and enemies
    while (read_line(&text, line, sizeof(line))) {
        const char *p = skip_whitespace(line);
        if (!*p || *p == '#') {
            continue;
        }

        if (strncmp(p, "spawn ", 6) == 0) {
            float sx, sy, angle;
            int team, team_spawn;
            if (sscanf(p + 6, "%f %f %f %d %d", &sx, &sy, &angle, &team,
                    &team_spawn)
                == 5) {
                if (map->spawn_count < PZ_MAP_MAX_SPAWNS) {
                    map->spawns[map->spawn_count++] = (pz_spawn_point) {
                        .pos = { sx, sy },
                        .angle = angle,
                        .team = team,
                        .team_spawn = team_spawn != 0,
                    };
                }
            }
        } else if (strncmp(p, "enemy ", 6) == 0) {
            float ex, ey, angle;
            int level;
            if (sscanf(p + 6, "%f %f %f %d", &ex, &ey, &angle, &level) == 4) {
                if (map->enemy_count < PZ_MAP_MAX_ENEMIES) {
                    map->enemies[map->enemy_count++] = (pz_enemy_spawn) {
                        .pos = { ex, ey },
                        .angle = angle,
                        .level = level,
                    };
                }
            }
        }
    }

    pz_free(file_data);

    pz_log(
        PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Loaded map: %s (%s)", map->name, path);

    return map;
}

bool
pz_map_save(const pz_map *map, const char *path)
{
    if (!map || !path) {
        return false;
    }

    // Build map file content
    // Calculate approximate size needed
    size_t buf_size = 1024 + (size_t)(map->width + 2) * (size_t)map->height * 2
        + (size_t)map->spawn_count * 64 + (size_t)map->enemy_count * 64;
    char *buf = pz_alloc(buf_size);
    if (!buf) {
        return false;
    }

    char *p = buf;
    int remaining = (int)buf_size;

    // Header
    int written = snprintf(p, remaining,
        "# Tank Game Map\n"
        "version 1\n"
        "name %s\n"
        "size %d %d\n"
        "tile_size %.1f\n"
        "\n"
        "terrain\n",
        map->name, map->width, map->height, map->tile_size);
    p += written;
    remaining -= written;

    // Terrain data (top to bottom in file = high Y to low Y)
    for (int y = map->height - 1; y >= 0 && remaining > map->width + 2; y--) {
        for (int x = 0; x < map->width; x++) {
            *p++ = tile_to_char(pz_map_get_tile(map, x, y));
            remaining--;
        }
        *p++ = '\n';
        remaining--;
    }

    // Height data
    written = snprintf(p, remaining, "\nheights\n");
    p += written;
    remaining -= written;

    for (int y = map->height - 1; y >= 0 && remaining > map->width + 2; y--) {
        for (int x = 0; x < map->width; x++) {
            uint8_t h = pz_map_get_height(map, x, y);
            *p++ = (char)('0' + (h > 9 ? 9 : h));
            remaining--;
        }
        *p++ = '\n';
        remaining--;
    }

    // Spawn points
    if (map->spawn_count > 0 && remaining > 64) {
        written = snprintf(p, remaining, "\n");
        p += written;
        remaining -= written;

        for (int i = 0; i < map->spawn_count && remaining > 64; i++) {
            const pz_spawn_point *sp = &map->spawns[i];
            written = snprintf(p, remaining, "spawn %.2f %.2f %.3f %d %d\n",
                sp->pos.x, sp->pos.y, sp->angle, sp->team,
                sp->team_spawn ? 1 : 0);
            p += written;
            remaining -= written;
        }
    }

    // Enemy spawns
    if (map->enemy_count > 0 && remaining > 64) {
        written = snprintf(p, remaining, "\n");
        p += written;
        remaining -= written;

        for (int i = 0; i < map->enemy_count && remaining > 64; i++) {
            const pz_enemy_spawn *es = &map->enemies[i];
            written = snprintf(p, remaining, "enemy %.2f %.2f %.3f %d\n",
                es->pos.x, es->pos.y, es->angle, es->level);
            p += written;
            remaining -= written;
        }
    }

    *p = '\0';

    bool success = pz_file_write_text(path, buf);
    pz_free(buf);

    if (success) {
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Saved map: %s", path);
    } else {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to save map: %s", path);
    }

    return success;
}

int64_t
pz_map_file_mtime(const char *path)
{
    return pz_file_mtime(path);
}

pz_vec2
pz_map_raycast(const pz_map *map, pz_vec2 start, pz_vec2 direction,
    float max_dist, bool *hit)
{
    if (hit)
        *hit = false;

    if (!map || max_dist <= 0.0f)
        return start;

    // Normalize direction
    float dir_len = pz_vec2_len(direction);
    if (dir_len < 0.0001f)
        return start;
    direction = pz_vec2_scale(direction, 1.0f / dir_len);

    // DDA-style raycast stepping through the grid
    // Step size for raymarching (smaller = more precise but slower)
    const float step_size = 0.05f;
    float dist = 0.0f;

    pz_vec2 pos = start;

    while (dist < max_dist) {
        // Check if current position is solid
        if (pz_map_is_solid(map, pos)) {
            if (hit)
                *hit = true;
            // Step back slightly to get the surface position
            return pz_vec2_sub(pos, pz_vec2_scale(direction, step_size));
        }

        // Check if out of bounds
        if (!pz_map_in_bounds_world(map, pos)) {
            if (hit)
                *hit = true;
            return pos;
        }

        // Step forward
        pos = pz_vec2_add(pos, pz_vec2_scale(direction, step_size));
        dist += step_size;
    }

    // Reached max distance without hitting anything
    return pz_vec2_add(start, pz_vec2_scale(direction, max_dist));
}
