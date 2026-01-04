/*
 * Tank Game - Sound Effects System
 */

#include "engine/pz_sfx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/pz_log.h"
#include "core/pz_mem.h"
#include "core/pz_platform.h"

// Maximum simultaneous sound instances
#define PZ_SFX_MAX_VOICES 32

// Sound data loaded from WAV file
typedef struct {
    float *samples; // Interleaved stereo samples (resampled to output rate)
    int sample_count; // Number of sample frames
    int channels; // 1 or 2
    bool loaded;
} pz_sfx_sound;

// A playing sound instance
typedef struct {
    pz_sfx_id sound_id;
    uint32_t handle;
    int position; // Current sample position
    float volume;
    bool looping;
    bool active;
} pz_sfx_voice;

struct pz_sfx_manager {
    pz_sfx_sound sounds[PZ_SFX_COUNT];
    pz_sfx_voice voices[PZ_SFX_MAX_VOICES];
    uint32_t next_handle;
    int output_sample_rate;
    float master_volume;
};

// WAV file parsing
typedef struct {
    char riff[4];
    uint32_t file_size;
    char wave[4];
} wav_header;

typedef struct {
    char id[4];
    uint32_t size;
} wav_chunk;

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_fmt;

// Simple linear interpolation resampler
static float *
resample_audio(const float *input, int input_frames, int input_rate,
    int output_rate, int channels, int *output_frames)
{
    if (input_rate == output_rate) {
        // No resampling needed, just copy
        *output_frames = input_frames;
        size_t size = sizeof(float) * (size_t)input_frames * (size_t)channels;
        float *output = (float *)pz_alloc_tagged(size, PZ_MEM_AUDIO);
        if (output) {
            memcpy(output, input, size);
        }
        return output;
    }

    double ratio = (double)output_rate / (double)input_rate;
    *output_frames = (int)((double)input_frames * ratio) + 1;

    size_t size = sizeof(float) * (size_t)(*output_frames) * (size_t)channels;
    float *output = (float *)pz_alloc_tagged(size, PZ_MEM_AUDIO);
    if (!output) {
        return NULL;
    }

    for (int i = 0; i < *output_frames; i++) {
        double src_pos = (double)i / ratio;
        int src_idx = (int)src_pos;
        double frac = src_pos - (double)src_idx;

        if (src_idx >= input_frames - 1) {
            src_idx = input_frames - 1;
            frac = 0.0;
        }

        for (int c = 0; c < channels; c++) {
            float s0 = input[src_idx * channels + c];
            float s1 = (src_idx + 1 < input_frames)
                ? input[(src_idx + 1) * channels + c]
                : s0;
            output[i * channels + c] = (float)(s0 + (s1 - s0) * frac);
        }
    }

    return output;
}

static bool
load_wav_file(const char *path, int output_sample_rate, pz_sfx_sound *sound)
{
    size_t file_size = 0;
    uint8_t *file_data = (uint8_t *)pz_file_read(path, &file_size);
    if (!file_data) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "Failed to read WAV: %s", path);
        return false;
    }

    if (file_size < sizeof(wav_header)) {
        pz_free(file_data);
        return false;
    }

    wav_header *header = (wav_header *)file_data;
    if (memcmp(header->riff, "RIFF", 4) != 0
        || memcmp(header->wave, "WAVE", 4) != 0) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "Invalid WAV header: %s", path);
        pz_free(file_data);
        return false;
    }

    // Find fmt and data chunks
    wav_fmt fmt = { 0 };
    uint8_t *data_ptr = NULL;
    uint32_t data_size = 0;

    uint8_t *ptr = file_data + sizeof(wav_header);
    uint8_t *end = file_data + file_size;

    while (ptr + sizeof(wav_chunk) <= end) {
        wav_chunk *chunk = (wav_chunk *)ptr;
        uint8_t *chunk_data = ptr + sizeof(wav_chunk);

        if (memcmp(chunk->id, "fmt ", 4) == 0) {
            if (chunk->size >= sizeof(wav_fmt)) {
                memcpy(&fmt, chunk_data, sizeof(wav_fmt));
            }
        } else if (memcmp(chunk->id, "data", 4) == 0) {
            data_ptr = chunk_data;
            data_size = chunk->size;
        }

        // Move to next chunk (chunks are word-aligned)
        uint32_t chunk_size = chunk->size;
        if (chunk_size & 1)
            chunk_size++;
        ptr = chunk_data + chunk_size;
    }

    if (!data_ptr || data_size == 0) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "No data chunk in WAV: %s", path);
        pz_free(file_data);
        return false;
    }

    if (fmt.audio_format != 1) { // PCM
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "WAV not PCM format: %s", path);
        pz_free(file_data);
        return false;
    }

    if (fmt.bits_per_sample != 16) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "WAV not 16-bit: %s", path);
        pz_free(file_data);
        return false;
    }

    int channels = fmt.num_channels;
    if (channels < 1 || channels > 2) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "WAV invalid channels: %s", path);
        pz_free(file_data);
        return false;
    }

    int sample_frames = (int)(data_size / (size_t)(channels * 2));

    // Convert 16-bit PCM to float
    float *float_samples = (float *)pz_alloc_tagged(
        sizeof(float) * (size_t)sample_frames * (size_t)channels, PZ_MEM_AUDIO);
    if (!float_samples) {
        pz_free(file_data);
        return false;
    }

    int16_t *pcm = (int16_t *)data_ptr;
    for (int i = 0; i < sample_frames * channels; i++) {
        float_samples[i] = (float)pcm[i] / 32768.0f;
    }

    pz_free(file_data);

    // Resample to output rate
    int output_frames = 0;
    float *resampled = resample_audio(float_samples, sample_frames,
        (int)fmt.sample_rate, output_sample_rate, channels, &output_frames);
    pz_free(float_samples);

    if (!resampled) {
        return false;
    }

    // If mono, convert to stereo
    if (channels == 1) {
        float *stereo = (float *)pz_alloc_tagged(
            sizeof(float) * (size_t)output_frames * 2, PZ_MEM_AUDIO);
        if (!stereo) {
            pz_free(resampled);
            return false;
        }
        for (int i = 0; i < output_frames; i++) {
            stereo[i * 2] = resampled[i];
            stereo[i * 2 + 1] = resampled[i];
        }
        pz_free(resampled);
        resampled = stereo;
        channels = 2;
    }

    sound->samples = resampled;
    sound->sample_count = output_frames;
    sound->channels = channels;
    sound->loaded = true;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_AUDIO, "Loaded WAV: %s (%d samples)", path,
        output_frames);

    return true;
}

