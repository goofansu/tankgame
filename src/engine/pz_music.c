/*
 * Tank Game - MIDI Music System
 */

#include "engine/pz_music.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>

#include "third_party/tml.h"
#include "third_party/tsf.h"

#include "core/pz_log.h"
#include "core/pz_mem.h"

typedef struct pz_music_layer {
    tml_message *midi;
    tml_message *current;
    double time_ms;
    double length_ms;
    _Atomic float volume;
    _Atomic bool enabled;
    bool active;
    bool loop;
    int midi_channel;
} pz_music_layer;

struct pz_music {
    tsf *soundfont;
    pz_music_layer layers[PZ_MUSIC_MAX_LAYERS];
    int layer_count;
    _Atomic float master_volume;
    _Atomic bool playing;
    _Atomic double playback_time_ms;
    int sample_rate;
};

static double
pz_music_find_length_ms(tml_message *midi)
{
    double last_time = 0.0;
    for (tml_message *msg = midi; msg; msg = msg->next) {
        if (msg->time > last_time) {
            last_time = (double)msg->time;
        }
    }
    return last_time;
}

static void
pz_music_dispatch_message(
    pz_music *music, pz_music_layer *layer, tml_message *msg, bool enabled)
{
    int channel = layer->midi_channel;
    switch (msg->type) {
    case TML_NOTE_ON:
        if (enabled) {
            if (msg->velocity > 0) {
                tsf_channel_note_on(music->soundfont, channel, msg->key,
                    msg->velocity / 127.0f);
            } else {
                tsf_channel_note_off(music->soundfont, channel, msg->key);
            }
        }
        break;
    case TML_NOTE_OFF:
        if (enabled) {
            tsf_channel_note_off(music->soundfont, channel, msg->key);
        }
        break;
    case TML_PROGRAM_CHANGE:
        tsf_channel_set_presetnumber(
            music->soundfont, channel, msg->program, channel == 9);
        break;
    case TML_CONTROL_CHANGE:
        tsf_channel_midi_control(
            music->soundfont, channel, msg->control, msg->control_value);
        break;
    case TML_PITCH_BEND:
        tsf_channel_set_pitchwheel(music->soundfont, channel, msg->pitch_bend);
        break;
    case TML_CHANNEL_PRESSURE:
        break;
    case TML_KEY_PRESSURE:
        break;
    case TML_SET_TEMPO:
        break;
    default:
        break;
    }
}

static void
pz_music_sync_layer(pz_music *music, pz_music_layer *layer, bool enabled)
{
    if (!layer->midi || !layer->current) {
        if (layer->loop && layer->midi && layer->length_ms > 0.0) {
            while (layer->time_ms >= layer->length_ms) {
                layer->time_ms -= layer->length_ms;
            }
            layer->current = layer->midi;
            tsf_channel_note_off_all(music->soundfont, layer->midi_channel);
        } else {
            return;
        }
    }

    while (true) {
        while (layer->current && layer->current->time <= layer->time_ms) {
            pz_music_dispatch_message(music, layer, layer->current, enabled);
            layer->current = layer->current->next;
        }

        if (!layer->current && layer->loop && layer->midi
            && layer->length_ms > 0.0) {
            while (layer->time_ms >= layer->length_ms) {
                layer->time_ms -= layer->length_ms;
            }
            layer->current = layer->midi;
            tsf_channel_note_off_all(music->soundfont, layer->midi_channel);
            continue;
        }
        break;
    }
}

static void
pz_music_reset_layer(pz_music *music, pz_music_layer *layer)
{
    if (!layer->midi) {
        return;
    }
    layer->time_ms = 0.0;
    layer->current = layer->midi;
    layer->active = atomic_load_explicit(&layer->enabled, memory_order_relaxed);
    tsf_channel_note_off_all(music->soundfont, layer->midi_channel);
}

