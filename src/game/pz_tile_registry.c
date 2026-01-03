/*
 * Tile Registry System Implementation
 *
 * Loads .tile files and manages tile configurations.
 */

#include "pz_tile_registry.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"
#include "../core/pz_str.h"

// Fallback tile name
#define FALLBACK_TILE_NAME "__fallback__"

struct pz_tile_registry {
    pz_tile_config tiles[PZ_TILE_REGISTRY_MAX_TILES];
    int tile_count;

    // Fallback tile for missing/invalid tiles
    pz_tile_config fallback;
    pz_texture_handle fallback_texture;
};

// ============================================================================
// Fallback Texture
// ============================================================================

// Fallback texture path - an orange checkerboard for missing tiles
#define FALLBACK_TEXTURE_PATH "assets/textures/fallback.png"

// Load or generate the fallback texture
static pz_texture_handle
load_fallback_texture(pz_texture_manager *tex_manager)
{
    // Try to load the fallback texture file
    pz_texture_handle tex = pz_texture_load(tex_manager, FALLBACK_TEXTURE_PATH);
    if (tex != PZ_INVALID_HANDLE) {
        return tex;
    }

    // If no fallback texture exists, log warning
    // The system will still work, just with missing textures
    pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "No fallback texture found at %s",
        FALLBACK_TEXTURE_PATH);
    return PZ_INVALID_HANDLE;
}

// ============================================================================
// Tile File Parsing
// ============================================================================

// Parse a single .tile file
static bool
parse_tile_file(const char *path, pz_tile_config *config)
{
    size_t size = 0;
    char *content = pz_file_read(path, &size);

    if (!content) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to read tile file: %s",
            path);
        return false;
    }

    // Initialize with defaults
    memset(config, 0, sizeof(*config));
    config->speed_multiplier = 1.0f;
    config->friction = 1.0f;
    config->valid = false;
    config->ground_texture = PZ_INVALID_HANDLE;
    config->wall_texture = PZ_INVALID_HANDLE;
    config->wall_side_texture = PZ_INVALID_HANDLE;

    // Parse line by line
    char *line = content;
    char *end = content + size;

    while (line < end) {
        // Find end of line
        char *eol = line;
        while (eol < end && *eol != '\n' && *eol != '\r') {
            eol++;
        }

        // Null-terminate the line
        char saved = *eol;
        *eol = '\0';

        // Skip whitespace
        while (*line == ' ' || *line == '\t') {
            line++;
        }

        // Skip empty lines and comments
        if (*line == '\0' || *line == '#') {
            *eol = saved;
            line = eol + 1;
            if (line < end && *(line - 1) == '\r' && *line == '\n') {
                line++;
            }
            continue;
        }

        // Parse key-value pair
        char key[64] = { 0 };
        char value[256] = { 0 };

        // Find the key (first word)
        char *key_end = line;
        while (*key_end && *key_end != ' ' && *key_end != '\t') {
            key_end++;
        }

        size_t key_len = key_end - line;
        if (key_len >= sizeof(key)) {
            key_len = sizeof(key) - 1;
        }
        strncpy(key, line, key_len);

        // Skip whitespace after key
        char *val_start = key_end;
        while (*val_start == ' ' || *val_start == '\t') {
            val_start++;
        }

        // Copy value (rest of line)
        strncpy(value, val_start, sizeof(value) - 1);

        // Trim trailing whitespace from value
        size_t val_len = strlen(value);
        while (val_len > 0
            && (value[val_len - 1] == ' ' || value[val_len - 1] == '\t')) {
            value[--val_len] = '\0';
        }

        // Process known keys
        if (strcmp(key, "name") == 0) {
            strncpy(config->name, value, sizeof(config->name) - 1);
        } else if (strcmp(key, "ground_texture") == 0) {
            strncpy(config->ground_texture_path, value,
                sizeof(config->ground_texture_path) - 1);
        } else if (strcmp(key, "wall_texture") == 0) {
            strncpy(config->wall_texture_path, value,
                sizeof(config->wall_texture_path) - 1);
        } else if (strcmp(key, "wall_side_texture") == 0) {
            strncpy(config->wall_side_texture_path, value,
                sizeof(config->wall_side_texture_path) - 1);
        } else if (strcmp(key, "speed_multiplier") == 0) {
            config->speed_multiplier = (float)atof(value);
        } else if (strcmp(key, "friction") == 0) {
            config->friction = (float)atof(value);
        }
        // Ignore unknown keys (forward compatibility)

        // Move to next line
        *eol = saved;
        line = eol + 1;
        if (line < end && *(line - 1) == '\r' && *line == '\n') {
            line++;
        }
    }

    pz_free(content);

    // Validate required fields
    if (config->name[0] == '\0') {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Tile file missing 'name': %s",
            path);
        return false;
    }

    if (config->ground_texture_path[0] == '\0') {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
            "Tile '%s' missing 'ground_texture': %s", config->name, path);
        return false;
    }

    // Apply defaults for wall textures
    if (config->wall_texture_path[0] == '\0') {
        strncpy(config->wall_texture_path, config->ground_texture_path,
            sizeof(config->wall_texture_path) - 1);
    }
    if (config->wall_side_texture_path[0] == '\0') {
        strncpy(config->wall_side_texture_path, config->wall_texture_path,
            sizeof(config->wall_side_texture_path) - 1);
    }

    config->valid = true;
    return true;
}

