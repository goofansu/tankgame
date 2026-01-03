/*
 * Map System Implementation
 *
 * Supports map format v2 with combined height+terrain cells.
 */

#include "pz_map.h"
#include "pz_tile_registry.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"
#include "../core/pz_str.h"

// Default tile definitions
static void
init_default_tile_defs(pz_map *map)
{
    // Ground - standard passable terrain (wood floor)
    pz_map_add_tile_def(map, '.', "wood_oak_brown");

    // Stone - wall material (dark rustic wood)
    pz_map_add_tile_def(map, '#', "wood_rustic_dark");
}

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

    map->version = 2;
    map->width = width;
    map->height = height;
    map->tile_size = tile_size;
    map->world_width = width * tile_size;
    map->world_height = height * tile_size;
    map->water_level = -100; // No water by default (far below any tile)
    map->has_water = false;
    map->water_color = (pz_vec3) { 0.2f, 0.4f, 0.6f }; // Default blue

    // Allocate cells
    int num_cells = width * height;
    map->cells = pz_calloc(num_cells, sizeof(pz_map_cell));
    if (!map->cells) {
        pz_map_destroy(map);
        return NULL;
    }

    // Initialize default tile definitions
    init_default_tile_defs(map);

    // Default all cells to ground at height 0
    for (int i = 0; i < num_cells; i++) {
        map->cells[i].height = 0;
        map->cells[i].tile_index = 0; // ground
    }

    // Default lighting: night mode (no sun, dark ambient)
    map->lighting.has_sun = false;
    map->lighting.sun_direction = (pz_vec3) { 0.4f, -0.8f, 0.3f };
    map->lighting.sun_color = (pz_vec3) { 1.0f, 0.95f, 0.85f };
    map->lighting.ambient_color = (pz_vec3) { 0.12f, 0.12f, 0.15f };
    map->lighting.ambient_darkness = 0.85f;

    // Default background: dark gray (fallback, maps should override)
    map->background.type = PZ_BACKGROUND_COLOR;
    map->background.color = (pz_vec3) { 0.2f, 0.2f, 0.25f };
    map->background.color_end = (pz_vec3) { 0.1f, 0.1f, 0.15f };
    map->background.gradient_dir = PZ_GRADIENT_VERTICAL;
    map->background.texture_path[0] = '\0';

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

    pz_free(map->cells);
    pz_free(map);
}

void
pz_map_set_tile_registry(pz_map *map, const pz_tile_registry *registry)
{
    if (map) {
        map->tile_registry = registry;
    }
}

