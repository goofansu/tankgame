/*
 * Tank Game - Game Sound Effects Controller
 *
 * Manages game-specific sound effects:
 * - Engine sounds per tank (idle/moving)
 * - Gunfire, explosions
 */

#ifndef PZ_GAME_SFX_H
#define PZ_GAME_SFX_H

#include <stdbool.h>
#include <stdint.h>

typedef struct pz_game_sfx pz_game_sfx;
typedef struct pz_tank_manager pz_tank_manager;

// Create game SFX controller
pz_game_sfx *pz_game_sfx_create(int sample_rate);

// Destroy game SFX controller
void pz_game_sfx_destroy(pz_game_sfx *gsfx);

// Update engine sounds for all tanks
// Call once per frame after tank positions/velocities are updated
void pz_game_sfx_update_engines(pz_game_sfx *gsfx, pz_tank_manager *tanks);

// Play gunfire sound (reduced volume)
void pz_game_sfx_play_gunfire(pz_game_sfx *gsfx);

// Play bullet-hits-bullet explosion
void pz_game_sfx_play_bullet_hit(pz_game_sfx *gsfx);

// Play tank explosion
// is_final: true if this is the last enemy tank
void pz_game_sfx_play_tank_explosion(pz_game_sfx *gsfx, bool is_final);

// Play tank hit sound (hit but not destroyed)
void pz_game_sfx_play_tank_hit(pz_game_sfx *gsfx);

// Play ricochet sound (bullet bounces off wall)
void pz_game_sfx_play_ricochet(pz_game_sfx *gsfx);

// Play plop sound (barrier placement)
void pz_game_sfx_play_plop(pz_game_sfx *gsfx);

// Set master volume for all SFX (0.0 - 1.0)
void pz_game_sfx_set_volume(pz_game_sfx *gsfx, float volume);

// Render audio into buffer (called from audio callback)
void pz_game_sfx_render(
    pz_game_sfx *gsfx, float *buffer, int num_frames, int num_channels);

#endif // PZ_GAME_SFX_H
