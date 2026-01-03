# Tile Registry Refactoring Plan

## Goal

Decouple tile definitions from hardcoded values. Maps reference tile names (e.g., `oak_dark`), and tile definitions in `.tile` files provide:
- Ground texture
- Wall texture (top)
- Wall side texture
- Movement properties (speed_multiplier, friction)

## Design Decisions

1. **Wall texture per tile type**: Each tile type has its own wall textures (top + side)
2. **Tile naming decoupled from textures**: Maps use semantic names like `oak_dark`, tile files specify actual texture paths
3. **`.tile` files are source of truth**: Removed hardcoded `texture_props_table` from `pz_map.c`
4. **Height determines wall vs ground**: Unchanged - height > 0 = wall, uses wall textures from tile config
5. **Fallback handling**: Missing tiles get orange checkerboard fallback texture

## Tile File Format

```
# assets/tiles/oak_dark.tile
name oak_dark
ground_texture assets/textures/wood_rustic_dark.png
wall_texture assets/textures/wood_rustic_dark.png      # optional, defaults to ground
wall_side_texture assets/textures/wood_rustic_dark.png # optional, defaults to wall
speed_multiplier 1.0
friction 1.0
```

## Completed Work

### New Files Created
- [x] `src/game/pz_tile_registry.h` - Tile registry API
- [x] `src/game/pz_tile_registry.c` - Tile registry implementation
- [x] `assets/textures/fallback.png` - Orange fallback texture (placeholder)

### Updated `.tile` Files (new format)
- [x] `assets/tiles/wood_oak_brown.tile`
- [x] `assets/tiles/wood_oak_amber.tile`
- [x] `assets/tiles/wood_oak_veneer.tile`
- [x] `assets/tiles/wood_pine_light.tile`
- [x] `assets/tiles/wood_rustic_dark.tile`
- [x] `assets/tiles/wood_walnut.tile`
- [x] `assets/tiles/mud_wet.tile`
- [x] `assets/tiles/mud_churned.tile`
- [x] `assets/tiles/mud_dry.tile`
- [x] `assets/tiles/carpet_gray.tile`
- [x] `assets/tiles/carpet_beige.tile`
- [x] `assets/tiles/water_caustic.tile`

### Removed Legacy Files
- [x] Deleted `assets/tiles/*.toml` files

### Header Changes

#### `pz_map.h`
- [x] Added forward declaration for `pz_tile_registry`
- [x] Removed `texture` field from `pz_tile_def` struct
- [x] Added `tile_registry` pointer to `pz_map` struct
- [x] Added `pz_map_set_tile_registry()` function
- [x] Added `pz_map_get_friction()` function
- [x] Added `pz_map_load_with_registry()` function

#### `pz_map_render.h`
- [x] Added `pz_tile_registry.h` include
- [x] Updated `pz_map_renderer_create()` to take `tile_registry` parameter

### Implementation Changes

#### `pz_map.c`
- [x] Added `#include "pz_tile_registry.h"`
- [x] Removed hardcoded `texture_props_table`
- [x] Removed `pz_get_texture_props()` function
- [x] Added `pz_map_set_tile_registry()` implementation
- [x] Updated `pz_map_get_speed_multiplier()` to use tile registry
- [x] Added `pz_map_get_friction()` implementation using tile registry
- [x] Added `pz_map_load_with_registry()` implementation
- [x] Removed `snprintf` calls that populated removed `texture` field in `pz_map_add_tile_def()`

#### `pz_map_render.c`
- [x] Added `wall_batch` struct (like `ground_batch` but with top + side textures)
- [x] Replaced single `wall_buffer`/`wall_vertex_count` with `wall_batches[]`/`wall_batch_count`
- [x] Added `tile_registry` pointer to `pz_map_renderer` struct
- [x] Removed `tile_textures[]` cache (now uses registry)
- [x] Removed `wall_top_tex`/`wall_side_tex` (now per-batch)
- [x] Replaced `load_tile_texture()` with `get_tile_config()`
- [x] Updated `pz_map_renderer_create()` to take registry parameter
- [x] Updated `pz_map_renderer_destroy()` to clean up wall batches
- [x] Updated `pz_map_renderer_set_map()` to build per-tile-type wall batches
- [x] Updated `pz_map_renderer_draw_walls()` to draw multiple batches

#### `main.c`
- [x] Added `#include "game/pz_tile_registry.h"`
- [x] Added `tile_registry` to `app_state` struct
- [x] Create tile registry in `app_init()` after texture manager
- [x] Set tile registry on map in `map_session_load()`
- [x] Pass tile registry to `pz_map_renderer_create()`
- [x] Destroy tile registry in `app_cleanup()`

#### `CMakeLists.txt`
- [x] Added `src/game/pz_tile_registry.c` to `GAME_SOURCES`

#### `tests/test_map.c`
- [x] Removed `map_texture_props` test (used removed `pz_get_texture_props`)
- [x] Simplified `map_speed_multiplier` test to work without registry

#### `tests/CMakeLists.txt`
- [x] Added tile registry and dependencies to test_map

## Remaining Work

### Build Fixes
- [x] Fix test_map linking (add pz_ds + stb_image_write, provide GL33 stub)

### Testing
- [x] Build completes successfully
- [x] `make test` passes
- [x] Visual validation with screenshot

### Optional Enhancements
- [ ] Create proper orange checkerboard fallback.png (currently uses mud_dry.png as placeholder)
- [ ] Add tile registry tests
- [ ] Hot-reload support for .tile files

## File Dependencies

```
main.c
  └── pz_tile_registry (created at startup)
        └── loads assets/tiles/*.tile
        └── loads textures via pz_texture_manager

map_session_load()
  └── pz_map_load()
  └── pz_map_set_tile_registry(map, registry)
  └── pz_map_renderer_create(renderer, tex_manager, registry)
        └── pz_map_renderer_set_map()
              └── get_tile_config() for each tile type
              └── builds ground_batches[] with ground textures
              └── builds wall_batches[] with wall/side textures
```

## Current Build Error

```
Resolved by adding missing test sources and a GL33 vtable stub for null builds.
```
