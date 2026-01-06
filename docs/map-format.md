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

# Optional ground fog level
fog_level 0
fog_color 0.25 0.3 0.4

# Optional toxic cloud
toxic_cloud enabled
toxic_delay 10.0
toxic_duration 90.0
toxic_safe_zone 0.20
toxic_damage 1
toxic_interval 5.0
toxic_slowdown 0.70
toxic_color 0.2 0.8 0.3
toxic_center 12 7

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

Define reusable objects, place them in grid cells. **Tags can be placed multiple times** - each placement creates a new instance of that object:

```
tag P spawn angle=0.785 team=0
tag E enemy angle=3.14 type=hunter
tag W powerup type=machine_gun respawn=15
tag B barrier tile=cobble health=20

<grid>
2# 0.|P 0.|E 0.|W 0.|B 2#
2# 0. 0.|B 0. 0.|B 2#
</grid>
```

In this example, the single `B` barrier tag creates 3 separate barriers at different positions.

**Spawn params:** `angle`, `team`, `team_spawn`
**Enemy params:** `angle`, `type` (sentry, skirmisher, hunter, sniper)
**Powerup params:** `type` (machine_gun, ricochet, barrier_placer), `respawn` (seconds, default 15)
**Barrier placer params:** `barrier` (tag name), `barrier_count` (max active barriers), `barrier_lifetime` (seconds until auto-destroy, 0 = infinite)
**Barrier params:** `tile` (tile name for texture), `health` (default 20)

## Water Level

```
water_level -1
water_color 0.2 0.4 0.6
wave_strength 1.0
wind_direction 3.93
wind_strength 1.5
```

Water surface renders slightly below ground. Tiles at or below water_level are submerged.

- **water_level**: Height threshold for water coverage (integer)
- **water_color**: RGB color (0.0-1.0 each) for the water surface
- **wave_strength**: Multiplier for wave amplitude (default 1.0, use higher for stormy seas)
- **wind_direction**: Direction wind blows in radians (0 = +X/east, π/2 = +Z/south, π = -X/west, 3π/2 = -Z/north). Default 0.0. Controls caustic texture drift direction.
- **wind_strength**: Speed multiplier for wind-driven water movement (default 1.0)

## Ground Fog

```
fog_level 0
fog_color 0.25 0.3 0.4
```

Ground fog creates an atmospheric layer that obscures lower parts of the map:

- **fog_level**: Height threshold for fog coverage (integer). Tiles at or below this level are covered.
- **fog_color**: RGB color (0.0-1.0 each) that tints the fog layer.

The fog plane renders at half a block above the specified level, creating a smooth transition. Use fog for:
- Swamp/marsh atmosphere (low-lying green-gray fog)
- Night ambiance (dark blue-gray fog at ground level)
- Industrial/urban maps (brownish smog)

Example combinations:
```
# Swamp fog
fog_level -1
fog_color 0.2 0.3 0.2

# Night mist
fog_level 0
fog_color 0.15 0.15 0.25

# Industrial smog
fog_level 0
fog_color 0.35 0.3 0.25
```

## Toxic Cloud

```
toxic_cloud enabled
toxic_delay 10.0
toxic_duration 90.0
toxic_safe_zone 0.20
toxic_damage 1
toxic_interval 5.0
toxic_slowdown 0.70
toxic_color 0.2 0.8 0.3
toxic_center 12 7
```

Toxic clouds shrink toward a rounded safe zone over time and apply damage/slowdown
to tanks outside the safe area:

- **toxic_cloud**: `enabled` or `disabled` (default: disabled)
- **toxic_delay**: Seconds before closing starts (default 10)
- **toxic_duration**: Seconds to reach final safe zone (default 90)
- **toxic_safe_zone**: Final safe zone radius as a ratio of map size (default 0.20)
- **toxic_damage**: Damage per tick (default 1)
- **toxic_interval**: Seconds between damage ticks (default 5)
- **toxic_slowdown**: Speed multiplier inside cloud (0.70 = 30% slower)
- **toxic_color**: RGB color for rendering (0.0-1.0)
- **toxic_center**: Center of safe zone in tile coordinates (defaults to map center)

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

