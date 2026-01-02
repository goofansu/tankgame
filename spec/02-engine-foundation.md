# Technical Engine Foundation

## Architecture Principles

- **C17** standard for broad compatibility
- **Naming:** `pz_module_function_name()`, `pz_module_struct`
- **Memory:** All allocations through `pz_alloc()`, `pz_free()`, `pz_realloc()`
- **Dependency injection:** Systems receive context pointers, no globals
- **Intrusive lists:** Embedded `pz_list_node` in structs
- **Renderer isolation:** Engine/game code depends only on `pz_renderer`, no API-specific types
- **Portability:** Keep shaders/features within a common subset (GL 3.3 / GLES 3.0) when possible

## Directory Structure

```
tankgame/
├── src/
│   ├── main.c
│   ├── core/           # Foundation utilities
│   │   ├── pz_mem.c/h      # Custom allocator
│   │   ├── pz_list.c/h     # Intrusive lists
│   │   ├── pz_array.c/h    # Dynamic arrays
│   │   ├── pz_hashmap.c/h  # Hash map
│   │   ├── pz_str.c/h      # String utilities
│   │   ├── pz_math.c/h     # vec2, vec3, mat4, etc.
│   │   ├── pz_log.c/h      # Logging
│   │   └── pz_platform.c/h # Platform abstraction
│   ├── engine/         # Core engine systems
│   │   ├── pz_engine.c/h   # Main engine context
│   │   ├── pz_window.c/h   # SDL window management
│   │   ├── pz_input.c/h    # Input handling
│   │   ├── render/         # Renderer API + backends
│   │   │   ├── pz_renderer.c/h       # Backend-agnostic API
│   │   │   ├── pz_render_backend.c/h # Backend selection/vtable
│   │   │   ├── pz_render_gl.c/h      # OpenGL 3.3 backend
│   │   │   ├── pz_render_null.c/h    # No-op backend (tests)
│   │   │   ├── pz_shader.c/h         # Shader descriptors
│   │   │   ├── pz_texture.c/h        # Texture descriptors
│   │   │   └── pz_mesh.c/h           # Mesh data
│   │   ├── pz_audio.c/h    # Audio playback
│   │   └── pz_assets.c/h   # Asset loading/management
│   ├── game/           # Game-specific code
│   │   ├── pz_game.c/h     # Game state management
│   │   ├── pz_tank.c/h     # Tank entity
│   │   ├── pz_projectile.c/h # Bullets, shells
│   │   ├── pz_mine.c/h     # Mine entities
│   │   ├── pz_pickup.c/h   # Pickup items
│   │   ├── pz_map.c/h      # Map loading/rendering
│   │   ├── pz_physics.c/h  # 2D collision
│   │   ├── pz_camera.c/h   # Camera control
│   │   └── modes/          # Game mode logic
│   │       ├── pz_mode.c/h
│   │       ├── pz_deathmatch.c/h
│   │       ├── pz_ctf.c/h
│   │       └── pz_domination.c/h
│   ├── editor/         # In-game editor
│   │   └── pz_editor.c/h
│   └── net/            # Networking
│       ├── pz_net.c/h
│       ├── pz_protocol.c/h
│       ├── pz_client.c/h
│       └── pz_server.c/h
├── shaders/
│   ├── basic.vert/frag
│   ├── tracks.vert/frag
│   └── light.vert/frag
├── assets/
│   ├── textures/
│   ├── meshes/
│   ├── sounds/
│   └── maps/
├── build/
└── CMakeLists.txt
```

## Core Utilities

### Memory (`core/pz_mem.h`)

```c
void  pz_mem_init(void);
void  pz_mem_shutdown(void);
void* pz_alloc(size_t size);
void* pz_calloc(size_t count, size_t size);
void* pz_realloc(void* ptr, size_t size);
void  pz_free(void* ptr);

// Debug/stats
size_t pz_mem_get_allocated(void);
void   pz_mem_dump_leaks(void);
```

### Intrusive Lists (`core/pz_list.h`)

```c
typedef struct pz_list_node {
    struct pz_list_node* next;
    struct pz_list_node* prev;
} pz_list_node;

typedef struct pz_list {
    pz_list_node head;
    size_t       count;
} pz_list;

void  pz_list_init(pz_list* list);
void  pz_list_append(pz_list* list, pz_list_node* node);
void  pz_list_prepend(pz_list* list, pz_list_node* node);
void  pz_list_remove(pz_list* list, pz_list_node* node);
bool  pz_list_empty(const pz_list* list);

#define PZ_LIST_FOREACH(list, node) \
    for (pz_list_node* node = (list)->head.next; node != &(list)->head; node = node->next)

#define PZ_LIST_ENTRY(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))
```

### Dynamic Array (`core/pz_array.h`)

```c
// Usage: int* arr = NULL; pz_array_push(arr, 42);
#define pz_array_push(arr, val)   // ...
#define pz_array_pop(arr)         // ...
#define pz_array_len(arr)         // ...
#define pz_array_cap(arr)         // ...
#define pz_array_free(arr)        // ...
#define pz_array_clear(arr)       // ...
```

STB-style stretchy buffers, but using our allocator.

### Math (`core/pz_math.h`)

