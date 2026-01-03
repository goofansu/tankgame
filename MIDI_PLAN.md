# MIDI Music System Implementation Plan

## Goal

Add a layered MIDI music system to the tank game, inspired by Wii Tanks. The music is a military march with 3 independently controllable layers that can be toggled on/off.

## Components Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     Audio Pipeline                              │
│                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌──────────────────┐   │
│  │ MIDI Files  │───▶│ TinyMidi    │───▶│ TinySoundFont    │   │
│  │ (.mid)      │    │ Loader      │    │ (SF2 Synth)      │   │
│  └─────────────┘    └─────────────┘    └────────┬─────────┘   │
│                                                  │              │
│  ┌─────────────┐                      ┌─────────▼──────────┐  │
│  │ SoundFont   │─────────────────────▶│ sokol_audio        │  │
│  │ (.sf2)      │                      │ (OS audio output)  │  │
│  └─────────────┘                      └────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Dependencies (All Vendored)

| Library | File | Size | License | Purpose |
|---------|------|------|---------|---------|
| TinySoundFont | `tsf.h` | 91KB | MIT | SoundFont synthesizer |
| TinyMidiLoader | `tml.h` | 20KB | zlib | MIDI file parser |
| sokol_audio | `sokol_audio.h` | (existing) | zlib | Audio output |

## Assets

| File | Size | Source | Purpose |
|------|------|--------|---------|
| `soundfont.sf2` | 32MB | Jnsgm2 GM | Instrument sounds |
| `march_drums.mid` | ~1KB | Create | Snare/bass drum pattern |
| `march_bass.mid` | ~1KB | Create | Tuba bass line |
| `march_melody.mid` | ~2KB | Create | Trumpet melody |

---

## Implementation Steps

### Step 1: Vendor Libraries

**Files to create:**
- `third_party/tsf.h` ✓ (already downloaded)
- `third_party/tml.h` ✓ (already downloaded)
- `third_party/tsf_impl.c` - Implementation file

**tsf_impl.c contents:**
```c
#define TSF_IMPLEMENTATION
#include "tsf.h"

#define TML_IMPLEMENTATION  
#include "tml.h"
```

**CMakeLists.txt changes:**
- Add `third_party/tsf_impl.c` to sources
- Link AudioToolbox framework on macOS
- Link asound on Linux

---

### Step 2: Create pz_audio Module

**Files:**
- `src/engine/pz_audio.h`
- `src/engine/pz_audio.c`

**Responsibilities:**
- Initialize sokol_audio with callback model
- Provide sample rate and channel count
- Route audio callback to music system
- Master volume control

**Key functions:**
```c
pz_audio* pz_audio_init(void);
void pz_audio_shutdown(pz_audio* audio);
void pz_audio_set_callback(pz_audio* audio, pz_audio_callback cb, void* userdata);
```

**sokol_audio setup:**
```c
saudio_setup(&(saudio_desc){
    .sample_rate = 44100,
    .num_channels = 2,
    .stream_userdata_cb = audio_callback,
    .user_data = audio,
    .logger.func = slog_func,
});
```

---

### Step 3: Create pz_music Module

**Files:**
- `src/engine/pz_music.h`
- `src/engine/pz_music.c`

**Data structures:**
```c
typedef struct pz_music_layer {
    tml_message* midi;           // Loaded MIDI data (head of linked list)
    tml_message* current;        // Current playback position
    double       time_ms;        // Playback time in milliseconds
    float        volume;         // Layer volume 0.0-1.0
    int          channel;        // MIDI channel for this layer
    bool         enabled;        // Is layer active?
    bool         loop;           // Loop when done?
} pz_music_layer;

typedef struct pz_music {
    tsf*             sf;         // SoundFont instance
    pz_music_layer   layers[8];  // Up to 8 layers
    int              layer_count;
    float            master_vol; // Master volume
    int              sample_rate;
    bool             playing;
} pz_music;
```

**Key functions:**
```c
// Lifecycle
pz_music* pz_music_create(const char* soundfont_path);
void pz_music_destroy(pz_music* music);

// Layer management
int  pz_music_add_layer(pz_music* music, const char* midi_path, int channel);
void pz_music_set_layer_enabled(pz_music* music, int layer, bool enabled);
void pz_music_set_layer_volume(pz_music* music, int layer, float vol);

// Playback
void pz_music_play(pz_music* music);
void pz_music_stop(pz_music* music);

// Audio rendering (called from audio thread)
void pz_music_render(pz_music* music, float* buffer, int frames, int channels);
```

---

### Step 4: MIDI Playback Logic