// Build a hardcoded 16x16 test map
pz_map *
pz_map_create_test(void)
{
    pz_map *map = pz_map_create(16, 16, 2.0f);
    if (!map) {
        return NULL;
    }

    snprintf(map->name, sizeof(map->name), "Test Arena");

    // Add tile definitions
    pz_map_add_tile_def(map, ':', "mud_wet");
    pz_map_add_tile_def(map, '*', "carpet_gray");

    // Define the map layout with heights
    // Format: height, tile_symbol
    // 2# = height 2, stone wall
    // 0. = height 0, ground
    // 0: = height 0, mud
    // 0* = height 0, ice
    const char *layout[] = {
        "2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2#",
        "2# 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 2#",
        "2# 0. 0. 2# 2# 0. 0. 0. 0. 2# 2# 0. 0. 0. 0. 2#",
        "2# 0. 0. 2# 2# 0. 0. 0. 0. 2# 2# 0. 0. 0* 0. 2#",
        "2# 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0* 0* 0* 2#",
        "2# 0. 0. 0. 0. 0: 0: 0. 0. 0. 0. 0. 0* 0. 0. 2#",
        "2# 0. 0. 0. 0. 0: 0: 0. 0. 0. 0. 0. 0. 0. 0. 2#",
        "2# 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 2#",
        "2# 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 2#",
        "2# 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 2#",
        "2# 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 2#",
        "2# 0. 0. 2# 2# 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 2#",
        "2# 0. 0. 2# 2# 0. 0. 0. 0. 2# 2# 0. 0. 0. 0. 2#",
        "2# 0. 0. 0. 0. 0. 0. 0. 0. 2# 2# 0. 0. 0. 0. 2#",
        "2# 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 0. 2#",
        "2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2# 2#",
    };

    // Parse layout
    for (int y = 0; y < 16; y++) {
        const char *row = layout[15 - y]; // Flip Y (file is top-down)
        const char *p = row;

        for (int x = 0; x < 16; x++) {
            // Skip whitespace
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }

            if (!*p)
                break;

            // Parse height (can be negative)
            int height = 0;
            bool negative = false;
            if (*p == '-') {
                negative = true;
                p++;
            }
            while (*p && isdigit((unsigned char)*p)) {
                height = height * 10 + (*p - '0');
                p++;
            }
            if (negative) {
                height = -height;
            }

            // Parse tile symbol
            char tile_symbol = *p++;

            // Set cell
            int tile_idx = pz_map_find_tile_def(map, tile_symbol);
            if (tile_idx < 0) {
                tile_idx = 0; // fallback to ground
            }

            pz_map_cell cell
                = { .height = (int8_t)height, .tile_index = (uint8_t)tile_idx };
            pz_map_set_cell(map, x, y, cell);
        }
    }

    // Add spawn points (4 corners for FFA)
    map->spawn_count = 4;

    // Bottom-left
    map->spawns[0] = (pz_spawn_point) {
        .pos = { -12.0f, -12.0f },
        .angle = 0.785f,
        .team = 0,
        .team_spawn = false,
    };

    // Bottom-right
    map->spawns[1] = (pz_spawn_point) {
        .pos = { 12.0f, -12.0f },
        .angle = 2.356f,
        .team = 0,
        .team_spawn = false,
    };

    // Top-left
    map->spawns[2] = (pz_spawn_point) {
        .pos = { -12.0f, 12.0f },
        .angle = -0.785f,
        .team = 0,
        .team_spawn = false,
    };

    // Top-right
    map->spawns[3] = (pz_spawn_point) {
        .pos = { 12.0f, 12.0f },
        .angle = -2.356f,
        .team = 0,
        .team_spawn = false,
    };

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Created test map: %s", map->name);

    return map;
}

// ============================================================================
// Cell Access
// ============================================================================

pz_map_cell
pz_map_get_cell(const pz_map *map, int x, int y)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        // Out of bounds returns a solid wall
        return (pz_map_cell) { .height = 99, .tile_index = 1 };
    }
    return map->cells[y * map->width + x];
}

void
pz_map_set_cell(pz_map *map, int x, int y, pz_map_cell cell)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        return;
    }
    map->cells[y * map->width + x] = cell;
}

int8_t
pz_map_get_height(const pz_map *map, int x, int y)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        return 99; // Out of bounds is very high wall
    }
    return map->cells[y * map->width + x].height;
}

void
pz_map_set_height(pz_map *map, int x, int y, int8_t height)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        return;
    }
    map->cells[y * map->width + x].height = height;
}

uint8_t
pz_map_get_tile_index(const pz_map *map, int x, int y)
{
    if (!map || !pz_map_in_bounds(map, x, y)) {
        return 1; // Out of bounds returns stone
    }
    return map->cells[y * map->width + x].tile_index;
}

const pz_tile_def *
pz_map_get_tile_def(const pz_map *map, int x, int y)
{
    if (!map)
        return NULL;
    uint8_t idx = pz_map_get_tile_index(map, x, y);
    return pz_map_get_tile_def_by_index(map, idx);
}

const pz_tile_def *
pz_map_get_tile_def_by_index(const pz_map *map, uint8_t index)
{
    if (!map || index >= map->tile_def_count) {
        return NULL;
    }
    return &map->tile_defs[index];
}

