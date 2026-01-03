/*
 * Tank Game - Game-level music controller
 */

#ifndef PZ_GAME_MUSIC_H
#define PZ_GAME_MUSIC_H

#include <stdbool.h>

typedef struct pz_game_music pz_game_music;

pz_game_music *pz_game_music_create(const char *soundfont_path);
void pz_game_music_destroy(pz_game_music *gm);

bool pz_game_music_load(pz_game_music *gm, const char *musicset_name);
void pz_game_music_stop(pz_game_music *gm);

void pz_game_music_update(pz_game_music *gm, int enemies_alive,
    bool has_level3_enemy, bool level_complete, float dt);

void pz_game_music_render(
    pz_game_music *gm, float *buffer, int num_frames, int num_channels);

void pz_game_music_set_volume(pz_game_music *gm, float volume);
float pz_game_music_get_volume(const pz_game_music *gm);

// Debug info for overlay
typedef struct pz_game_music_debug_info {
    bool playing;
    bool is_victory;
    float bpm;
    double time_ms;
    double loop_length_ms;
    double beat_pos;
    float master_volume;
    bool intensity1_active;
    bool intensity2_active;
    bool intensity1_pending;
    bool intensity2_pending;
    int layer_count;
} pz_game_music_debug_info;

bool pz_game_music_get_debug_info(
    const pz_game_music *gm, pz_game_music_debug_info *info);

// Forward declaration for layer info
struct pz_music_layer_info;
bool pz_game_music_get_layer_info(
    const pz_game_music *gm, int layer, struct pz_music_layer_info *info);

#endif // PZ_GAME_MUSIC_H