**Render loop pseudocode:**
```c
void pz_music_render(pz_music* m, float* buf, int frames, int channels) {
    if (!m->playing) {
        memset(buf, 0, frames * channels * sizeof(float));
        return;
    }
    
    double ms_per_sample = 1000.0 / m->sample_rate;
    
    for (int i = 0; i < frames; i++) {
        // Advance each layer and send MIDI events to synth
        for (int l = 0; l < m->layer_count; l++) {
            pz_music_layer* layer = &m->layers[l];
            if (!layer->enabled) continue;
            
            // Process all MIDI events up to current time
            while (layer->current && layer->current->time <= layer->time_ms) {
                tml_message* msg = layer->current;
                
                // Route to correct channel based on layer
                if (msg->type == TML_NOTE_ON) {
                    tsf_channel_note_on(m->sf, layer->channel, 
                                        msg->key, msg->velocity / 127.0f);
                } else if (msg->type == TML_NOTE_OFF) {
                    tsf_channel_note_off(m->sf, layer->channel, msg->key);
                }
                // ... handle other message types
                
                layer->current = layer->current->next;
            }
            
            layer->time_ms += ms_per_sample;
            
            // Loop handling
            if (!layer->current && layer->loop) {
                layer->current = layer->midi;
                layer->time_ms = 0;
            }
        }
        
        // Render one sample from synth
        tsf_render_float(m->sf, buf + i * channels, 1, 0);
    }
}
```

---

### Step 5: Create MIDI Files

**Approach:** Write a Python script (no deps) or C tool to generate MIDI files.

**March specification (from Wii Tanks screenshots):**
- Tempo: 120 BPM
- Time signature: 4/4
- Key: G major
- Length: 8 bars, looping

**Layer 0: Drums (Channel 9)**
```
Bar pattern (repeats):
Beat 1: Bass drum (note 36) + Snare (note 38)
Beat 2: Snare
Beat 3: Bass drum + Snare  
Beat 4: Snare
With eighth-note hi-hat throughout
```

**Layer 1: Bass/Tuba (Channel 0, Program 58)**
```
Notes (quarter notes):
| G2 | G2 | C3 | C3 | D3 | D3 | G2 | G2 |
(Follows I - IV - V - I progression)
```

**Layer 2: Melody/Trumpet (Channel 1, Program 56)**
Based on the screenshot, staccato eighth notes with dotted rhythms:
```
Approximate transcription:
| G4 B4 D5 G5 . | G5 F#5 E5 D5 | ... |
(Military fanfare style, repeating motif)
```

**MIDI file format notes:**
- Standard MIDI File format 0 (single track) or 1 (multi-track)
- 480 ticks per quarter note (standard)
- Include tempo meta event (500000 microseconds = 120 BPM)
- Include program change for instrument selection

---

### Step 6: Integration

**In main.c or pz_game.c:**
```c
// During init
g_audio = pz_audio_init();
g_music = pz_music_create("assets/sounds/soundfont.sf2");

pz_music_add_layer(g_music, "assets/music/march_drums.mid", 9);   // drums
pz_music_add_layer(g_music, "assets/music/march_bass.mid", 0);    // bass
pz_music_add_layer(g_music, "assets/music/march_melody.mid", 1);  // melody

pz_audio_set_callback(g_audio, music_audio_callback, g_music);
pz_music_play(g_music);

// Audio callback
void music_audio_callback(float* buf, int frames, int ch, void* ud) {
    pz_music_render((pz_music*)ud, buf, frames, ch);
}

// During shutdown
pz_music_destroy(g_music);
pz_audio_shutdown(g_audio);
```

---

## Build System Changes

**CMakeLists.txt additions:**
```cmake
# Audio sources
set(ENGINE_SOURCES
    ...
    src/engine/pz_audio.c
    src/engine/pz_music.c
    third_party/tsf_impl.c
)

# Platform-specific audio libraries
if(APPLE)
    target_link_libraries(tankgame "-framework AudioToolbox")
elseif(UNIX)
    target_link_libraries(tankgame asound)
endif()
```

---

## Testing Plan

1. **Audio init test**: Verify sokol_audio initializes without errors
2. **SoundFont load test**: Load SF2, check preset count
3. **Single note test**: Play middle C for 1 second
4. **Single MIDI test**: Play one MIDI file through
5. **Multi-layer test**: Play all 3 layers, verify mixing
6. **Toggle test**: Enable/disable layers with keyboard (1/2/3 keys)

---

## File Checklist

| File | Status | Description |
|------|--------|-------------|
| `third_party/tsf.h` | ✓ Done | TinySoundFont header |
| `third_party/tml.h` | ✓ Done | TinyMidiLoader header |
| `third_party/tsf_impl.c` | TODO | Implementation |
| `assets/sounds/soundfont.sf2` | ✓ Done | Jnsgm2 SoundFont |
| `assets/music/march_drums.mid` | TODO | Drum layer |
| `assets/music/march_bass.mid` | TODO | Bass layer |
| `assets/music/march_melody.mid` | TODO | Melody layer |
| `src/engine/pz_audio.h` | TODO | Audio system header |
| `src/engine/pz_audio.c` | TODO | Audio system impl |
| `src/engine/pz_music.h` | TODO | Music system header |
| `src/engine/pz_music.c` | TODO | Music system impl |
| `spec/06-music-system.md` | ✓ Done | Detailed spec |

---

## Future Enhancements (Out of Scope)

- Beat-synchronized layer transitions
- Volume fade envelopes
- Dynamic intensity based on gameplay
- Victory/defeat stingers
- Menu vs gameplay music crossfade

---

## Questions Resolved

1. **SoundFont**: Using Jnsgm2 (32MB GM SoundFont) - good enough for now
2. **MIDI creation**: Will write a script to generate the 3 MIDI files
3. **Layers**: 3 layers (drums, bass, melody)
4. **Initial integration**: Basic playback only, no dynamic control yet