// ============================================================================
// Registry Management
// ============================================================================

pz_tile_registry *
pz_tile_registry_create(void)
{
    pz_tile_registry *registry = pz_calloc(1, sizeof(pz_tile_registry));
    if (!registry) {
        return NULL;
    }

    registry->tile_count = 0;
    registry->fallback_texture = PZ_INVALID_HANDLE;

    // Initialize fallback tile
    memset(&registry->fallback, 0, sizeof(registry->fallback));
    strncpy(registry->fallback.name, FALLBACK_TILE_NAME,
        sizeof(registry->fallback.name) - 1);
    registry->fallback.speed_multiplier = 1.0f;
    registry->fallback.friction = 1.0f;
    registry->fallback.valid = true;

    return registry;
}

void
pz_tile_registry_destroy(pz_tile_registry *registry)
{
    if (!registry) {
        return;
    }

    // Note: textures are owned by texture_manager, not us
    pz_free(registry);
}

int
pz_tile_registry_load_all(pz_tile_registry *registry,
    pz_texture_manager *tex_manager, const char *tiles_dir)
{
    if (!registry || !tiles_dir) {
        return 0;
    }

    // Load fallback texture first
    if (tex_manager) {
        registry->fallback_texture = load_fallback_texture(tex_manager);
        registry->fallback.ground_texture = registry->fallback_texture;
        registry->fallback.wall_texture = registry->fallback_texture;
        registry->fallback.wall_side_texture = registry->fallback_texture;
    }

    // Open directory
    DIR *dir = opendir(tiles_dir);
    if (!dir) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Could not open tiles directory: %s", tiles_dir);
        return 0;
    }

    int loaded = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip non-.tile files
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 6 || strcmp(name + len - 5, ".tile") != 0) {
            continue;
        }

        // Build full path
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", tiles_dir, name);

        // Check capacity
        if (registry->tile_count >= PZ_TILE_REGISTRY_MAX_TILES) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
                "Tile registry full, skipping: %s", path);
            continue;
        }

        // Parse tile file
        pz_tile_config *config = &registry->tiles[registry->tile_count];
        if (parse_tile_file(path, config)) {
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                "Loaded tile: %s (speed=%.1f, friction=%.1f)", config->name,
                config->speed_multiplier, config->friction);
            registry->tile_count++;
            loaded++;
        } else {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
                "Failed to parse tile file: %s", path);
        }
    }

    closedir(dir);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Loaded %d tiles from %s", loaded,
        tiles_dir);

    // Load textures for all tiles
    if (tex_manager) {
        pz_tile_registry_load_textures(registry, tex_manager);
    }

    return loaded;
}

