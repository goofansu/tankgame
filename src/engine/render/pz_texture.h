/*
 * Tank Game - Texture Loading and Management
 *
 * Higher-level texture loading with caching and hot-reload support.
 */

#ifndef PZ_TEXTURE_H
#define PZ_TEXTURE_H

#include "pz_renderer.h"
#include <stdbool.h>

/* ============================================================================
 * Texture Manager
 * ============================================================================
 */

typedef struct pz_texture_manager pz_texture_manager;

// Create/destroy texture manager
pz_texture_manager *pz_texture_manager_create(pz_renderer *renderer);
void pz_texture_manager_destroy(pz_texture_manager *tm);

// Load texture from file (cached - same path returns same handle)
pz_texture_handle pz_texture_load(pz_texture_manager *tm, const char *path);

// Load texture with specific settings
pz_texture_handle pz_texture_load_ex(pz_texture_manager *tm, const char *path,
    pz_texture_filter filter, pz_texture_wrap wrap);

// Reload a texture from disk (for hot-reload)
bool pz_texture_reload(pz_texture_manager *tm, pz_texture_handle handle);

// Check for and reload any modified textures
void pz_texture_check_hot_reload(pz_texture_manager *tm);

// Get texture info
bool pz_texture_get_size(
    pz_texture_manager *tm, pz_texture_handle handle, int *width, int *height);
const char *pz_texture_get_path(
    pz_texture_manager *tm, pz_texture_handle handle);

// Unload a specific texture
void pz_texture_unload(pz_texture_manager *tm, pz_texture_handle handle);

// Unload all textures
void pz_texture_unload_all(pz_texture_manager *tm);

/* ============================================================================
 * Low-level Loading (no caching)
 * ============================================================================
 */

// Load image data from file
// Returns pixel data (RGBA, caller must free with pz_free)
// Sets width, height, channels
unsigned char *pz_image_load(
    const char *path, int *width, int *height, int *channels);

// Free image data
void pz_image_free(unsigned char *data);

#endif // PZ_TEXTURE_H