tag P spawn angle=0.785 team=0
tag E enemy angle=3.14 type=sentry
tag B barrier tile=cobble health=20

<grid>
2# 2# 2# 2# 2# 2#
2# 0.|P 0.|B 0: 0. 2#
2# 0. 0.|E 0.|B 0.|P 2#
2# 2# 2# 2# 2# 2#
</grid>

ambient_color 0.12 0.12 0.15
ambient_darkness 0.85
```

## Map Tool

Use `tools/map_tool.py` for programmatic map creation and editing. This is preferred over manual text editing.

### Commands

```bash
./tools/map_tool.py info <map_file>      # Show map info
./tools/map_tool.py validate <map_file>  # Validate and re-serialize
./tools/map_tool.py --help               # Full API documentation
```

### Python API

```python
from tools.map_tool import (
    load_map, save_map, Map, Cell, TagDef, TileDef,
    BackgroundGradient, InlineSpawn, InlineEnemy
)

# Load and modify existing map
m = load_map("assets/maps/arena.map")
m.fill_rect(2, 2, 5, 5, 0, ".")      # Clear area to ground
m.place_tag(3, 3, "P1")              # Add spawn tag at position
save_map(m, "assets/maps/arena.map")

# Create new map from scratch
m = Map()
m.name = "My Arena"
m.tile_size = 2.0
m.width = 10
m.height = 10
m.cells = [[Cell(0, ".") for _ in range(10)] for _ in range(10)]

# Add walls
for x in range(10):
    m.set_cell(x, 0, Cell(2, "#"))
    m.set_cell(x, 9, Cell(2, "#"))

# Define tiles and tags
m.tile_defs = [TileDef(".", "ground"), TileDef("#", "wall")]
m.tag_defs = [TagDef("P1", "spawn", {"angle": "0", "team": "0"})]
m.place_tag(5, 5, "P1")

# Set lighting
m.sun_direction = (-0.5, -1.0, -0.3)
m.sun_color = (1.0, 0.95, 0.8)
m.ambient_color = (0.3, 0.35, 0.4)

# Set background gradient
m.background_gradient = BackgroundGradient("vertical", (0.5, 0.6, 0.8), (0.8, 0.7, 0.5))

# Set water with wind
m.water_level = -1
m.water_color = (0.2, 0.4, 0.6)
m.wind_direction = 2.36
m.wind_strength = 3.0

# Or use inline spawns (alternative to tags)
m.inline_spawns.append(InlineSpawn(5, 5, 0.0, team=0))
m.inline_enemies.append(InlineEnemy(8, 8, 3.14, "hunter"))

save_map(m, "assets/maps/my_arena.map")
```

### Classes

| Class | Description |
|-------|-------------|
| `Cell(height, tile, tags=[])` | Single map cell |
| `TileDef(symbol, name)` | Tile symbol mapping |
| `TagDef(name, type, params={})` | Tag definition (spawn/enemy/powerup/barrier) |
| `BackgroundGradient(direction, top_color, bottom_color)` | Sky gradient |
| `InlineSpawn(x, y, angle, team=0, team_spawn=0)` | Inline spawn point |
| `InlineEnemy(x, y, angle, enemy_type)` | Inline enemy |

### Map Methods

| Method | Description |
|--------|-------------|
| `get_cell(x, y)` | Get cell at position |
| `set_cell(x, y, cell)` | Set cell at position |
| `fill(height, tile)` | Fill entire map |
| `fill_rect(x1, y1, x2, y2, h, t)` | Fill rectangle |
| `place_tag(x, y, tag_name)` | Add tag to cell |
| `resize(w, h, fill_h, tile)` | Resize map |
| `pad(l, t, r, b, h, tile)` | Add padding |
| `set_toxic_cloud(...)` | Configure toxic cloud |

## Implementation

- Parser: `src/game/pz_map.c`
- Header: `src/game/pz_map.h`
- Renderer: `src/game/pz_map_render.c`
- Tool: `tools/map_tool.py`
- Maps: `assets/maps/*.map`
