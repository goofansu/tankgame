/*
 * Tank Game - Game-level music controller
 */

#include "pz_game_music.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "engine/pz_music.h"
#include "pz_musicset.h"

typedef enum {
    MUSIC_STATE_PLAYING,
    MUSIC_STATE_FADING_OUT,
    MUSIC_STATE_VICTORY,
    MUSIC_STATE_STOPPED,
} pz_game_music_state;

struct pz_game_music {
    pz_music *music;
    pz_music *victory_music;
    pz_musicset *musicset;
    char soundfont_path[256];

    float bpm;
    float beat_duration_ms;
    double current_time_ms;
    float beat_threshold_ms;

    bool pending_intensity1;
    bool pending_intensity2;
    bool has_pending_changes;
    bool current_intensity1;
    bool current_intensity2;

    pz_game_music_state state;
    float fade_timer;
    float fade_duration;

    float master_volume;
};

static void
pz_game_music_reset_state(pz_game_music *gm)
{
    gm->state = MUSIC_STATE_STOPPED;
    gm->fade_timer = 0.0f;
    gm->fade_duration = 0.5f;
    gm->bpm = 120.0f;
    gm->beat_duration_ms = 60000.0f / gm->bpm;
    gm->current_time_ms = 0.0;
    gm->beat_threshold_ms = 50.0f;
    gm->pending_intensity1 = false;
    gm->pending_intensity2 = false;
    gm->has_pending_changes = false;
    gm->current_intensity1 = false;
    gm->current_intensity2 = false;
}

static void
pz_game_music_destroy_loaded(pz_game_music *gm)
{
    if (gm->music) {
        pz_music_destroy(gm->music);
        gm->music = NULL;
    }
    if (gm->victory_music) {
        pz_music_destroy(gm->victory_music);
        gm->victory_music = NULL;
    }
    if (gm->musicset) {
        pz_musicset_destroy(gm->musicset);
        gm->musicset = NULL;
    }
}

static void
pz_game_music_apply_role(pz_game_music *gm, pz_music_role role, bool enabled)
{
    if (!gm || !gm->music || !gm->musicset) {
        return;
    }

    for (int i = 0; i < gm->musicset->layer_count; i++) {
        const pz_musicset_layer *layer = &gm->musicset->layers[i];
        if (layer->role == role) {
            pz_music_set_layer_enabled(gm->music, i, enabled);
        }
    }
}

pz_game_music *
pz_game_music_create(const char *soundfont_path)
{
    if (!soundfont_path) {
        return NULL;
    }

    pz_game_music *gm = (pz_game_music *)pz_calloc_tagged(
        1, sizeof(pz_game_music), PZ_MEM_AUDIO);
    if (!gm) {
        return NULL;
    }

    strncpy(gm->soundfont_path, soundfont_path, sizeof(gm->soundfont_path) - 1);
    gm->soundfont_path[sizeof(gm->soundfont_path) - 1] = '\0';
    gm->master_volume = 0.6f;
    pz_game_music_reset_state(gm);

    return gm;
}

void
pz_game_music_destroy(pz_game_music *gm)
{
    if (!gm) {
        return;
    }
    pz_game_music_destroy_loaded(gm);
    pz_free(gm);
}

bool
pz_game_music_load(pz_game_music *gm, const char *musicset_name)
{
    if (!gm) {
        return false;
    }

    if (!musicset_name || !*musicset_name) {
        pz_game_music_stop(gm);
        return true;
    }

    char path[256];
    snprintf(path, sizeof(path), "assets/music/%s.musicset", musicset_name);

    pz_musicset *set = pz_musicset_load(path);
    if (!set) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "Musicset not found: %s", path);
        pz_game_music_stop(gm);
        return false;
    }

    pz_game_music_destroy_loaded(gm);
    gm->musicset = set;

    pz_music_config config = {
        .soundfont_path = gm->soundfont_path,
        .layer_count = set->layer_count,
        .master_volume = gm->master_volume,
    };

    for (int i = 0; i < set->layer_count; i++) {
        const pz_musicset_layer *layer = &set->layers[i];
        config.layers[i] = (pz_music_layer_config) {
            .midi_path = layer->midi_path,
            .midi_channel = layer->channel,
            .volume = layer->volume,
            .enabled = (layer->role == PZ_MUSIC_ROLE_BASE),
            .loop = true,
        };
    }

    gm->music = pz_music_create(&config);
    if (!gm->music) {
        pz_musicset_destroy(set);
        gm->musicset = NULL;
        return false;
    }

    if (set->has_victory) {
        pz_music_config victory_config = {
            .soundfont_path = gm->soundfont_path,
            .layer_count = 1,
            .master_volume = gm->master_volume,
            .layers = {
                {
                    .midi_path = set->victory_path,
                    .midi_channel = set->victory_channel,
                    .volume = 1.0f,
                    .enabled = true,
                    .loop = false,
                },
            },
        };
        gm->victory_music = pz_music_create(&victory_config);
        if (!gm->victory_music) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO,
                "Failed to load victory music: %s", set->victory_path);
        }
    }

    pz_game_music_reset_state(gm);
    gm->bpm = set->bpm;
    if (gm->bpm <= 0.0f) {
        gm->bpm = 120.0f;
    }
    gm->beat_duration_ms = 60000.0f / gm->bpm;

    pz_music_play(gm->music);
    gm->state = MUSIC_STATE_PLAYING;

    return true;
}