// Sound file paths for each ID
static const char *sfx_paths[PZ_SFX_COUNT] = {
    [PZ_SFX_NONE] = NULL,
    [PZ_SFX_ENGINE_IDLE] = "assets/sounds/engine2.wav",
    [PZ_SFX_ENGINE_MOVING] = "assets/sounds/engine1.wav",
    [PZ_SFX_GUN_FIRE] = "assets/sounds/gun1.wav",
    [PZ_SFX_BULLET_HIT] = "assets/sounds/gun3.wav",
    [PZ_SFX_EXPLOSION_TANK] = "assets/sounds/explosion2.wav",
    [PZ_SFX_EXPLOSION_FINAL] = "assets/sounds/explosion1.wav",
    [PZ_SFX_TANK_HIT] = "assets/sounds/hit1.wav",
};

pz_sfx_manager *
pz_sfx_create(int sample_rate)
{
    pz_sfx_manager *sfx = (pz_sfx_manager *)pz_calloc_tagged(
        1, sizeof(pz_sfx_manager), PZ_MEM_AUDIO);
    if (!sfx) {
        return NULL;
    }

    sfx->output_sample_rate = sample_rate;
    sfx->next_handle = 1;
    sfx->master_volume = 1.0f;

    // Load all sound files
    for (int i = 1; i < PZ_SFX_COUNT; i++) {
        if (sfx_paths[i]) {
            load_wav_file(sfx_paths[i], sample_rate, &sfx->sounds[i]);
        }
    }

    return sfx;
}

void
pz_sfx_destroy(pz_sfx_manager *sfx)
{
    if (!sfx) {
        return;
    }

    for (int i = 0; i < PZ_SFX_COUNT; i++) {
        if (sfx->sounds[i].samples) {
            pz_free(sfx->sounds[i].samples);
        }
    }

    pz_free(sfx);
}

static pz_sfx_voice *
find_free_voice(pz_sfx_manager *sfx)
{
    for (int i = 0; i < PZ_SFX_MAX_VOICES; i++) {
        if (!sfx->voices[i].active) {
            return &sfx->voices[i];
        }
    }

    // All voices in use - steal oldest non-looping voice
    for (int i = 0; i < PZ_SFX_MAX_VOICES; i++) {
        if (!sfx->voices[i].looping) {
            sfx->voices[i].active = false;
            return &sfx->voices[i];
        }
    }

    return NULL;
}