void
pz_tile_registry_load_textures(
    pz_tile_registry *registry, pz_texture_manager *tex_manager)
{
    if (!registry || !tex_manager) {
        return;
    }

    for (int i = 0; i < registry->tile_count; i++) {
        pz_tile_config *config = &registry->tiles[i];

        // Load ground texture (required)
        config->ground_texture
            = pz_texture_load(tex_manager, config->ground_texture_path);
        if (config->ground_texture == PZ_INVALID_HANDLE) {
            pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
                "Failed to load ground texture for tile '%s': %s", config->name,
                config->ground_texture_path);
            config->valid = false;
            config->ground_texture = registry->fallback_texture;
            config->wall_texture = registry->fallback_texture;
            config->wall_side_texture = registry->fallback_texture;
            continue;
        }

        // Load wall texture (defaults to ground)
        if (strcmp(config->wall_texture_path, config->ground_texture_path)
            == 0) {
            config->wall_texture = config->ground_texture;
        } else {
            config->wall_texture
                = pz_texture_load(tex_manager, config->wall_texture_path);
            if (config->wall_texture == PZ_INVALID_HANDLE) {
                pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
                    "Failed to load wall texture for tile '%s', using ground",
                    config->name);
                config->wall_texture = config->ground_texture;
            }
        }

        // Load wall side texture (defaults to wall)
        if (strcmp(config->wall_side_texture_path, config->wall_texture_path)
            == 0) {
            config->wall_side_texture = config->wall_texture;
        } else {
            config->wall_side_texture
                = pz_texture_load(tex_manager, config->wall_side_texture_path);
            if (config->wall_side_texture == PZ_INVALID_HANDLE) {
                pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
                    "Failed to load wall side texture for tile '%s', using "
                    "wall",
                    config->name);
                config->wall_side_texture = config->wall_texture;
            }
        }
    }
}

const pz_tile_config *
pz_tile_registry_get(const pz_tile_registry *registry, const char *name)
{
    if (!registry || !name) {
        return NULL;
    }

    for (int i = 0; i < registry->tile_count; i++) {
        if (strcmp(registry->tiles[i].name, name) == 0) {
            // Return fallback if tile is invalid
            if (!registry->tiles[i].valid) {
                return &registry->fallback;
            }
            return &registry->tiles[i];
        }
    }

    pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "Tile not found: '%s', using fallback",
        name);
    return &registry->fallback;
}

const pz_tile_config *
pz_tile_registry_get_fallback(const pz_tile_registry *registry)
{
    if (!registry) {
        return NULL;
    }
    return &registry->fallback;
}

int
pz_tile_registry_count(const pz_tile_registry *registry)
{
    return registry ? registry->tile_count : 0;
}

const pz_tile_config *
pz_tile_registry_get_by_index(const pz_tile_registry *registry, int index)
{
    if (!registry || index < 0 || index >= registry->tile_count) {
        return NULL;
    }
    return &registry->tiles[index];
}

void
pz_tile_registry_print(const pz_tile_registry *registry)
{
    if (!registry) {
        return;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
        "=== Tile Registry (%d tiles) ===", registry->tile_count);

    for (int i = 0; i < registry->tile_count; i++) {
        const pz_tile_config *t = &registry->tiles[i];
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
            "  [%d] %s: ground=%s, wall=%s, side=%s, speed=%.1f, "
            "friction=%.1f%s",
            i, t->name, t->ground_texture_path, t->wall_texture_path,
            t->wall_side_texture_path, t->speed_multiplier, t->friction,
            t->valid ? "" : " [INVALID]");
    }
}