```c
typedef struct pz_vec2 { float x, y; } pz_vec2;
typedef struct pz_vec3 { float x, y, z; } pz_vec3;
typedef struct pz_vec4 { float x, y, z, w; } pz_vec4;
typedef struct pz_mat4 { float m[16]; } pz_mat4;

// vec2 operations
pz_vec2 pz_vec2_add(pz_vec2 a, pz_vec2 b);
pz_vec2 pz_vec2_sub(pz_vec2 a, pz_vec2 b);
pz_vec2 pz_vec2_scale(pz_vec2 v, float s);
float   pz_vec2_dot(pz_vec2 a, pz_vec2 b);
float   pz_vec2_len(pz_vec2 v);
pz_vec2 pz_vec2_normalize(pz_vec2 v);
pz_vec2 pz_vec2_rotate(pz_vec2 v, float angle);
pz_vec2 pz_vec2_reflect(pz_vec2 v, pz_vec2 normal);

// mat4 operations
pz_mat4 pz_mat4_identity(void);
pz_mat4 pz_mat4_mul(pz_mat4 a, pz_mat4 b);
pz_mat4 pz_mat4_translate(pz_vec3 t);
pz_mat4 pz_mat4_rotate_x(float angle);
pz_mat4 pz_mat4_rotate_y(float angle);
pz_mat4 pz_mat4_rotate_z(float angle);
pz_mat4 pz_mat4_scale(pz_vec3 s);
pz_mat4 pz_mat4_perspective(float fov, float aspect, float near, float far);
pz_mat4 pz_mat4_look_at(pz_vec3 eye, pz_vec3 target, pz_vec3 up);
```

## Engine Context

```c
typedef struct pz_engine {
    // Subsystems
    pz_window*   window;
    pz_renderer* renderer;
    pz_input*    input;
    pz_audio*    audio;
    pz_assets*   assets;
    
    // Timing
    double       time;
    double       delta_time;
    uint64_t     frame;
    
    // State
    bool         running;
} pz_engine;

pz_engine* pz_engine_create(const pz_engine_config* config);
void       pz_engine_destroy(pz_engine* engine);
void       pz_engine_run(pz_engine* engine, pz_game* game);
```

## Rendering

### Renderer Abstraction
- Backend-agnostic `pz_renderer` API used everywhere outside render backends
- Backends are selected at init (OpenGL 3.3 first; future GLES/Vulkan)
- Opaque handles in headers, no GL types leak across modules

### Backend Capabilities (OpenGL 3.3 Core First)
- Vertex Array Objects
- Framebuffer Objects (for track accumulation)
- Basic shaders (no geometry shaders, no compute)
- 2D texture arrays for atlas

### Render Pipeline

1. **Clear** framebuffer
2. **Render ground** with track accumulation texture
3. **Render 3D objects** (walls, crates, tanks) sorted by depth
4. **Render particles/effects**
5. **Render UI** (orthographic overlay)

### Track Accumulation System

```c
typedef uint32_t pz_texture_handle;
typedef uint32_t pz_render_target_handle;

typedef struct pz_track_layer {
    pz_render_target_handle target;
    pz_texture_handle       texture;   // RGBA8, map-sized
    int                     width;
    int                     height;
} pz_track_layer;

void pz_track_layer_init(pz_renderer* r, pz_track_layer* layer, int width, int height);
void pz_track_layer_add_track(pz_renderer* r, pz_track_layer* layer, pz_vec2 pos, float angle, float width);
void pz_track_layer_clear(pz_renderer* r, pz_track_layer* layer);
```

## Input

```c
typedef struct pz_input {
    // Keyboard
    bool keys[512];
    bool keys_pressed[512];   // This frame only
    bool keys_released[512];
    
    // Mouse
    pz_vec2 mouse_pos;
    pz_vec2 mouse_delta;
    bool    mouse_buttons[8];
    float   scroll_delta;
} pz_input;

void   pz_input_update(pz_input* input);  // Call each frame
bool   pz_input_key_down(pz_input* input, int key);
bool   pz_input_key_pressed(pz_input* input, int key);
pz_vec2 pz_input_mouse_world_pos(pz_input* input, pz_camera* camera);
```

## Audio

Using miniaudio (portable, web-friendly):

```c
typedef struct pz_audio {
    // Internal state
} pz_audio;

typedef struct pz_sound {
    // Sound data
} pz_sound;

pz_sound* pz_audio_load_sound(pz_audio* audio, const char* path);
void      pz_audio_play(pz_audio* audio, pz_sound* sound, float volume, float pan);
void      pz_audio_play_at(pz_audio* audio, pz_sound* sound, pz_vec2 world_pos, pz_vec2 listener_pos);
```

## Asset Management

```c
typedef struct pz_assets {
    pz_hashmap* textures;
    pz_hashmap* meshes;
    pz_hashmap* sounds;
    pz_hashmap* shaders;
} pz_assets;

pz_texture* pz_assets_get_texture(pz_assets* assets, const char* name);
pz_mesh*    pz_assets_get_mesh(pz_assets* assets, const char* name);
pz_sound*   pz_assets_get_sound(pz_assets* assets, const char* name);
pz_shader*  pz_assets_get_shader(pz_assets* assets, const char* name);

// Hot-reload support
void pz_assets_check_reload(pz_assets* assets);
```

## Platform Layer

```c
// File watching for hot-reload
typedef void (*pz_file_changed_cb)(const char* path, void* userdata);

void pz_platform_watch_file(const char* path, pz_file_changed_cb cb, void* userdata);
void pz_platform_poll_file_changes(void);

// Time
double pz_platform_get_time(void);

// File I/O
char* pz_platform_read_file(const char* path, size_t* out_size);
bool  pz_platform_write_file(const char* path, const void* data, size_t size);
```

## Build System

CMake for cross-platform, with Emscripten toolchain file for web:

```cmake
cmake_minimum_required(VERSION 3.16)
project(tankgame C)

set(CMAKE_C_STANDARD 17)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2 -s USE_WEBGL2=1 -s FULL_ES3=1")
endif()

find_package(SDL2 REQUIRED)
# ... etc
```
