/*
 * Tank Game - Texture Loading and Management Implementation
 */

#include "pz_texture.h"
#include "../../core/pz_ds.h"
#include "../../core/pz_log.h"
#include "../../core/pz_mem.h"
#include "../../core/pz_platform.h"
#include "../../core/pz_str.h"

#include "third_party/stb_image.h"

#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================
 */

#define MAX_CACHED_TEXTURES 256

/* ============================================================================
 * Cached Texture Entry
 * ============================================================================
 */

typedef struct cached_texture {
    pz_texture_handle handle;
    char *path;
    int width;
    int height;
    pz_texture_filter filter;
    pz_texture_wrap wrap;
    uint64_t mtime; // Last modification time
    bool used;
} cached_texture;

/* ============================================================================
 * Texture Manager
 * ============================================================================
 */

struct pz_texture_manager {
    pz_renderer *renderer;
    cached_texture textures[MAX_CACHED_TEXTURES];
    pz_hashmap path_to_index; // path -> index in textures array
};

/* ============================================================================
 * Low-level Loading
 * ============================================================================
 */

unsigned char *
pz_image_load(const char *path, int *width, int *height, int *channels)
{
    // stbi_load returns RGBA if we request 4 channels
    unsigned char *data = stbi_load(path, width, height, channels, 4);
    if (!data) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER, "Failed to load image: %s (%s)",
            path, stbi_failure_reason());
        return NULL;
    }

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Loaded image: %s (%dx%d, %d ch)",
        path, *width, *height, *channels);

    return data;
}

void
pz_image_free(unsigned char *data)
{
    stbi_image_free(data);
}

/* ============================================================================
 * Texture Manager Implementation
 * ============================================================================
 */

pz_texture_manager *
pz_texture_manager_create(pz_renderer *renderer)
{
    pz_texture_manager *tm = pz_calloc(1, sizeof(pz_texture_manager));
    if (!tm) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to allocate texture manager");
        return NULL;
    }

    tm->renderer = renderer;
    pz_hashmap_init(&tm->path_to_index, 64);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Texture manager created");
    return tm;
}

void
pz_texture_manager_destroy(pz_texture_manager *tm)
{
    if (!tm)
        return;

    pz_texture_unload_all(tm);
    pz_hashmap_destroy(&tm->path_to_index);
    pz_free(tm);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Texture manager destroyed");
}

static int
find_free_slot(pz_texture_manager *tm)
{
    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        if (!tm->textures[i].used) {
            return i;
        }
    }
    return -1;
}

pz_texture_handle
pz_texture_load(pz_texture_manager *tm, const char *path)
{
    return pz_texture_load_ex(tm, path, PZ_FILTER_LINEAR, PZ_WRAP_REPEAT);
}

pz_texture_handle
pz_texture_load_ex(pz_texture_manager *tm, const char *path,
    pz_texture_filter filter, pz_texture_wrap wrap)
{
    if (!tm || !path)
        return PZ_INVALID_HANDLE;

    // Check if already loaded
    void *existing = pz_hashmap_get(&tm->path_to_index, path);
    if (existing) {
        int index = (int)(intptr_t)existing - 1; // We store index+1 to avoid 0
        return tm->textures[index].handle;
    }

    // Find free slot
    int slot = find_free_slot(tm);
    if (slot < 0) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Texture cache full, cannot load: %s", path);
        return PZ_INVALID_HANDLE;
    }

    // Load image
    int width, height, channels;
    unsigned char *data = pz_image_load(path, &width, &height, &channels);
    if (!data) {
        return PZ_INVALID_HANDLE;
    }

    // Create texture
    pz_texture_desc desc = {
        .width = width,
        .height = height,
        .format = PZ_TEXTURE_RGBA8,
        .filter = filter,
        .wrap = wrap,
        .data = data,
    };

    pz_texture_handle handle = pz_renderer_create_texture(tm->renderer, &desc);

    // Free image data after upload
    pz_image_free(data);

    if (handle == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to create GPU texture for: %s", path);
        return PZ_INVALID_HANDLE;
    }

    // Cache the texture
    cached_texture *cached = &tm->textures[slot];
    cached->handle = handle;
    cached->path = pz_str_dup(path);
    cached->width = width;
    cached->height = height;
    cached->filter = filter;
    cached->wrap = wrap;
    cached->mtime = (uint64_t)pz_file_mtime(path);
    cached->used = true;

    // Add to lookup table (store index+1 to avoid 0 being interpreted as NULL)
    pz_hashmap_set(&tm->path_to_index, path, (void *)(intptr_t)(slot + 1));

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Cached texture: %s (slot=%d)",
        path, slot);

    return handle;
}

