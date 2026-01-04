#!/usr/bin/env python3
"""Generate the Deep Sea map with two islands."""

import sys
sys.path.insert(0, '.')
from map_tool import Map, Cell, TileDef, TagDef, save_map

# Create a larger map (32x20) - mostly deep water with two islands
m = Map()
m.name = "Deep Sea"
m.tile_size = 2.0
m.music = "march"

# Tile definitions
m.tile_defs = [
    TileDef(".", "wood_pine_light"),  # Island ground
    TileDef("#", "wood_walnut"),       # Walls/structures
    TileDef(":", "mud_wet"),           # Shallow water/beach
]

# Tag definitions - players on one island, enemies on the other
m.tag_defs = [
    TagDef("P1", "spawn", {"angle": "0.0", "team": "0"}),
    TagDef("P2", "spawn", {"angle": "-1.57", "team": "0"}),
    TagDef("E1", "enemy", {"angle": "3.14", "type": "skirmisher"}),
    TagDef("E2", "enemy", {"angle": "-1.57", "type": "skirmisher"}),
    TagDef("E3", "enemy", {"angle": "1.57", "type": "sniper"}),
    TagDef("W1", "powerup", {"type": "ricochet", "respawn": "20"}),
]

# Map size
WIDTH = 32
HEIGHT = 20

# Initialize with deep water
m.width = WIDTH
m.height = HEIGHT
m.cells = [[Cell(-2, ":") for _ in range(WIDTH)] for _ in range(HEIGHT)]

def make_island(cx, cy, ground_cells, wall_positions=None, beach_border=True):
    """Create an island at center (cx, cy) with given ground cell positions (relative)."""
    # First, add beach border at ground level (height 0) if requested
    if beach_border:
        for gx, gy in ground_cells:
            for dx in [-1, 0, 1]:
                for dy in [-1, 0, 1]:
                    x, y = cx + gx + dx, cy + gy + dy
                    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
                        if m.cells[y][x].height < 0:
                            m.cells[y][x] = Cell(0, ":")  # Beach at height 0
    
    # Then add main ground (overwrites beach where needed)
    for gx, gy in ground_cells:
        x, y = cx + gx, cy + gy
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            m.cells[y][x] = Cell(0, ".")
    
    # Add walls
    if wall_positions:
        for wx, wy in wall_positions:
            x, y = cx + wx, cy + wy
            if 0 <= x < WIDTH and 0 <= y < HEIGHT:
                m.cells[y][x] = Cell(2, "#")

# Player island (upper-left area)
# Shape: roughly 6x5 irregular island
player_island_ground = [
    # Core area
    (0, 0), (1, 0), (2, 0), (3, 0),
    (0, 1), (1, 1), (2, 1), (3, 1), (4, 1),
    (-1, 2), (0, 2), (1, 2), (2, 2), (3, 2), (4, 2),
    (0, 3), (1, 3), (2, 3), (3, 3), (4, 3),
    (1, 4), (2, 4), (3, 4),
]
player_island_walls = [
    (1, 1), (2, 2),  # Some cover
]
make_island(5, 4, player_island_ground, player_island_walls)

# Enemy island (lower-right area)  
# Shape: roughly 7x5 irregular island
enemy_island_ground = [
    (1, 0), (2, 0), (3, 0), (4, 0),
    (0, 1), (1, 1), (2, 1), (3, 1), (4, 1), (5, 1),
    (0, 2), (1, 2), (2, 2), (3, 2), (4, 2), (5, 2), (6, 2),
    (0, 3), (1, 3), (2, 3), (3, 3), (4, 3), (5, 3),
    (1, 4), (2, 4), (3, 4), (4, 4),
]
enemy_island_walls = [
    (2, 1), (3, 1),  # Some cover
    (4, 3),
]
make_island(21, 11, enemy_island_ground, enemy_island_walls)

# Place spawns on player island
m.place_tag(6, 5, "P1")   # Upper area
m.place_tag(8, 7, "P2")   # Lower area

# Place enemies on enemy island
m.place_tag(22, 12, "E1")
m.place_tag(25, 14, "E2")
m.place_tag(27, 13, "E3")

# Place powerup on enemy island
m.place_tag(24, 13, "W1")

# Add walls around the perimeter
# Left wall (height 30)
for y in range(HEIGHT):
    m.cells[y][0] = Cell(30, "#")

# Right wall (height 30)
for y in range(HEIGHT):
    m.cells[y][WIDTH - 1] = Cell(30, "#")

# Top wall (height 30)
for x in range(WIDTH):
    m.cells[0][x] = Cell(30, "#")

# Bottom wall (height 3)
for x in range(WIDTH):
    m.cells[HEIGHT - 1][x] = Cell(3, "#")

# Lighting - underwater/dawn theme
m.sun_direction = (0.5, -0.7, 0.4)
m.sun_color = (0.9, 0.95, 1.0)
m.ambient_color = (0.3, 0.4, 0.5)
m.ambient_darkness = 0.2

# Background - deep blue ocean gradient
m.background_gradient = "vertical 0.1 0.2 0.4 0.2 0.4 0.6"

# Water - deep teal
m.water_level = -1
m.water_color = (0.1, 0.35, 0.45)

# Save
save_map(m, "assets/maps/deep_sea.map")
print(f"Created deep_sea.map: {WIDTH}x{HEIGHT}")
print(f"Player island at (5, 4)")
print(f"Enemy island at (21, 11)")