int
pz_map_add_tile_def(pz_map *map, char symbol, const char *name)
{
    if (!map || !name || map->tile_def_count >= PZ_MAP_MAX_TILE_DEFS) {
        return -1;
    }

    // Check if symbol already exists
    for (int i = 0; i < map->tile_def_count; i++) {
        if (map->tile_defs[i].symbol == symbol) {
            // Update existing
            strncpy(map->tile_defs[i].name, name,
                sizeof(map->tile_defs[i].name) - 1);
            map->tile_defs[i].name[sizeof(map->tile_defs[i].name) - 1] = '\0';
            return i;
        }
    }

    int idx = map->tile_def_count++;
    pz_tile_def *def = &map->tile_defs[idx];

    def->symbol = symbol;
    strncpy(def->name, name, sizeof(def->name) - 1);
    def->name[sizeof(def->name) - 1] = '\0';

    return idx;
}

int
pz_map_find_tile_def(const pz_map *map, char symbol)
{
    if (!map)
        return -1;

    for (int i = 0; i < map->tile_def_count; i++) {
        if (map->tile_defs[i].symbol == symbol) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// Collision and Movement
// ============================================================================

bool
pz_map_is_solid(const pz_map *map, pz_vec2 world_pos)
{
    int tx, ty;
    pz_map_world_to_tile(map, world_pos, &tx, &ty);

    if (!pz_map_in_bounds(map, tx, ty)) {
        return true; // Out of bounds is solid
    }

    int8_t height = pz_map_get_height(map, tx, ty);

    // Height > 0 = wall, height < 0 = pit (both block movement)
    return height != 0;
}

bool
pz_map_is_passable(const pz_map *map, pz_vec2 world_pos)
{
    return !pz_map_is_solid(map, world_pos);
}

bool
pz_map_blocks_bullets(const pz_map *map, pz_vec2 world_pos)
{
    int tx, ty;
    pz_map_world_to_tile(map, world_pos, &tx, &ty);

    if (!pz_map_in_bounds(map, tx, ty)) {
        return true;
    }

    int8_t height = pz_map_get_height(map, tx, ty);

    // Only walls (height > 0) block bullets
    // Pits (height < 0) don't block bullets - they fly over
    return height > 0;
}

float
pz_map_get_speed_multiplier(const pz_map *map, pz_vec2 world_pos)
{
    int tx, ty;
    pz_map_world_to_tile(map, world_pos, &tx, &ty);

    if (!pz_map_in_bounds(map, tx, ty)) {
        return 0.0f;
    }

    // Check if solid
    int8_t height = pz_map_get_height(map, tx, ty);
    if (height != 0) {
        return 0.0f; // Impassable
    }

    // Get tile definition and look up properties from registry
    const pz_tile_def *def = pz_map_get_tile_def(map, tx, ty);
    if (def && map->tile_registry) {
        const pz_tile_config *config
            = pz_tile_registry_get(map->tile_registry, def->name);
        if (config) {
            return config->speed_multiplier;
        }
    }

    return 1.0f;
}

float
pz_map_get_friction(const pz_map *map, pz_vec2 world_pos)
{
    int tx, ty;
    pz_map_world_to_tile(map, world_pos, &tx, &ty);

    if (!pz_map_in_bounds(map, tx, ty)) {
        return 1.0f;
    }

    // Check if solid - no friction applies
    int8_t height = pz_map_get_height(map, tx, ty);
    if (height != 0) {
        return 1.0f;
    }

    // Get tile definition and look up properties from registry
    const pz_tile_def *def = pz_map_get_tile_def(map, tx, ty);
    if (def && map->tile_registry) {
        const pz_tile_config *config
            = pz_tile_registry_get(map->tile_registry, def->name);
        if (config) {
            return config->friction;
        }
    }

    return 1.0f;
}

// ============================================================================
// Coordinate Conversion
// ============================================================================

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

// ============================================================================
// Spawn/Enemy Helpers
// ============================================================================

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

const pz_map_lighting *
pz_map_get_lighting(const pz_map *map)
{
    return map ? &map->lighting : NULL;
}

const pz_map_background *
pz_map_get_background(const pz_map *map)
{
    return map ? &map->background : NULL;
}

// ============================================================================
// Debug
// ============================================================================

void
pz_map_print(const pz_map *map)
{
    if (!map) {
        return;
    }

    printf("Map: %s (%dx%d, tile_size=%.1f, version=%d)\n", map->name,
        map->width, map->height, map->tile_size, map->version);
    printf("World size: %.1f x %.1f\n", map->world_width, map->world_height);
    printf("Tile definitions: %d\n", map->tile_def_count);
    for (int i = 0; i < map->tile_def_count; i++) {
        printf(
            "  '%c' = %s\n", map->tile_defs[i].symbol, map->tile_defs[i].name);
    }
    printf("Spawns: %d, Enemies: %d\n", map->spawn_count, map->enemy_count);

    printf("\nGrid (height + tile):\n");
    for (int y = map->height - 1; y >= 0; y--) {
        printf("  ");
        for (int x = 0; x < map->width; x++) {
            pz_map_cell cell = pz_map_get_cell(map, x, y);
            const pz_tile_def *def
                = pz_map_get_tile_def_by_index(map, cell.tile_index);
            char symbol = def ? def->symbol : '?';
            printf("%2d%c ", cell.height, symbol);
        }
        printf("\n");
    }
}

// ============================================================================
// Raycast
// ============================================================================

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
    const float step_size = 0.05f;
    float dist = 0.0f;

    pz_vec2 pos = start;

    while (dist < max_dist) {
        // Check if current position blocks bullets
        if (pz_map_blocks_bullets(map, pos)) {
            if (hit)
                *hit = true;
            return pz_vec2_sub(pos, pz_vec2_scale(direction, step_size));
        }

        // Check if out of bounds
        if (!pz_map_in_bounds_world(map, pos)) {
            if (hit)
                *hit = true;
            return pos;
        }

        pos = pz_vec2_add(pos, pz_vec2_scale(direction, step_size));
        dist += step_size;
    }

    return pz_vec2_add(start, pz_vec2_scale(direction, max_dist));
}

// ============================================================================
// Map Loading/Saving
// ============================================================================

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

    while (*p && (*p == '\n' || *p == '\r')) {
        p++;
    }

    *text = p;
    return buf;
}

