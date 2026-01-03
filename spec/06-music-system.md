# Music System Specification

## Overview

A layered MIDI music system inspired by Wii Tanks, where multiple musical layers can be independently controlled for dynamic, beat-synchronized music that responds to gameplay.

## Dependencies

### Third-party Libraries (vendored in `third_party/`)
- **TinySoundFont** (`tsf.h`) - SoundFont2 synthesizer, MIT license
- **TinyMidiLoader** (`tml.h`) - MIDI file parser, zlib license
- **sokol_audio** (already in project) - Cross-platform audio output

### Assets
- `assets/sounds/soundfont.sf2` - Jnsgm2 General MIDI SoundFont (32MB)
- `assets/music/*.mid` - MIDI files for each layer

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         pz_music                                │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    Music Layers                          │  │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐                  │  │
│  │  │ Layer 0 │  │ Layer 1 │  │ Layer 2 │                  │  │
│  │  │ drums   │  │ bass    │  │ melody  │                  │  │
│  │  │ vol:1.0 │  │ vol:0.8 │  │ vol:1.0 │                  │  │
│  │  │ enabled │  │ enabled │  │ enabled │                  │  │
│  │  └────┬────┘  └────┬────┘  └────┬────┘                  │  │
│  └───────┼────────────┼────────────┼───────────────────────┘  │
│          │            │            │                           │
│          └────────────┼────────────┘                           │
│                       ▼                                        │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              TinySoundFont (tsf)                         │  │
│  │  - Loads SoundFont once                                  │  │
│  │  - Each layer uses separate MIDI channel range           │  │
│  │  - Renders all active notes to float buffer              │  │
│  └──────────────────────────────────────┬───────────────────┘  │
│                                         │                      │
│  ┌──────────────────────────────────────▼───────────────────┐  │
│  │              sokol_audio callback                        │  │
│  │  - Called from audio thread                              │  │
│  │  - Requests N samples at 44100 Hz stereo                 │  │
│  │  - pz_music fills buffer with mixed output               │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## File Structure

```
src/engine/
  pz_audio.c/h        # sokol_audio wrapper, audio system init
  pz_music.c/h        # Multi-layer MIDI music system

third_party/
  tsf.h               # TinySoundFont header
  tml.h               # TinyMidiLoader header  
  tsf_impl.c          # TSF implementation file

assets/
  sounds/
    soundfont.sf2     # General MIDI SoundFont
  music/
    march_drums.mid   # Layer 0: Snare drum pattern
    march_bass.mid    # Layer 1: Bass/tuba line
    march_melody.mid  # Layer 2: Brass melody
```

## API Design

### pz_audio.h - Low-level audio system

```c
// Audio system (wraps sokol_audio)
typedef struct pz_audio pz_audio;

// Initialize audio system (44100 Hz, stereo)
pz_audio* pz_audio_init(void);

// Shutdown audio system
void pz_audio_shutdown(pz_audio* audio);

// Set the audio callback that fills the buffer
// callback: void (*)(float* buffer, int num_frames, int num_channels, void* userdata)
void pz_audio_set_callback(pz_audio* audio, 
                           void (*callback)(float*, int, int, void*),
                           void* userdata);

// Master volume control (0.0 - 1.0)
void pz_audio_set_volume(pz_audio* audio, float volume);
float pz_audio_get_volume(pz_audio* audio);
```

### pz_music.h - Music layer system

```c
#define PZ_MUSIC_MAX_LAYERS 8

typedef struct pz_music pz_music;

// Layer configuration
typedef struct pz_music_layer_config {
    const char* midi_path;      // Path to .mid file
    int         midi_channel;   // MIDI channel to use (0-15)
    float       volume;         // Initial volume (0.0 - 1.0)
    bool        enabled;        // Start enabled?
    bool        loop;           // Loop the MIDI?
} pz_music_layer_config;

// Music configuration  
typedef struct pz_music_config {
    const char*             soundfont_path;  // Path to .sf2 file
    pz_music_layer_config   layers[PZ_MUSIC_MAX_LAYERS];
    int                     layer_count;
    float                   master_volume;   // Overall volume
} pz_music_config;

// Create music system with config
pz_music* pz_music_create(const pz_music_config* config);

// Destroy music system
void pz_music_destroy(pz_music* music);

// Start/stop playback
void pz_music_play(pz_music* music);
void pz_music_stop(pz_music* music);
void pz_music_pause(pz_music* music);
bool pz_music_is_playing(pz_music* music);

// Layer control
void  pz_music_set_layer_enabled(pz_music* music, int layer, bool enabled);
bool  pz_music_get_layer_enabled(pz_music* music, int layer);
void  pz_music_set_layer_volume(pz_music* music, int layer, float volume);
float pz_music_get_layer_volume(pz_music* music, int layer);

// Master volume
void  pz_music_set_volume(pz_music* music, float volume);
float pz_music_get_volume(pz_music* music);

// Audio callback (called from audio thread)
void pz_music_render(pz_music* music, float* buffer, int num_frames, int num_channels);

// Update (call from main thread each frame for timing)
void pz_music_update(pz_music* music, float dt);
```

