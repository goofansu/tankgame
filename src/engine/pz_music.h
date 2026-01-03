/*
 * Tank Game - MIDI Music System
 */

#ifndef PZ_MUSIC_H
#define PZ_MUSIC_H

#include <stdbool.h>

#define PZ_MUSIC_MAX_LAYERS 8

typedef struct pz_music pz_music;

typedef struct pz_music_layer_config {
    const char *midi_path;
    int midi_channel;
    float volume;
    bool enabled;
    bool loop;
} pz_music_layer_config;

typedef struct pz_music_config {
    const char *soundfont_path;
    pz_music_layer_config layers[PZ_MUSIC_MAX_LAYERS];
    int layer_count;
    float master_volume;
} pz_music_config;

pz_music *pz_music_create(const pz_music_config *config);
void pz_music_destroy(pz_music *music);

void pz_music_play(pz_music *music);
void pz_music_stop(pz_music *music);
void pz_music_pause(pz_music *music);
bool pz_music_is_playing(const pz_music *music);

void pz_music_set_layer_enabled(pz_music *music, int layer, bool enabled);
bool pz_music_get_layer_enabled(const pz_music *music, int layer);
void pz_music_set_layer_volume(pz_music *music, int layer, float volume);
float pz_music_get_layer_volume(const pz_music *music, int layer);

void pz_music_set_volume(pz_music *music, float volume);
float pz_music_get_volume(const pz_music *music);

double pz_music_get_time_ms(const pz_music *music);

void pz_music_render(
    pz_music *music, float *buffer, int num_frames, int num_channels);
void pz_music_update(pz_music *music, float dt);

#endif // PZ_MUSIC_H
