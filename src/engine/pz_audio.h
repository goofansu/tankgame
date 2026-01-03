/*
 * Tank Game - Audio System
 */

#ifndef PZ_AUDIO_H
#define PZ_AUDIO_H

#include <stdbool.h>

typedef struct pz_audio pz_audio;

typedef void (*pz_audio_callback)(
    float *buffer, int num_frames, int num_channels, void *userdata);

// Initialize audio system (44100 Hz, stereo)
pz_audio *pz_audio_init(void);

// Shutdown audio system
void pz_audio_shutdown(pz_audio *audio);

// Set the audio callback that fills the buffer
void pz_audio_set_callback(
    pz_audio *audio, pz_audio_callback callback, void *userdata);

// Master volume control (0.0 - 1.0)
void pz_audio_set_volume(pz_audio *audio, float volume);
float pz_audio_get_volume(const pz_audio *audio);

// Query audio output format
int pz_audio_get_sample_rate(const pz_audio *audio);
int pz_audio_get_channels(const pz_audio *audio);

#endif // PZ_AUDIO_H
