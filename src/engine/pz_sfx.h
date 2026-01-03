/*
 * Tank Game - Sound Effects System
 *
 * Loads WAV files and plays them with mixing support.
 * Supports one-shot sounds and looping sounds (for engine noises).
 */

#ifndef PZ_SFX_H
#define PZ_SFX_H

#include <stdbool.h>
#include <stdint.h>

// Predefined sound effect IDs
typedef enum {
    PZ_SFX_NONE = 0,
    PZ_SFX_ENGINE_IDLE, // engine2.wav - tank idle loop
    PZ_SFX_ENGINE_MOVING, // engine1.wav - tank moving loop
    PZ_SFX_GUN_FIRE, // gun1.wav - tank fires
    PZ_SFX_BULLET_HIT, // gun3.wav - bullet hits bullet
    PZ_SFX_EXPLOSION_TANK, // explosion3.wav - tank explodes
    PZ_SFX_EXPLOSION_FINAL, // explosion1.wav - last enemy explodes
    PZ_SFX_TANK_HIT, // hit1.wav - tank hit but not destroyed
    PZ_SFX_COUNT
} pz_sfx_id;

// Handle to a playing sound instance (for stopping loops)
typedef uint32_t pz_sfx_handle;
#define PZ_SFX_INVALID_HANDLE 0

typedef struct pz_sfx_manager pz_sfx_manager;

// Create sound effects manager and load all sounds
pz_sfx_manager *pz_sfx_create(int sample_rate);

// Destroy sound effects manager
void pz_sfx_destroy(pz_sfx_manager *sfx);

// Play a one-shot sound effect
// Returns a handle (can be ignored for one-shots)
pz_sfx_handle pz_sfx_play(pz_sfx_manager *sfx, pz_sfx_id id, float volume);

// Play a looping sound effect
// Returns a handle that must be used to stop the loop
pz_sfx_handle pz_sfx_play_loop(pz_sfx_manager *sfx, pz_sfx_id id, float volume);

// Stop a playing sound by handle
void pz_sfx_stop(pz_sfx_manager *sfx, pz_sfx_handle handle);

// Stop all instances of a sound ID
void pz_sfx_stop_all(pz_sfx_manager *sfx, pz_sfx_id id);

// Check if a handle is still playing
bool pz_sfx_is_playing(pz_sfx_manager *sfx, pz_sfx_handle handle);

// Set volume for a playing sound
void pz_sfx_set_volume(pz_sfx_manager *sfx, pz_sfx_handle handle, float volume);

// Set master volume for all SFX (0.0 - 1.0)
void pz_sfx_set_master_volume(pz_sfx_manager *sfx, float volume);

// Render audio into buffer (called from audio callback)
// Mixes all playing sounds into the buffer
void pz_sfx_render(
    pz_sfx_manager *sfx, float *buffer, int num_frames, int num_channels);

#endif // PZ_SFX_H