void
pz_game_music_stop(pz_game_music *gm)
{
    if (!gm) {
        return;
    }
    if (gm->music) {
        pz_music_stop(gm->music);
    }
    if (gm->victory_music) {
        pz_music_stop(gm->victory_music);
    }
    pz_game_music_reset_state(gm);
}

void
pz_game_music_update(pz_game_music *gm, int enemies_alive,
    bool has_level3_enemy, bool level_complete, float dt)
{
    if (!gm || !gm->music || gm->state == MUSIC_STATE_STOPPED) {
        return;
    }

    double time_ms = pz_music_get_time_ms(gm->music);
    double beat_pos = fmod(time_ms, gm->beat_duration_ms);
    double prev_beat_pos = fmod(gm->current_time_ms, gm->beat_duration_ms);
    bool beat_crossed = beat_pos < prev_beat_pos;

    gm->current_time_ms = time_ms;

    if (gm->has_pending_changes && beat_crossed) {
        pz_game_music_apply_role(
            gm, PZ_MUSIC_ROLE_INTENSITY1, gm->pending_intensity1);
        pz_game_music_apply_role(
            gm, PZ_MUSIC_ROLE_INTENSITY2, gm->pending_intensity2);
        gm->current_intensity1 = gm->pending_intensity1;
        gm->current_intensity2 = gm->pending_intensity2;
        gm->has_pending_changes = false;
    }

    bool want_i1 = enemies_alive > 1;
    bool want_i2 = has_level3_enemy;
    if (want_i1 != gm->current_intensity1
        || want_i2 != gm->current_intensity2) {
        double time_to_beat = gm->beat_duration_ms - beat_pos;
        if (time_to_beat < gm->beat_threshold_ms) {
            pz_game_music_apply_role(gm, PZ_MUSIC_ROLE_INTENSITY1, want_i1);
            pz_game_music_apply_role(gm, PZ_MUSIC_ROLE_INTENSITY2, want_i2);
            gm->current_intensity1 = want_i1;
            gm->current_intensity2 = want_i2;
            gm->has_pending_changes = false;
        } else {
            gm->pending_intensity1 = want_i1;
            gm->pending_intensity2 = want_i2;
            gm->has_pending_changes = true;
        }
    }

    if (level_complete && gm->state == MUSIC_STATE_PLAYING) {
        gm->state = MUSIC_STATE_FADING_OUT;
        gm->fade_timer = gm->fade_duration;
    }

    if (gm->state == MUSIC_STATE_FADING_OUT) {
        gm->fade_timer -= dt;
        float fade = 0.0f;
        if (gm->fade_duration > 0.0f) {
            fade = gm->fade_timer / gm->fade_duration;
        }
        if (fade < 0.0f) {
            fade = 0.0f;
        }
        pz_music_set_volume(gm->music, gm->master_volume * fade);

        if (gm->fade_timer <= 0.0f) {
            pz_music_stop(gm->music);
            if (gm->victory_music) {
                pz_music_play(gm->victory_music);
                gm->state = MUSIC_STATE_VICTORY;
            } else {
                gm->state = MUSIC_STATE_STOPPED;
            }
        }
    }
}

void
pz_game_music_render(
    pz_game_music *gm, float *buffer, int num_frames, int num_channels)
{
    if (!buffer || num_frames <= 0 || num_channels <= 0) {
        return;
    }

    if (!gm) {
        memset(buffer, 0,
            sizeof(float) * (size_t)num_frames * (size_t)num_channels);
        return;
    }

    if (gm->state == MUSIC_STATE_VICTORY && gm->victory_music) {
        pz_music_render(gm->victory_music, buffer, num_frames, num_channels);
        return;
    }

    if (gm->music) {
        pz_music_render(gm->music, buffer, num_frames, num_channels);
        return;
    }

    memset(
        buffer, 0, sizeof(float) * (size_t)num_frames * (size_t)num_channels);
}

void
pz_game_music_set_volume(pz_game_music *gm, float volume)
{
    if (!gm) {
        return;
    }
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }
    gm->master_volume = volume;
    if (gm->music) {
        pz_music_set_volume(gm->music, volume);
    }
    if (gm->victory_music) {
        pz_music_set_volume(gm->victory_music, volume);
    }
}

float
pz_game_music_get_volume(const pz_game_music *gm)
{
    if (!gm) {
        return 0.0f;
    }
    return gm->master_volume;
}