bool
pz_texture_reload(pz_texture_manager *tm, pz_texture_handle handle)
{
    if (!tm || handle == PZ_INVALID_HANDLE)
        return false;

    // Find the cached texture
    cached_texture *cached = NULL;
    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        if (tm->textures[i].used && tm->textures[i].handle == handle) {
            cached = &tm->textures[i];
            break;
        }
    }

    if (!cached || !cached->path) {
        return false;
    }

    // Load new image data
    int width, height, channels;
    unsigned char *data
        = pz_image_load(cached->path, &width, &height, &channels);
    if (!data) {
        return false;
    }

    // Check if size changed - if so, we need to recreate the texture
    if (width != cached->width || height != cached->height) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_RENDER,
            "Texture size changed during reload: %s (%dx%d -> %dx%d)",
            cached->path, cached->width, cached->height, width, height);

        // Destroy old texture
        pz_renderer_destroy_texture(tm->renderer, cached->handle);

        // Create new texture
        pz_texture_desc desc = {
            .width = width,
            .height = height,
            .format = PZ_TEXTURE_RGBA8,
            .filter = cached->filter,
            .wrap = cached->wrap,
            .data = data,
        };

        cached->handle = pz_renderer_create_texture(tm->renderer, &desc);
        cached->width = width;
        cached->height = height;
    } else {
        // Update existing texture
        pz_renderer_update_texture(
            tm->renderer, cached->handle, 0, 0, width, height, data);
    }

    pz_image_free(data);

    cached->mtime = (uint64_t)pz_file_mtime(cached->path);

    pz_log(
        PZ_LOG_INFO, PZ_LOG_CAT_RENDER, "Reloaded texture: %s", cached->path);

    return true;
}

void
pz_texture_check_hot_reload(pz_texture_manager *tm)
{
    if (!tm)
        return;

    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        cached_texture *cached = &tm->textures[i];
        if (!cached->used || !cached->path)
            continue;

        uint64_t current_mtime = (uint64_t)pz_file_mtime(cached->path);
        if (current_mtime != cached->mtime && current_mtime != 0) {
            pz_texture_reload(tm, cached->handle);
        }
    }
}

bool
pz_texture_get_size(
    pz_texture_manager *tm, pz_texture_handle handle, int *width, int *height)
{
    if (!tm || handle == PZ_INVALID_HANDLE)
        return false;

    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        if (tm->textures[i].used && tm->textures[i].handle == handle) {
            if (width)
                *width = tm->textures[i].width;
            if (height)
                *height = tm->textures[i].height;
            return true;
        }
    }

    return false;
}

const char *
pz_texture_get_path(pz_texture_manager *tm, pz_texture_handle handle)
{
    if (!tm || handle == PZ_INVALID_HANDLE)
        return NULL;

    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        if (tm->textures[i].used && tm->textures[i].handle == handle) {
            return tm->textures[i].path;
        }
    }

    return NULL;
}

void
pz_texture_unload(pz_texture_manager *tm, pz_texture_handle handle)
{
    if (!tm || handle == PZ_INVALID_HANDLE)
        return;

    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        cached_texture *cached = &tm->textures[i];
        if (cached->used && cached->handle == handle) {
            if (cached->path) {
                pz_hashmap_remove(&tm->path_to_index, cached->path);
                pz_free(cached->path);
            }
            pz_renderer_destroy_texture(tm->renderer, cached->handle);
            memset(cached, 0, sizeof(*cached));
            return;
        }
    }
}

void
pz_texture_unload_all(pz_texture_manager *tm)
{
    if (!tm)
        return;

    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        cached_texture *cached = &tm->textures[i];
        if (cached->used) {
            if (cached->path) {
                pz_free(cached->path);
            }
            pz_renderer_destroy_texture(tm->renderer, cached->handle);
            memset(cached, 0, sizeof(*cached));
        }
    }

    // Clear the hashmap
    pz_hashmap_clear(&tm->path_to_index);
}