## MIDI Channel Assignment

To allow independent layer control, each layer uses a dedicated MIDI channel:

| Layer | Name   | MIDI Channel | GM Instrument              |
|-------|--------|--------------|----------------------------|
| 0     | drums  | 9 (10)       | Standard drum kit          |
| 1     | bass   | 0            | 58 - Tuba                  |
| 2     | melody | 1            | 56 - Trumpet               |

Note: MIDI channel 10 (index 9) is reserved for drums in General MIDI.

## Implementation Phases

### Phase 1: Audio Foundation
1. Create `pz_audio.c/h` wrapping sokol_audio
2. Create `tsf_impl.c` for TinySoundFont implementation
3. Test: Play a single note through the system

### Phase 2: Basic MIDI Playback
1. Create `pz_music.c/h` with single-layer support
2. Load SoundFont and MIDI file
3. Implement playback loop with tempo handling
4. Test: Play a single MIDI file

### Phase 3: Multi-layer Support
1. Add layer array to pz_music
2. Each layer tracks its own playback position
3. Mix all layers together in render callback
4. Test: Play 3 layers, toggle with keys

### Phase 4: Music Assets
1. Create `march_drums.mid` - snare pattern
2. Create `march_bass.mid` - bass/tuba line
3. Create `march_melody.mid` - trumpet melody
4. Test: Full 3-layer march plays correctly

## Thread Safety Notes

The audio callback runs on a separate thread. Key considerations:

1. **Atomic flags** for layer enabled/disabled state
2. **Lock-free volume changes** (single float writes are atomic on most platforms)
3. **No allocations** in the render callback
4. **Pre-allocate voices** in TinySoundFont to avoid runtime allocation

```c
// In pz_music struct
typedef struct pz_music_layer {
    tml_message* midi_data;      // Loaded MIDI (owned)
    tml_message* current_msg;    // Current position in playback
    double       playback_time;  // Current time in ms
    float        volume;         // 0.0 - 1.0
    int          midi_channel;   // Which MIDI channel this layer uses
    bool         enabled;        // Is this layer playing?
    bool         loop;           // Loop when finished?
} pz_music_layer;

typedef struct pz_music {
    tsf*              soundfont;
    pz_music_layer    layers[PZ_MUSIC_MAX_LAYERS];
    int               layer_count;
    float             master_volume;
    bool              playing;
    int               sample_rate;
} pz_music;
```

## Military March Music Notes

Based on the Wii Tanks score screenshots, the march is approximately:
- **Tempo**: 120 BPM (standard march tempo)
- **Time signature**: 4/4 (or 2/4 cut time)
- **Key**: G major (one sharp)

### Drum Pattern (Layer 0, Channel 9)
Typical snare drum march pattern with:
- Quarter note bass drum on beats 1 and 3
- Eighth notes on snare with accents
- Triplet fills

### Bass Line (Layer 1, Channel 0 - Tuba)
- Root notes following chord progression
- Quarter and half notes
- Typical I-IV-V-I march progression

### Melody (Layer 2, Channel 1 - Trumpet)
- Staccato eighth notes
- Dotted rhythms (dotted-eighth + sixteenth)
- Stays in upper register
- Repeating 4 or 8 bar phrase

## Future Enhancements (Not in Initial Scope)

1. **Beat-synchronized transitions** - Queue layer changes for next beat
2. **Volume fades** - Smooth fade in/out over N beats
3. **Dynamic intensity** - More layers = more intense combat
4. **Stingers** - One-shot musical cues (victory, death)
5. **Crossfade between songs** - Menu music → gameplay music
