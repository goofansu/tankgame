/*
 * Tank Game - Musicset file parsing
 */

#ifndef PZ_MUSICSET_H
#define PZ_MUSICSET_H

#include <stdbool.h>

#define PZ_MUSICSET_MAX_LAYERS 6
#define PZ_MUSICSET_NAME_LEN 64
#define PZ_MUSICSET_PATH_LEN 128

typedef enum {
    PZ_MUSIC_ROLE_BASE,
    PZ_MUSIC_ROLE_INTENSITY1,
    PZ_MUSIC_ROLE_INTENSITY2,
} pz_music_role;

typedef struct pz_musicset_layer {
    pz_music_role role;
    char midi_path[PZ_MUSICSET_PATH_LEN];
    int channel;
    float volume;
} pz_musicset_layer;

typedef struct pz_musicset {
    char name[PZ_MUSICSET_NAME_LEN];
    float bpm;
    pz_musicset_layer layers[PZ_MUSICSET_MAX_LAYERS];
    int layer_count;
    char victory_path[PZ_MUSICSET_PATH_LEN];
    int victory_channel;
    bool has_victory;
} pz_musicset;

pz_musicset *pz_musicset_load(const char *path);
void pz_musicset_destroy(pz_musicset *set);

#endif // PZ_MUSICSET_H
