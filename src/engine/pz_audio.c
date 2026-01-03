/*
 * Tank Game - Audio System
 */

#include "engine/pz_audio.h"

#include <string.h>

#define SOKOL_AUDIO_IMPL
#include "third_party/sokol/sokol_audio.h"
#include "third_party/sokol/sokol_log.h"

#include "core/pz_log.h"
#include "core/pz_mem.h"

struct pz_audio {
    float volume;
    int sample_rate;
    int channels;
    pz_audio_callback callback;
    void *userdata;
};

static void
pz_audio_stream_cb(
    float *buffer, int num_frames, int num_channels, void *user_data)
{
    pz_audio *audio = (pz_audio *)user_data;
    if (!buffer || num_frames <= 0 || num_channels <= 0) {
        return;
    }

    if (!audio || !audio->callback) {
        memset(buffer, 0,
            sizeof(float) * (size_t)num_frames * (size_t)num_channels);
        return;
    }

    audio->callback(buffer, num_frames, num_channels, audio->userdata);

    float volume = audio->volume;
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }

    if (volume != 1.0f) {
        int total_samples = num_frames * num_channels;
        for (int i = 0; i < total_samples; i++) {
            buffer[i] *= volume;
        }
    }
}

pz_audio *
pz_audio_init(void)
{
    pz_audio *audio
        = (pz_audio *)pz_calloc_tagged(1, sizeof(pz_audio), PZ_MEM_AUDIO);
    if (!audio) {
        return NULL;
    }

    audio->volume = 1.0f;

    saudio_setup(&(saudio_desc) {
        .sample_rate = 44100,
        .num_channels = 2,
        .stream_userdata_cb = pz_audio_stream_cb,
        .user_data = audio,
        .logger.func = slog_func,
    });

    if (!saudio_isvalid()) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_AUDIO, "Audio initialization failed");
        saudio_shutdown();
        pz_free(audio);
        return NULL;
    }

    saudio_desc desc = saudio_query_desc();
    audio->sample_rate = desc.sample_rate;
    audio->channels = desc.num_channels;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_AUDIO, "Audio ready: %d Hz, %d channels",
        audio->sample_rate, audio->channels);

    return audio;
}

void
pz_audio_shutdown(pz_audio *audio)
{
    if (!audio) {
        return;
    }

    saudio_shutdown();
    pz_free(audio);
}

void
pz_audio_set_callback(
    pz_audio *audio, pz_audio_callback callback, void *userdata)
{
    if (!audio) {
        return;
    }
    audio->callback = callback;
    audio->userdata = userdata;
}

void
pz_audio_set_volume(pz_audio *audio, float volume)
{
    if (!audio) {
        return;
    }
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }
    audio->volume = volume;
}

float
pz_audio_get_volume(const pz_audio *audio)
{
    return audio ? audio->volume : 0.0f;
}

int
pz_audio_get_sample_rate(const pz_audio *audio)
{
    return audio ? audio->sample_rate : 0;
}

int
pz_audio_get_channels(const pz_audio *audio)
{
    return audio ? audio->channels : 0;
}