pz_music *
pz_music_create(const pz_music_config *config)
{
    if (!config || !config->soundfont_path || config->layer_count <= 0) {
        return NULL;
    }

    pz_music *music
        = (pz_music *)pz_calloc_tagged(1, sizeof(pz_music), PZ_MEM_AUDIO);
    if (!music) {
        return NULL;
    }

    music->soundfont = tsf_load_filename(config->soundfont_path);
    if (!music->soundfont) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_AUDIO, "Failed to load SoundFont: %s",
            config->soundfont_path);
        pz_free(music);
        return NULL;
    }

    music->sample_rate = 44100;
    tsf_set_output(
        music->soundfont, TSF_STEREO_INTERLEAVED, music->sample_rate, 0.0f);
    tsf_set_max_voices(music->soundfont, 128);

    atomic_store_explicit(
        &music->master_volume, config->master_volume, memory_order_relaxed);
    tsf_set_volume(music->soundfont,
        atomic_load_explicit(&music->master_volume, memory_order_relaxed));

    music->layer_count = config->layer_count;
    if (music->layer_count > PZ_MUSIC_MAX_LAYERS) {
        music->layer_count = PZ_MUSIC_MAX_LAYERS;
    }

    for (int i = 0; i < music->layer_count; i++) {
        pz_music_layer *layer = &music->layers[i];
        const pz_music_layer_config *layer_config = &config->layers[i];

        layer->midi = tml_load_filename(layer_config->midi_path);
        if (!layer->midi) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_AUDIO, "Failed to load MIDI: %s",
                layer_config->midi_path);
            continue;
        }

        layer->current = layer->midi;
        layer->time_ms = 0.0;
        layer->length_ms = pz_music_find_length_ms(layer->midi);
        layer->midi_channel = layer_config->midi_channel;
        layer->loop = layer_config->loop;
        layer->active = layer_config->enabled;

        atomic_store_explicit(
            &layer->volume, layer_config->volume, memory_order_relaxed);
        atomic_store_explicit(
            &layer->enabled, layer_config->enabled, memory_order_relaxed);

        tsf_channel_set_presetnumber(
            music->soundfont, layer->midi_channel, 0, layer->midi_channel == 9);
        tsf_channel_set_volume(music->soundfont, layer->midi_channel,
            atomic_load_explicit(&layer->volume, memory_order_relaxed));
        tsf_channel_set_pan(music->soundfont, layer->midi_channel, 0.5f);
    }

    atomic_store_explicit(&music->playing, false, memory_order_relaxed);
    atomic_store_explicit(&music->playback_time_ms, 0.0, memory_order_relaxed);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_AUDIO, "Music system ready (%d layers)",
        music->layer_count);

    return music;
}

void
pz_music_destroy(pz_music *music)
{
    if (!music) {
        return;
    }

    pz_music_stop(music);

    for (int i = 0; i < music->layer_count; i++) {
        if (music->layers[i].midi) {
            tml_free(music->layers[i].midi);
            music->layers[i].midi = NULL;
        }
    }

    if (music->soundfont) {
        tsf_close(music->soundfont);
        music->soundfont = NULL;
    }

    pz_free(music);
}

void
pz_music_play(pz_music *music)
{
    if (!music) {
        return;
    }
    atomic_store_explicit(&music->playing, true, memory_order_relaxed);
}

void
pz_music_stop(pz_music *music)
{
    if (!music) {
        return;
    }
    atomic_store_explicit(&music->playing, false, memory_order_relaxed);
    atomic_store_explicit(&music->playback_time_ms, 0.0, memory_order_relaxed);
    for (int i = 0; i < music->layer_count; i++) {
        pz_music_reset_layer(music, &music->layers[i]);
    }
}

void
pz_music_pause(pz_music *music)
{
    if (!music) {
        return;
    }
    atomic_store_explicit(&music->playing, false, memory_order_relaxed);
}

bool
pz_music_is_playing(const pz_music *music)
{
    if (!music) {
        return false;
    }
    return atomic_load_explicit(&music->playing, memory_order_relaxed);
}

void
pz_music_set_layer_enabled(pz_music *music, int layer, bool enabled)
{
    if (!music || layer < 0 || layer >= music->layer_count) {
        return;
    }
    atomic_store_explicit(
        &music->layers[layer].enabled, enabled, memory_order_relaxed);
}

bool
pz_music_get_layer_enabled(const pz_music *music, int layer)
{
    if (!music || layer < 0 || layer >= music->layer_count) {
        return false;
    }
    return atomic_load_explicit(
        &music->layers[layer].enabled, memory_order_relaxed);
}

void
pz_music_set_layer_volume(pz_music *music, int layer, float volume)
{
    if (!music || layer < 0 || layer >= music->layer_count) {
        return;
    }
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }
    atomic_store_explicit(
        &music->layers[layer].volume, volume, memory_order_relaxed);
}