// Parse a single cell from string: "[-]<height><tile>[|tags]"
// Returns pointer past the parsed cell, or NULL on error
static const char *
parse_cell(const char *p, int8_t *out_height, char *out_tile, char *out_tags,
    size_t tags_size)
{
    *out_height = 0;
    *out_tile = '.';
    if (out_tags)
        out_tags[0] = '\0';

    // Skip leading whitespace
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    if (!*p)
        return NULL;

    // Parse height (can be negative)
    bool negative = false;
    if (*p == '-') {
        negative = true;
        p++;
    }

    int height = 0;
    while (*p && isdigit((unsigned char)*p)) {
        height = height * 10 + (*p - '0');
        p++;
    }
    if (negative) {
        height = -height;
    }
    *out_height = (int8_t)height;

    // Parse tile symbol (single char)
    if (!*p || isspace((unsigned char)*p)) {
        return NULL; // Missing tile symbol
    }
    *out_tile = *p++;

    // Parse optional tags after |
    if (*p == '|') {
        p++; // skip |
        size_t i = 0;
        while (*p && !isspace((unsigned char)*p) && i < tags_size - 1) {
            out_tags[i++] = *p++;
        }
        out_tags[i] = '\0';
    }

    return p;
}

// Parse spawn tag: "spawn angle=<f> team=<i>"
static bool
parse_spawn_tag(const char *params, pz_spawn_point *spawn)
{
    spawn->angle = 0.0f;
    spawn->team = 0;
    spawn->team_spawn = false;

    // Parse key=value pairs
    const char *p = params;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) {
            p++;
        }
        if (!*p)
            break;

        if (strncmp(p, "angle=", 6) == 0) {
            spawn->angle = (float)atof(p + 6);
        } else if (strncmp(p, "team=", 5) == 0) {
            spawn->team = atoi(p + 5);
        } else if (strncmp(p, "team_spawn=", 11) == 0) {
            spawn->team_spawn = atoi(p + 11) != 0;
        }

        // Skip to next param
        while (*p && !isspace((unsigned char)*p) && *p != ',') {
            p++;
        }
    }
    return true;
}

