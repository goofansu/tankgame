# Map File Format

Tank game maps use a text-based format with combined height+terrain cells.

## Structure

```
name Map Name
tile_size 2.0

# Tile definitions
tile SYMBOL name

# Object tags
tag NAME type key=value...

# Optional water level
water_level -1

<grid>
... cells ...
</grid>

# Spawn/enemy (alternative to tags)
spawn X Y ANGLE TEAM TEAM_SPAWN
enemy X Y ANGLE TYPE

# Lighting
sun_direction X Y Z
sun_color R G B
ambient_color R G B
ambient_darkness 0.0-1.0
```

## Grid Section

The grid is wrapped in `<grid>...</grid>` tags. The map size is automatically detected from the grid:
- **Width**: Number of cells in the first row
- **Height**: Number of rows

All rows must have the same number of cells. Empty lines inside the grid are ignored.

### Cell Format

Format: `HEIGHT TILE [|tag,tag,...]`

- **HEIGHT**: Integer, can be negative
  - `>0` = wall (blocks movement and bullets)
  - `0` = ground (passable)
  - `<0` = pit (blocks movement, bullets fly over)
- **TILE**: Single character from tile definitions
- **|tags**: Optional, comma-separated tag references

Examples:
```
2#      # Height 2 stone wall
0.      # Ground level floor
-1.     # Pit (1 level deep)
0.|P1   # Ground with spawn point P1
0.|P1,E1  # Ground with both spawn and enemy
```

## Tile Definitions

```
tile . ground
tile # stone
tile : mud
tile * ice
```

Tiles reference textures at `assets/textures/<name>.png`. Built-in behaviors:
- `mud` → 50% movement speed
- `ice` → 120% speed, reduced friction

## Object Tags

Define reusable objects, place them in grid cells:

```
tag P1 spawn angle=0.785 team=0
tag E1 enemy angle=3.14 type=hunter
tag W1 powerup type=machine_gun respawn=15
tag W2 powerup type=ricochet respawn=20
tag B1 barrier tile=cobble health=20

<grid>
2# 0.|P1 0.|E1 0.|W1 0.|B1 2#
</grid>
```

**Spawn params:** `angle`, `team`, `team_spawn`
**Enemy params:** `angle`, `type` (sentry, skirmisher, hunter, sniper)
**Powerup params:** `type` (machine_gun, ricochet), `respawn` (seconds, default 15)
**Barrier params:** `tile` (tile name for texture), `health` (default 20)

## Water Level

```
water_level -1
```

Water surface renders slightly below ground. Tiles at or below water_level are submerged.

## Lighting

Day mode (sun enabled):
```
sun_direction 0.4 -0.8 0.3
sun_color 1.0 0.95 0.85
ambient_color 0.4 0.45 0.5
ambient_darkness 0.0
```

Night mode (no sun, dark):
```
ambient_color 0.12 0.12 0.15
ambient_darkness 0.85
```

## Complete Example

```
name Test Arena
tile_size 2.0

tile . ground
tile # stone
tile : mud

tag P1 spawn angle=0.785 team=0
tag P2 spawn angle=2.356 team=0
tag E1 enemy angle=3.14 type=sentry

<grid>
2# 2# 2# 2# 2# 2#
2# 0.|P1 0. 0: 0. 2#
2# 0. 0.|E1 0. 0.|P2 2#
2# 2# 2# 2# 2# 2#
</grid>

ambient_color 0.12 0.12 0.15
ambient_darkness 0.85
```

## Implementation

- Parser: `src/game/pz_map.c`
- Header: `src/game/pz_map.h`
- Renderer: `src/game/pz_map_render.c`
- Maps: `assets/maps/*.map`
