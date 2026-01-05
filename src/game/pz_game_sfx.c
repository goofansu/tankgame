/*
 * Tank Game - Game Sound Effects Controller
 */

#include "pz_game_sfx.h"

#include <math.h>
#include <string.h>

#include "core/pz_log.h"
#include "core/pz_mem.h"
#include "engine/pz_sfx.h"
#include "pz_tank.h"

// Engine sound state per tank
typedef struct {
    int tank_id;
    pz_sfx_handle engine_handle;
    bool was_moving;
    bool active;
} tank_engine_state;

struct pz_game_sfx {
    pz_sfx_manager *sfx;

    // Engine sounds tracked per tank
    tank_engine_state engine_states[PZ_MAX_TANKS];

    float master_volume;
};

// Volume levels for different sounds
#define GUNFIRE_VOLUME 1.0f
#define BULLET_HIT_VOLUME 0.5f
#define EXPLOSION_VOLUME 0.6f
#define FINAL_EXPLOSION_VOLUME 0.4f
#define ENGINE_VOLUME 0.55f

// Speed threshold for moving vs idle
#define MOVING_SPEED_THRESHOLD 0.3f

pz_game_sfx *
pz_game_sfx_create(int sample_rate)
{
    pz_game_sfx *gsfx
        = (pz_game_sfx *)pz_calloc_tagged(1, sizeof(pz_game_sfx), PZ_MEM_AUDIO);
    if (!gsfx) {
        return NULL;
    }

    gsfx->sfx = pz_sfx_create(sample_rate);
    if (!gsfx->sfx) {
        pz_free(gsfx);
        return NULL;
    }

    gsfx->master_volume = 1.0f;

    // Initialize engine states as inactive
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        gsfx->engine_states[i].tank_id = -1;
        gsfx->engine_states[i].engine_handle = PZ_SFX_INVALID_HANDLE;
        gsfx->engine_states[i].was_moving = false;
        gsfx->engine_states[i].active = false;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_AUDIO, "Game SFX system initialized");

    return gsfx;
}

void
pz_game_sfx_destroy(pz_game_sfx *gsfx)
{
    if (!gsfx) {
        return;
    }

    pz_sfx_destroy(gsfx->sfx);
    pz_free(gsfx);
}

// Find engine state slot for a tank, or allocate new one
static tank_engine_state *
find_or_create_engine_state(pz_game_sfx *gsfx, int tank_id)
{
    // Look for existing
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        if (gsfx->engine_states[i].active
            && gsfx->engine_states[i].tank_id == tank_id) {
            return &gsfx->engine_states[i];
        }
    }

    // Find free slot
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        if (!gsfx->engine_states[i].active) {
            gsfx->engine_states[i].tank_id = tank_id;
            gsfx->engine_states[i].engine_handle = PZ_SFX_INVALID_HANDLE;
            gsfx->engine_states[i].was_moving = false;
            gsfx->engine_states[i].active = true;
            return &gsfx->engine_states[i];
        }
    }

    return NULL;
}