// Parse enemy tag: "enemy angle=<f> level=<i>"
static bool
parse_enemy_tag(const char *params, pz_enemy_spawn *enemy)
{
    enemy->angle = 0.0f;
    enemy->level = 1;

    const char *p = params;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) {
            p++;
        }
        if (!*p)
            break;

        if (strncmp(p, "angle=", 6) == 0) {
            enemy->angle = (float)atof(p + 6);
        } else if (strncmp(p, "level=", 6) == 0) {
            enemy->level = atoi(p + 6);
        }

        while (*p && !isspace((unsigned char)*p) && *p != ',') {
            p++;
        }
    }
    return true;
}

// Tag storage for v2 format
typedef struct tag_def {
    char name[32];
    char type[16]; // "spawn" or "enemy"
    char params[128];
} tag_def;

#define MAX_TAGS 64

// Pending tag placement (to resolve after grid is parsed)
typedef struct tag_placement {
    char tag_name[32];
    int tile_x;
    int tile_y;
} tag_placement;

#define MAX_TAG_PLACEMENTS 256

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
    char line[1024];

    int version = 0;
    char name[64] = "Unnamed";
    int width = 0, height = 0;
    float tile_size = 2.0f;

    // Tag definitions
    tag_def tags[MAX_TAGS];
    int tag_count = 0;

    // Tag placements (resolved after grid)
    tag_placement placements[MAX_TAG_PLACEMENTS];
    int placement_count = 0;

    // Temporary tile defs (before map creation)
    char tile_symbols[PZ_MAP_MAX_TILE_DEFS];
    char tile_names[PZ_MAP_MAX_TILE_DEFS][32];
    int tile_def_count = 0;

    // Parse header
    while (read_line(&text, line, sizeof(line))) {
        const char *p = skip_whitespace(line);

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
        } else if (strncmp(p, "tile ", 5) == 0) {
            // Parse: tile <symbol> <name>
            char sym;
            char tname[32];
            if (sscanf(p + 5, " %c %31s", &sym, tname) == 2) {
                if (tile_def_count < PZ_MAP_MAX_TILE_DEFS) {
                    tile_symbols[tile_def_count] = sym;
                    strncpy(tile_names[tile_def_count], tname, 31);
                    tile_def_count++;
                }
            }
        } else if (strncmp(p, "tag ", 4) == 0) {
            // Parse: tag <name> <type> <params...>
            char tname[32], ttype[16], tparams[128] = "";
            int n = sscanf(p + 4, "%31s %15s %127[^\n]", tname, ttype, tparams);
            if (n >= 2 && tag_count < MAX_TAGS) {
                strncpy(tags[tag_count].name, tname, 31);
                strncpy(tags[tag_count].type, ttype, 15);
                strncpy(tags[tag_count].params, tparams, 127);
                tag_count++;
            }
        } else if (strcmp(p, "grid") == 0) {
            break;
        }
    }

    if (version != 2) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
            "Unsupported map version: %d (expected 2)", version);
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
    map->version = version;

    // Add tile definitions from file
    for (int i = 0; i < tile_def_count; i++) {
        pz_map_add_tile_def(map, tile_symbols[i], tile_names[i]);
    }

    // Parse grid
    int rows_read = 0;
    while (read_line(&text, line, sizeof(line)) && rows_read < height) {
        const char *p = skip_whitespace(line);

        if (!*p)
            continue;

        // Check for end of grid section
        if (*p == '#' || strncmp(p, "spawn ", 6) == 0
            || strncmp(p, "enemy ", 6) == 0 || strncmp(p, "sun_", 4) == 0
            || strncmp(p, "ambient_", 8) == 0
            || strncmp(p, "water_level ", 12) == 0) {
            // Re-process this line below
            break;
        }

        // Parse row of cells
        int y = height - 1 - rows_read; // File is top-down
        int x = 0;

        while (*p && x < width) {
            int8_t cell_height;
            char cell_tile;
            char cell_tags[64] = "";

            p = parse_cell(
                p, &cell_height, &cell_tile, cell_tags, sizeof(cell_tags));
            if (!p)
                break;

            // Find tile definition
            int tile_idx = pz_map_find_tile_def(map, cell_tile);
            if (tile_idx < 0) {
                pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
                    "Unknown tile '%c' at (%d,%d), using ground", cell_tile, x,
                    y);
                tile_idx = 0;
            }

            // Set cell
            pz_map_cell cell
                = { .height = cell_height, .tile_index = (uint8_t)tile_idx };
            pz_map_set_cell(map, x, y, cell);

            // Record tag placements
            if (cell_tags[0]) {
                // Can have multiple tags: "P1,E1"
                char *tag = cell_tags;
                char *comma;
                while ((comma = strchr(tag, ',')) != NULL || tag[0] != '\0') {
                    char single_tag[32];
                    if (comma) {
                        size_t len = comma - tag;
                        if (len > 31)
                            len = 31;
                        strncpy(single_tag, tag, len);
                        single_tag[len] = '\0';
                        tag = comma + 1;
                    } else {
                        strncpy(single_tag, tag, 31);
                        single_tag[31] = '\0';
                        tag += strlen(tag); // end loop
                    }

                    if (single_tag[0] && placement_count < MAX_TAG_PLACEMENTS) {
                        strncpy(placements[placement_count].tag_name,
                            single_tag, 31);
                        placements[placement_count].tile_x = x;
                        placements[placement_count].tile_y = y;
                        placement_count++;
                    }

                    if (!comma)
                        break;
                }
            }

            x++;
        }

        if (x != width) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
                "Row %d has %d cells, expected %d", rows_read, x, width);
        }

        rows_read++;
    }

    // Resolve tag placements
    for (int i = 0; i < placement_count; i++) {
        pz_vec2 pos = pz_map_tile_to_world(
            map, placements[i].tile_x, placements[i].tile_y);

        // Find tag definition
        tag_def *tag = NULL;
        for (int j = 0; j < tag_count; j++) {
            if (strcmp(tags[j].name, placements[i].tag_name) == 0) {
                tag = &tags[j];
                break;
            }
        }

        if (!tag) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "Unknown tag: %s",
                placements[i].tag_name);
            continue;
        }

        if (strcmp(tag->type, "spawn") == 0) {
            if (map->spawn_count < PZ_MAP_MAX_SPAWNS) {
                pz_spawn_point *sp = &map->spawns[map->spawn_count++];
                sp->pos = pos;
                parse_spawn_tag(tag->params, sp);
            }
        } else if (strcmp(tag->type, "enemy") == 0) {
            if (map->enemy_count < PZ_MAP_MAX_ENEMIES) {
                pz_enemy_spawn *es = &map->enemies[map->enemy_count++];
                es->pos = pos;
                parse_enemy_tag(tag->params, es);
            }
        }
    }

    // Continue parsing for remaining directives (spawns, enemies, lighting)
    // The current line might need re-processing
    const char *p = skip_whitespace(line);
    bool first_iteration = (*p != '\0');

    do {
        if (!first_iteration) {
            if (!read_line(&text, line, sizeof(line)))
                break;
            p = skip_whitespace(line);
        }
        first_iteration = false;

        if (!*p || *p == '#')
            continue;

        // Legacy spawn format: spawn x y angle team team_spawn
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
        }
        // Legacy enemy format: enemy x y angle level
        else if (strncmp(p, "enemy ", 6) == 0) {
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
        // Lighting
        else if (strncmp(p, "sun_direction ", 14) == 0) {
            float x, y, z;
            if (sscanf(p + 14, "%f %f %f", &x, &y, &z) == 3) {
                map->lighting.sun_direction = (pz_vec3) { x, y, z };
                map->lighting.has_sun = true;
            }
        } else if (strncmp(p, "sun_color ", 10) == 0) {
            float r, g, b;
            if (sscanf(p + 10, "%f %f %f", &r, &g, &b) == 3) {
                map->lighting.sun_color = (pz_vec3) { r, g, b };
            }
        } else if (strncmp(p, "ambient_color ", 14) == 0) {
            float r, g, b;
            if (sscanf(p + 14, "%f %f %f", &r, &g, &b) == 3) {
                map->lighting.ambient_color = (pz_vec3) { r, g, b };
            }
        } else if (strncmp(p, "ambient_darkness ", 17) == 0) {
            map->lighting.ambient_darkness = (float)atof(p + 17);
        } else if (strncmp(p, "water_level ", 12) == 0) {
            map->water_level = atoi(p + 12);
            map->has_water = true;
        } else if (strncmp(p, "water_color ", 12) == 0) {
            float r, g, b;
            if (sscanf(p + 12, "%f %f %f", &r, &g, &b) == 3) {
                map->water_color = (pz_vec3) { r, g, b };
            }
        }
        // Background settings
        else if (strncmp(p, "background_color ", 17) == 0) {
            float r, g, b;
            if (sscanf(p + 17, "%f %f %f", &r, &g, &b) == 3) {
                map->background.type = PZ_BACKGROUND_COLOR;
                map->background.color = (pz_vec3) { r, g, b };
            }
        } else if (strncmp(p, "background_gradient ", 20) == 0) {
            // Format: background_gradient <direction> r1 g1 b1 r2 g2 b2
            // direction: vertical or radial
            char dir[16];
            float r1, g1, b1, r2, g2, b2;
            if (sscanf(p + 20, "%15s %f %f %f %f %f %f", dir, &r1, &g1, &b1,
                    &r2, &g2, &b2)
                == 7) {
                map->background.type = PZ_BACKGROUND_GRADIENT;
                map->background.color = (pz_vec3) { r1, g1, b1 };
                map->background.color_end = (pz_vec3) { r2, g2, b2 };
                if (strcmp(dir, "radial") == 0) {
                    map->background.gradient_dir = PZ_GRADIENT_RADIAL;
                } else {
                    map->background.gradient_dir = PZ_GRADIENT_VERTICAL;
                }
            }
        } else if (strncmp(p, "background_texture ", 19) == 0) {
            // Future: background_texture <path>
            map->background.type = PZ_BACKGROUND_TEXTURE;
            sscanf(p + 19, "%63s", map->background.texture_path);
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
                "Background textures not yet implemented: %s",
                map->background.texture_path);
        }
    } while (1);

    pz_free(file_data);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Loaded map v%d: %s (%s)",
        map->version, map->name, path);

    return map;
}

