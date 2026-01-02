# Tooling and Debugging Plan

Good tooling is essential for a project this size. We'll invest upfront in debugging capabilities that pay dividends throughout development.

## Core Philosophy

1. **Visual debugging by default** - See what's happening, don't just log it
2. **Hot-reload everything** - Maps, shaders, config - minimize restart cycles
3. **Record and replay** - Reproduce bugs deterministically
4. **Metrics always available** - Performance visible at a glance
5. **Test early, test continuously** - Catch regressions immediately

---

## 1. Debug Overlay System

An in-game overlay toggled with backtick (`) that shows real-time info.

### Layers (toggle individually):

```
[F1] Editor mode
[F2] Debug overlay
[F3] Physics debug (collision shapes, rays)
[F4] Network debug (packets, latency graph)
[F5] Entity inspector
[F6] Performance profiler
[F7] Memory stats
[F8] Console/log viewer
```

### Debug Overlay Contents (F2):

```
┌─────────────────────────────────────┐
│ FPS: 60.0  Frame: 16.2ms            │
│ Tick: 12345  Delta: 0.0167          │
│ Entities: 24 (tanks:4 proj:12 ...)  │
│ Draw calls: 45  Tris: 12.4k         │
│ Memory: 24.5 MB (peak: 28.1 MB)     │
│                                     │
│ [Mouse] World: (12.4, 8.2)          │
│ [Camera] Pos: (16, 16, 20)          │
└─────────────────────────────────────┘
```

### Implementation:

```c
typedef struct pz_debug_overlay {
    bool     enabled;
    bool     show_physics;
    bool     show_network;
    bool     show_entities;
    bool     show_profiler;
    bool     show_memory;
    bool     show_console;
    
    // Console
    char     console_input[256];
    char     console_history[64][256];
    int      console_history_count;
    
    // Log buffer (ring)
    char     log_buffer[128][256];
    int      log_head;
} pz_debug_overlay;

void pz_debug_overlay_render(pz_debug_overlay* dbg, pz_renderer* r);
void pz_debug_log(pz_debug_overlay* dbg, const char* fmt, ...);
```

---

## 2. Physics Debug Visualization

When F3 is active:

- **Collision shapes:** Green wireframes for all colliders
- **Velocity vectors:** Blue arrows showing movement direction
- **Raycast visualization:** Yellow lines for LOS checks, bullet paths
- **Collision points:** Red dots where collisions occur
- **Trigger zones:** Semi-transparent colored regions

```c
// Debug draw API (immediate mode, rendered at end of frame)
void pz_debug_draw_line(pz_vec2 a, pz_vec2 b, pz_color color);
void pz_debug_draw_circle(pz_vec2 center, float radius, pz_color color);
void pz_debug_draw_rect(pz_vec2 min, pz_vec2 max, pz_color color);
void pz_debug_draw_arrow(pz_vec2 from, pz_vec2 to, pz_color color);
void pz_debug_draw_text(pz_vec2 pos, const char* text, pz_color color);

// Called internally by systems when debug enabled
void pz_physics_debug_draw(pz_collision_world* world);
```

---

## 3. Network Debug Tools

### Latency Simulator

Inject artificial network conditions for testing:

```c
typedef struct pz_net_simulator {
    bool   enabled;
    float  latency_ms;          // Added delay
    float  latency_jitter_ms;   // Random variation
    float  packet_loss_pct;     // 0-100
    float  duplicate_pct;       // Packet duplication
    bool   out_of_order;        // Shuffle packet delivery
} pz_net_simulator;
```

### Network Debug Overlay (F4):

```
┌─────────────────────────────────────┐
│ QUIC Connection: ESTABLISHED        │
│ RTT: 45ms (min:32 max:78)           │
│ Sent: 124.5 KB/s  Recv: 89.2 KB/s   │
│ Datagrams: ↑60/s ↓30/s              │
│ Packet loss: 0.2%                   │
│                                     │
│ [Latency Graph - last 5 seconds]    │
│ ▁▂▃▂▁▂▄▃▂▁▂▃▂▁▂▃▄▅▃▂▁▂▃▂▁          │
│                                     │
│ Input seq: 12340 (ack: 12335)       │
│ Server tick: 45678                  │
│ Prediction error: 2.3 units         │
└─────────────────────────────────────┘
```

### Packet Logger

Detailed logging of all network traffic (toggleable, writes to file):

```c
void pz_net_log_packet(const char* direction, pz_msg_type type, 
    const uint8_t* data, size_t len);
    
// Output: packets.log
// [12345.678] SEND DATAGRAM CLIENT_INPUT seq=1234 buttons=0x05 angle=180
// [12345.680] RECV DATAGRAM GAME_DELTA tick=5678 tanks=4 projs=8
```

---

## 4. Entity Inspector (F5)

Click on any entity to see its full state:

```
┌─ Tank #3 (Player: "Alice") ─────────┐
│ Position: (12.45, 8.23)             │
│ Angle: 45.0°  Turret: 90.0°         │
│ Velocity: (2.1, 0.0)                │
│ HP: 10/10                           │
│ Weapon: RAPID_FIRE (ammo: 8)        │
│ Mines: 2                            │
│ State: MOVING                       │
│ Team: 1 (Blue)                      │
│ Owner: Client #2                    │
│                                     │
│ [Prediction]                        │
│ Last confirmed: input #1234         │
│ Pending inputs: 3                   │
│ Error: 0.0 units                    │
└─────────────────────────────────────┘
```

Allow editing values in debug builds for testing.

---

## 5. Performance Profiler (F6)

Hierarchical frame profiler:

```
┌─ Frame 16.2ms ──────────────────────┐
│ ├─ Update 4.2ms                     │
│ │  ├─ Physics 1.8ms                 │
│ │  ├─ Tanks 0.9ms                   │
│ │  ├─ Projectiles 0.4ms             │
│ │  └─ Network 1.1ms                 │
│ ├─ Render 11.5ms                    │
│ │  ├─ Ground 2.1ms                  │
│ │  ├─ Walls 3.4ms                   │
│ │  ├─ Entities 4.2ms                │
│ │  ├─ Particles 0.8ms               │
│ │  └─ UI 1.0ms                      │
│ └─ Swap 0.5ms                       │
└─────────────────────────────────────┘
```

Implementation:

```c
// Usage:
PZ_PROFILE_BEGIN("Physics");
pz_physics_update(world, dt);
PZ_PROFILE_END();

// Macros expand to:
typedef struct pz_profile_scope {
    const char* name;
    uint64_t    start_time;
    uint64_t    end_time;
    int         parent;
} pz_profile_scope;

void pz_profile_begin(const char* name);
void pz_profile_end(void);
void pz_profile_frame_end(void);  // Finalize and store frame data
```

---

## 6. Memory Debugging (F7)

Track all allocations through our `pz_alloc` system:

```
┌─ Memory ────────────────────────────┐
│ Total: 24.5 MB (peak: 28.1 MB)      │
│ Allocations: 1,234 active           │
│                                     │
│ By Category:                        │
│   Entities:    8.2 MB (412 allocs)  │
│   Rendering:   6.1 MB (89 allocs)   │
│   Map:         4.8 MB (23 allocs)   │
│   Audio:       3.2 MB (156 allocs)  │
│   Network:     1.4 MB (45 allocs)   │
│   Other:       0.8 MB (509 allocs)  │
│                                     │
│ [!] Leak suspects: 0                │
└─────────────────────────────────────┘
```

Implementation:

```c
typedef enum pz_mem_category {
    PZ_MEM_GENERAL,
    PZ_MEM_ENTITY,
    PZ_MEM_RENDER,
    PZ_MEM_MAP,
    PZ_MEM_AUDIO,
    PZ_MEM_NETWORK,
    PZ_MEM_COUNT
} pz_mem_category;

void* pz_alloc_tagged(size_t size, pz_mem_category cat, const char* file, int line);
#define pz_alloc_cat(size, cat) pz_alloc_tagged(size, cat, __FILE__, __LINE__)

// In debug builds, track every allocation
typedef struct pz_alloc_record {
    void*           ptr;
    size_t          size;
    pz_mem_category category;
    const char*     file;
    int             line;
    uint64_t        frame;
} pz_alloc_record;
```

---

## 7. Console System (F8)

In-game console for commands and live tweaking:

```
> help
Available commands:
  spawn_tank <x> <y> <team>    - Spawn a tank
  kill_all                      - Kill all tanks
  give_weapon <weapon>          - Give weapon to player
  set_hp <amount>               - Set player HP
  teleport <x> <y>              - Teleport player
  reload_map                    - Hot-reload current map
  reload_shaders                - Recompile all shaders
  net_sim <latency> <loss>      - Enable network simulator
  timescale <factor>            - Slow/speed time (0.1-10)
  pause                         - Pause game
  step                          - Advance one frame (when paused)
  record <filename>             - Start recording inputs
  replay <filename>             - Play back recorded inputs
  dump_entities                 - Print all entities to log
  quit                          - Exit game

> timescale 0.25
Time scale set to 0.25x

> spawn_tank 10 10 2
Spawned tank #5 at (10, 10) for team 2
```

### Console Variables (CVars):

```c
// Define tweakable variables
PZ_CVAR_FLOAT(g_tank_speed, 5.0f, "Tank movement speed");
PZ_CVAR_INT(g_tank_hp, 10, "Tank starting HP");
PZ_CVAR_BOOL(g_debug_physics, false, "Show physics debug");

// Console commands:
> set g_tank_speed 8.0
> get g_tank_speed
g_tank_speed = 8.0 (default: 5.0)
> reset g_tank_speed
g_tank_speed reset to 5.0
```

---

## 8. Record and Replay System

Deterministic replay for bug reproduction:

```c
typedef struct pz_replay_frame {
    uint32_t        tick;
    uint8_t         input_count;
    pz_tank_input   inputs[PZ_MAX_PLAYERS];
    uint32_t        random_seed;  // If we use RNG
} pz_replay_frame;

typedef struct pz_replay {
    uint32_t          version;
    char              map_name[64];
    uint32_t          frame_count;
    pz_replay_frame*  frames;
} pz_replay;

void pz_replay_start_recording(pz_replay* replay);
void pz_replay_record_frame(pz_replay* replay, pz_game* game);
void pz_replay_stop_recording(pz_replay* replay, const char* filename);
void pz_replay_load(pz_replay* replay, const char* filename);
void pz_replay_playback_frame(pz_replay* replay, pz_game* game);
```

### Replay Controls:
- Play/Pause
- Step forward/backward
- Seek to tick
- Speed control (0.25x, 0.5x, 1x, 2x, 4x)
- Save snapshot at any point

---

## 9. Hot-Reload System

Minimize iteration time by reloading assets without restart:

| Asset | Trigger | Notes |
|-------|---------|-------|
| Maps | File change or F9 | Full map reload, reset entities |
| Shaders | File change or F10 | Recompile, keep game state |
| Textures | File change | Re-upload to GPU |
| Config | File change | Re-read CVars |
| Code | N/A | Requires restart (but keep state minimal) |

```c
typedef struct pz_hot_reload {
    pz_file_watch*  watches;
    int             watch_count;
} pz_hot_reload;

void pz_hot_reload_init(pz_hot_reload* hr);
void pz_hot_reload_add(pz_hot_reload* hr, const char* path, pz_reload_cb callback);
void pz_hot_reload_poll(pz_hot_reload* hr);  // Call each frame
```

---

## 10. Testing Framework

Lightweight C test framework:

```c
// test_math.c
PZ_TEST(vec2_add) {
    pz_vec2 a = {1, 2};
    pz_vec2 b = {3, 4};
    pz_vec2 c = pz_vec2_add(a, b);
    PZ_ASSERT_FLOAT_EQ(c.x, 4.0f);
    PZ_ASSERT_FLOAT_EQ(c.y, 6.0f);
}

PZ_TEST(vec2_normalize) {
    pz_vec2 v = {3, 4};
    pz_vec2 n = pz_vec2_normalize(v);
    PZ_ASSERT_FLOAT_NEAR(pz_vec2_len(n), 1.0f, 0.0001f);
}

// Run with: ./tankgame --test
// Or: ./tankgame --test vec2_*
```

### Test Categories:

1. **Unit tests** - Pure functions (math, data structures)
2. **Integration tests** - Systems working together (physics + entities)
3. **Snapshot tests** - Render a scene, compare to golden image
4. **Network tests** - Simulate client/server locally
5. **Replay tests** - Play recorded sessions, verify final state

---

## 11. Build Configurations

```cmake
# Debug: All tooling, asserts, no optimization
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DPZ_DEBUG=1 -DPZ_ASSERTS=1 -DPZ_PROFILER=1")

# Dev: Tooling + optimization (daily development)
set(CMAKE_C_FLAGS_DEV "-g -O2 -DPZ_DEBUG=1 -DPZ_ASSERTS=1 -DPZ_PROFILER=1")

# Release: No debug, full optimization
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -DPZ_DEBUG=0 -DPZ_ASSERTS=0 -DPZ_PROFILER=0")
```

---

## 12. Logging System

Structured, filterable logging:

```c
typedef enum pz_log_level {
    PZ_LOG_TRACE,
    PZ_LOG_DEBUG,
    PZ_LOG_INFO,
    PZ_LOG_WARN,
    PZ_LOG_ERROR,
} pz_log_level;

typedef enum pz_log_category {
    PZ_LOG_CAT_CORE,
    PZ_LOG_CAT_RENDER,
    PZ_LOG_CAT_PHYSICS,
    PZ_LOG_CAT_NETWORK,
    PZ_LOG_CAT_GAME,
    PZ_LOG_CAT_AUDIO,
    PZ_LOG_CAT_EDITOR,
} pz_log_category;

#define pz_log_info(cat, fmt, ...) \
    pz_log(PZ_LOG_INFO, cat, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Usage:
pz_log_info(PZ_LOG_CAT_NETWORK, "Connected to %s:%d", host, port);
pz_log_warn(PZ_LOG_CAT_PHYSICS, "Collision overflow, %d contacts", count);

// Filter in console:
> log_level warn            // Only warnings and errors
> log_filter network        // Only network category
> log_filter +physics +game // Multiple categories
```

---

## Summary: Debug Key Bindings

| Key | Function |
|-----|----------|
| ` | Toggle console |
| F1 | Editor mode |
| F2 | Debug overlay |
| F3 | Physics debug draw |
| F4 | Network debug |
| F5 | Entity inspector |
| F6 | Profiler |
| F7 | Memory stats |
| F8 | Full console view |
| F9 | Reload map |
| F10 | Reload shaders |
| F11 | Toggle fullscreen |
| F12 | Screenshot |
| Pause | Pause game |
| ] | Step frame (when paused) |
| [ | Step back (replay mode) |

---

## Implementation Priority

1. **Phase 1 (with core engine):**
   - Basic logging
   - Debug overlay (FPS, basic stats)
   - Memory tracking (simple)
   - Hot-reload for maps

2. **Phase 2 (with gameplay):**
   - Physics debug draw
   - Entity inspector
   - Console with basic commands
   - CVars

3. **Phase 3 (with networking):**
   - Network debug overlay
   - Latency simulator
   - Packet logger

4. **Phase 4 (polish):**
   - Full profiler
   - Record/replay
   - Snapshot tests