pz_sfx_handle
pz_sfx_play(pz_sfx_manager *sfx, pz_sfx_id id, float volume)
{
    if (!sfx || id <= PZ_SFX_NONE || id >= PZ_SFX_COUNT) {
        return PZ_SFX_INVALID_HANDLE;
    }

    if (!sfx->sounds[id].loaded) {
        return PZ_SFX_INVALID_HANDLE;
    }

    pz_sfx_voice *voice = find_free_voice(sfx);
    if (!voice) {
        return PZ_SFX_INVALID_HANDLE;
    }

    voice->sound_id = id;
    voice->handle = sfx->next_handle++;
    voice->position = 0;
    voice->volume = volume;
    voice->looping = false;
    voice->active = true;

    if (sfx->next_handle == PZ_SFX_INVALID_HANDLE) {
        sfx->next_handle = 1;
    }

    return voice->handle;
}

pz_sfx_handle
pz_sfx_play_loop(pz_sfx_manager *sfx, pz_sfx_id id, float volume)
{
    if (!sfx || id <= PZ_SFX_NONE || id >= PZ_SFX_COUNT) {
        return PZ_SFX_INVALID_HANDLE;
    }

    if (!sfx->sounds[id].loaded) {
        return PZ_SFX_INVALID_HANDLE;
    }

    pz_sfx_voice *voice = find_free_voice(sfx);
    if (!voice) {
        return PZ_SFX_INVALID_HANDLE;
    }

    voice->sound_id = id;
    voice->handle = sfx->next_handle++;
    voice->position = 0;
    voice->volume = volume;
    voice->looping = true;
    voice->active = true;

    if (sfx->next_handle == PZ_SFX_INVALID_HANDLE) {
        sfx->next_handle = 1;
    }

    return voice->handle;
}

void
pz_sfx_stop(pz_sfx_manager *sfx, pz_sfx_handle handle)
{
    if (!sfx || handle == PZ_SFX_INVALID_HANDLE) {
        return;
    }

    for (int i = 0; i < PZ_SFX_MAX_VOICES; i++) {
        if (sfx->voices[i].active && sfx->voices[i].handle == handle) {
            sfx->voices[i].active = false;
            return;
        }
    }
}

void
pz_sfx_stop_all(pz_sfx_manager *sfx, pz_sfx_id id)
{
    if (!sfx) {
        return;
    }

    for (int i = 0; i < PZ_SFX_MAX_VOICES; i++) {
        if (sfx->voices[i].active && sfx->voices[i].sound_id == id) {
            sfx->voices[i].active = false;
        }
    }
}

bool
pz_sfx_is_playing(pz_sfx_manager *sfx, pz_sfx_handle handle)
{
    if (!sfx || handle == PZ_SFX_INVALID_HANDLE) {
        return false;
    }

    for (int i = 0; i < PZ_SFX_MAX_VOICES; i++) {
        if (sfx->voices[i].active && sfx->voices[i].handle == handle) {
            return true;
        }
    }
    return false;
}

void
pz_sfx_set_volume(pz_sfx_manager *sfx, pz_sfx_handle handle, float volume)
{
    if (!sfx || handle == PZ_SFX_INVALID_HANDLE) {
        return;
    }

    for (int i = 0; i < PZ_SFX_MAX_VOICES; i++) {
        if (sfx->voices[i].active && sfx->voices[i].handle == handle) {
            sfx->voices[i].volume = volume;
            return;
        }
    }
}

void
pz_sfx_set_master_volume(pz_sfx_manager *sfx, float volume)
{
    if (!sfx) {
        return;
    }
    if (volume < 0.0f)
        volume = 0.0f;
    if (volume > 1.0f)
        volume = 1.0f;
    sfx->master_volume = volume;
}

void
pz_sfx_render(
    pz_sfx_manager *sfx, float *buffer, int num_frames, int num_channels)
{
    if (!buffer || num_frames <= 0 || num_channels <= 0) {
        return;
    }

    // Clear buffer first (SFX render adds to existing content)
    // Note: Caller should already have music in buffer, we add to it

    if (!sfx) {
        return;
    }

    for (int v = 0; v < PZ_SFX_MAX_VOICES; v++) {
        pz_sfx_voice *voice = &sfx->voices[v];
        if (!voice->active) {
            continue;
        }

        pz_sfx_sound *sound = &sfx->sounds[voice->sound_id];
        if (!sound->loaded) {
            voice->active = false;
            continue;
        }

        float vol = voice->volume * sfx->master_volume;

        for (int i = 0; i < num_frames; i++) {
            if (voice->position >= sound->sample_count) {
                if (voice->looping) {
                    voice->position = 0;
                } else {
                    voice->active = false;
                    break;
                }
            }

            // Get stereo sample from sound
            float left = sound->samples[voice->position * 2];
            float right = sound->samples[voice->position * 2 + 1];

            // Add to output buffer
            if (num_channels == 2) {
                buffer[i * 2] += left * vol;
                buffer[i * 2 + 1] += right * vol;
            } else if (num_channels == 1) {
                buffer[i] += (left + right) * 0.5f * vol;
            }

            voice->position++;
        }
    }
}