pz_map *
pz_map_load_with_registry(const char *path, const pz_tile_registry *registry)
{
    pz_map *map = pz_map_load(path);
    if (map && registry) {
        pz_map_set_tile_registry(map, registry);
    }
    return map;
}

bool
pz_map_save(const pz_map *map, const char *path)
{
    if (!map || !path) {
        return false;
    }

    // Calculate buffer size
    // Each cell needs up to 8 chars (e.g., "-10.|P1 "), plus row overhead
    size_t buf_size = 2048 + (size_t)(map->width * 10 + 4) * (size_t)map->height
        + (size_t)map->spawn_count * 128 + (size_t)map->enemy_count * 128;
    char *buf = pz_alloc(buf_size);
    if (!buf) {
        return false;
    }

    char *p = buf;
    int remaining = (int)buf_size;
    int written;

    // Header
    written = snprintf(p, remaining,
        "# Tank Game Map\n"
        "version 2\n"
        "name %s\n"
        "size %d %d\n"
        "tile_size %.1f\n"
        "\n",
        map->name, map->width, map->height, map->tile_size);
    p += written;
    remaining -= written;

    // Tile definitions
    written = snprintf(p, remaining, "# Tile definitions\n");
    p += written;
    remaining -= written;

    for (int i = 0; i < map->tile_def_count; i++) {
        written = snprintf(p, remaining, "tile %c %s\n",
            map->tile_defs[i].symbol, map->tile_defs[i].name);
        p += written;
        remaining -= written;
    }

    // Water level
    if (map->has_water) {
        written
            = snprintf(p, remaining, "\nwater_level %d\n", map->water_level);
        p += written;
        remaining -= written;

        written = snprintf(p, remaining, "water_color %.2f %.2f %.2f\n",
            map->water_color.x, map->water_color.y, map->water_color.z);
        p += written;
        remaining -= written;
    }

    // For v2 save, we output spawns/enemies after the grid
    // (In a more complete implementation, we'd track which ones
    // came from tags and which from explicit coordinates)

    // Grid
    written = snprintf(p, remaining, "\ngrid\n");
    p += written;
    remaining -= written;

    // Determine cell width for alignment
    int max_cell_width = 2; // minimum "0."
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            pz_map_cell cell = pz_map_get_cell(map, x, y);
            char tmp[16];
            int len = snprintf(tmp, sizeof(tmp), "%d", cell.height);
            len += 1; // tile char
            if (len > max_cell_width)
                max_cell_width = len;
        }
    }

    // Write grid rows (top to bottom in file = high Y to low Y)
    for (int y = map->height - 1; y >= 0; y--) {
        for (int x = 0; x < map->width; x++) {
            pz_map_cell cell = pz_map_get_cell(map, x, y);
            const pz_tile_def *def
                = pz_map_get_tile_def_by_index(map, cell.tile_index);
            char symbol = def ? def->symbol : '.';

            char cell_str[16];
            snprintf(cell_str, sizeof(cell_str), "%d%c", cell.height, symbol);

            // Pad to max_cell_width
            written = snprintf(p, remaining, "%*s ", max_cell_width, cell_str);
            p += written;
            remaining -= written;
        }
        *p++ = '\n';
        remaining--;
    }

    // Spawn points (using explicit coordinates for now)
    if (map->spawn_count > 0) {
        written = snprintf(p, remaining, "\n# Spawn points\n");
        p += written;
        remaining -= written;

        for (int i = 0; i < map->spawn_count; i++) {
            const pz_spawn_point *sp = &map->spawns[i];
            written = snprintf(p, remaining, "spawn %.2f %.2f %.3f %d %d\n",
                sp->pos.x, sp->pos.y, sp->angle, sp->team,
                sp->team_spawn ? 1 : 0);
            p += written;
            remaining -= written;
        }
    }

    // Enemy spawns
    if (map->enemy_count > 0) {
        written = snprintf(p, remaining, "\n# Enemy spawns\n");
        p += written;
        remaining -= written;

        for (int i = 0; i < map->enemy_count; i++) {
            const pz_enemy_spawn *es = &map->enemies[i];
            written = snprintf(p, remaining, "enemy %.2f %.2f %.3f %d\n",
                es->pos.x, es->pos.y, es->angle, es->level);
            p += written;
            remaining -= written;
        }
    }

    // Lighting
    written = snprintf(p, remaining, "\n# Lighting\n");
    p += written;
    remaining -= written;

    if (map->lighting.has_sun) {
        written = snprintf(p, remaining, "sun_direction %.2f %.2f %.2f\n",
            map->lighting.sun_direction.x, map->lighting.sun_direction.y,
            map->lighting.sun_direction.z);
        p += written;
        remaining -= written;

        written = snprintf(p, remaining, "sun_color %.2f %.2f %.2f\n",
            map->lighting.sun_color.x, map->lighting.sun_color.y,
            map->lighting.sun_color.z);
        p += written;
        remaining -= written;
    }

    written = snprintf(p, remaining, "ambient_color %.2f %.2f %.2f\n",
        map->lighting.ambient_color.x, map->lighting.ambient_color.y,
        map->lighting.ambient_color.z);
    p += written;
    remaining -= written;

    written = snprintf(p, remaining, "ambient_darkness %.2f\n",
        map->lighting.ambient_darkness);
    p += written;
    remaining -= written;

    *p = '\0';

    bool success = pz_file_write_text(path, buf);
    pz_free(buf);

    if (success) {
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Saved map v2: %s", path);
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