void
pz_game_sfx_update_engines(pz_game_sfx *gsfx, pz_tank_manager *tanks)
{
    if (!gsfx || !tanks) {
        return;
    }

    // Mark all engine states for cleanup check
    bool tank_still_exists[PZ_MAX_TANKS] = { false };

    // Update engine sounds for each active tank
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        pz_tank *tank = &tanks->tanks[i];

        // Skip inactive or dead tanks
        if (!(tank->flags & PZ_TANK_FLAG_ACTIVE)
            || (tank->flags & PZ_TANK_FLAG_DEAD)) {
            continue;
        }

        tank_engine_state *state = find_or_create_engine_state(gsfx, tank->id);
        if (!state) {
            continue;
        }

        // Mark this tank as still existing
        for (int j = 0; j < PZ_MAX_TANKS; j++) {
            if (gsfx->engine_states[j].active
                && gsfx->engine_states[j].tank_id == tank->id) {
                tank_still_exists[j] = true;
                break;
            }
        }

        // Check if tank is moving
        float speed
            = sqrtf(tank->vel.x * tank->vel.x + tank->vel.y * tank->vel.y);
        bool is_moving = speed > MOVING_SPEED_THRESHOLD;

        // Determine which sound to play
        pz_sfx_id wanted_sound
            = is_moving ? PZ_SFX_ENGINE_MOVING : PZ_SFX_ENGINE_IDLE;
        pz_sfx_id current_sound
            = state->was_moving ? PZ_SFX_ENGINE_MOVING : PZ_SFX_ENGINE_IDLE;

        // Switch sound if state changed, or start sound if none playing
        bool need_new_sound = false;

        if (state->engine_handle == PZ_SFX_INVALID_HANDLE) {
            need_new_sound = true;
        } else if (!pz_sfx_is_playing(gsfx->sfx, state->engine_handle)) {
            need_new_sound = true;
        } else if (is_moving != state->was_moving) {
            // State changed, switch sound
            pz_sfx_stop(gsfx->sfx, state->engine_handle);
            need_new_sound = true;
        }

        if (need_new_sound) {
            state->engine_handle
                = pz_sfx_play_loop(gsfx->sfx, wanted_sound, ENGINE_VOLUME);
            state->was_moving = is_moving;
        }
    }

    // Clean up engine states for tanks that no longer exist
    for (int i = 0; i < PZ_MAX_TANKS; i++) {
        if (gsfx->engine_states[i].active && !tank_still_exists[i]) {
            // Stop the engine sound
            if (gsfx->engine_states[i].engine_handle != PZ_SFX_INVALID_HANDLE) {
                pz_sfx_stop(gsfx->sfx, gsfx->engine_states[i].engine_handle);
            }
            gsfx->engine_states[i].active = false;
            gsfx->engine_states[i].tank_id = -1;
            gsfx->engine_states[i].engine_handle = PZ_SFX_INVALID_HANDLE;
        }
    }
}

void
pz_game_sfx_play_gunfire(pz_game_sfx *gsfx)
{
    if (!gsfx) {
        return;
    }
    pz_sfx_play(gsfx->sfx, PZ_SFX_GUN_FIRE, GUNFIRE_VOLUME);
}

void
pz_game_sfx_play_bullet_hit(pz_game_sfx *gsfx)
{
    if (!gsfx) {
        return;
    }
    pz_sfx_play(gsfx->sfx, PZ_SFX_BULLET_HIT, BULLET_HIT_VOLUME);
}

void
pz_game_sfx_play_tank_explosion(pz_game_sfx *gsfx, bool is_final)
{
    if (!gsfx) {
        return;
    }

    if (is_final) {
        pz_sfx_play(gsfx->sfx, PZ_SFX_EXPLOSION_FINAL, FINAL_EXPLOSION_VOLUME);
    } else {
        pz_sfx_play(gsfx->sfx, PZ_SFX_EXPLOSION_TANK, EXPLOSION_VOLUME);
    }
}

#define TANK_HIT_VOLUME 0.6f
#define RICOCHET_VOLUME 0.5f

void
pz_game_sfx_play_tank_hit(pz_game_sfx *gsfx)
{
    if (!gsfx) {
        return;
    }
    pz_sfx_play(gsfx->sfx, PZ_SFX_TANK_HIT, TANK_HIT_VOLUME);
}

void
pz_game_sfx_play_ricochet(pz_game_sfx *gsfx)
{
    if (!gsfx) {
        return;
    }
    pz_sfx_play(gsfx->sfx, PZ_SFX_GUN_FIRE, RICOCHET_VOLUME);
}

void
pz_game_sfx_play_plop(pz_game_sfx *gsfx)
{
    if (!gsfx) {
        return;
    }
    pz_sfx_play(gsfx->sfx, PZ_SFX_PLOP, 0.7f);
}

void
pz_game_sfx_set_volume(pz_game_sfx *gsfx, float volume)
{
    if (!gsfx) {
        return;
    }
    if (volume < 0.0f)
        volume = 0.0f;
    if (volume > 1.0f)
        volume = 1.0f;
    gsfx->master_volume = volume;
    pz_sfx_set_master_volume(gsfx->sfx, volume);
}

void
pz_game_sfx_render(
    pz_game_sfx *gsfx, float *buffer, int num_frames, int num_channels)
{
    if (!gsfx) {
        return;
    }
    pz_sfx_render(gsfx->sfx, buffer, num_frames, num_channels);
}