float
pz_music_get_layer_volume(const pz_music *music, int layer)
{
    if (!music || layer < 0 || layer >= music->layer_count) {
        return 0.0f;
    }
    return atomic_load_explicit(
        &music->layers[layer].volume, memory_order_relaxed);
}

void
pz_music_set_volume(pz_music *music, float volume)
{
    if (!music) {
        return;
    }
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }
    atomic_store_explicit(&music->master_volume, volume, memory_order_relaxed);
}

float
pz_music_get_volume(const pz_music *music)
{
    if (!music) {
        return 0.0f;
    }
    return atomic_load_explicit(&music->master_volume, memory_order_relaxed);
}

double
pz_music_get_time_ms(const pz_music *music)
{
    if (!music) {
        return 0.0;
    }
    return atomic_load_explicit(&music->playback_time_ms, memory_order_relaxed);
}

void
pz_music_render(
    pz_music *music, float *buffer, int num_frames, int num_channels)
{
    if (!buffer || num_frames <= 0 || num_channels <= 0) {
        return;
    }

    if (!music || !music->soundfont
        || !atomic_load_explicit(&music->playing, memory_order_relaxed)) {
        memset(buffer, 0,
            sizeof(float) * (size_t)num_frames * (size_t)num_channels);
        return;
    }

    float master_volume
        = atomic_load_explicit(&music->master_volume, memory_order_relaxed);
    if (master_volume < 0.0f) {
        master_volume = 0.0f;
    } else if (master_volume > 1.0f) {
        master_volume = 1.0f;
    }
    tsf_set_volume(music->soundfont, master_volume);

    double ms_per_sample = 1000.0 / (double)music->sample_rate;
    int remaining = num_frames;
    int offset = 0;

    for (int i = 0; i < music->layer_count; i++) {
        pz_music_layer *layer = &music->layers[i];
        if (!layer->midi) {
            continue;
        }
        float volume
            = atomic_load_explicit(&layer->volume, memory_order_relaxed);
        if (volume < 0.0f) {
            volume = 0.0f;
        } else if (volume > 1.0f) {
            volume = 1.0f;
        }
        tsf_channel_set_volume(music->soundfont, layer->midi_channel, volume);
    }

    for (int i = 0; i < music->layer_count; i++) {
        pz_music_layer *layer = &music->layers[i];
        if (!layer->midi) {
            continue;
        }
        bool enabled
            = atomic_load_explicit(&layer->enabled, memory_order_relaxed);
        if (!enabled && layer->active) {
            tsf_channel_note_off_all(music->soundfont, layer->midi_channel);
            layer->active = false;
        } else if (enabled && !layer->active) {
            layer->active = true;
        }
        pz_music_sync_layer(music, layer, enabled);
    }

    while (remaining > 0) {
        double next_delta_ms = (double)remaining * ms_per_sample;

        for (int i = 0; i < music->layer_count; i++) {
            pz_music_layer *layer = &music->layers[i];
            if (!layer->midi || !layer->current) {
                continue;
            }
            double delta = (double)layer->current->time - layer->time_ms;
            if (delta > 0.0 && delta < next_delta_ms) {
                next_delta_ms = delta;
            }
        }

        int frames_to_render = (int)floor(next_delta_ms / ms_per_sample);
        if (frames_to_render <= 0) {
            frames_to_render = 1;
        }
        if (frames_to_render > remaining) {
            frames_to_render = remaining;
        }

        tsf_render_float(music->soundfont,
            buffer + (size_t)offset * (size_t)num_channels, frames_to_render,
            0);

        double advance_ms = (double)frames_to_render * ms_per_sample;
        for (int i = 0; i < music->layer_count; i++) {
            pz_music_layer *layer = &music->layers[i];
            if (!layer->midi) {
                continue;
            }
            layer->time_ms += advance_ms;
        }

        offset += frames_to_render;
        remaining -= frames_to_render;

        for (int i = 0; i < music->layer_count; i++) {
            pz_music_layer *layer = &music->layers[i];
            if (!layer->midi) {
                continue;
            }
            bool enabled
                = atomic_load_explicit(&layer->enabled, memory_order_relaxed);
            pz_music_sync_layer(music, layer, enabled);
        }
    }

    if (music->layer_count > 0) {
        atomic_store_explicit(&music->playback_time_ms,
            music->layers[0].time_ms, memory_order_relaxed);
    }
}

void
pz_music_update(pz_music *music, float dt)
{
    (void)music;
    (void)dt;
}
