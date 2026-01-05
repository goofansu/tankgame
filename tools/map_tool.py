#!/usr/bin/env -S uv run --script
"""
Tank Game Map Tool - Parse, manipulate, and generate map files.
"""

import sys
import re
from dataclasses import dataclass, field
from typing import Optional

@dataclass
class Cell:
    height: int
    tile: str
    tags: list[str] = field(default_factory=list)
    
    def __str__(self):
        base = f"{self.height}{self.tile}"
        if self.tags:
            base += "|" + ",".join(self.tags)
        return base

@dataclass
class TagDef:
    name: str
    type: str  # spawn, enemy, powerup, barrier
    params: dict[str, str] = field(default_factory=dict)
    
    def __str__(self):
        params_str = " ".join(f"{k}={v}" for k, v in self.params.items())
        if params_str:
            return f"tag {self.name} {self.type} {params_str}"
        return f"tag {self.name} {self.type}"

@dataclass
class TileDef:
    symbol: str
    name: str
    
    def __str__(self):
        return f"tile {self.symbol} {self.name}"

@dataclass
class BackgroundGradient:
    direction: str  # "vertical" or "horizontal"
    top_color: tuple[float, float, float]
    bottom_color: tuple[float, float, float]
    
    def __str__(self):
        return (f"{self.direction} "
                f"{self.top_color[0]} {self.top_color[1]} {self.top_color[2]} "
                f"{self.bottom_color[0]} {self.bottom_color[1]} {self.bottom_color[2]}")

@dataclass
class InlineSpawn:
    """Inline spawn definition (spawn X Y ANGLE [TEAM TEAM_SPAWN])"""
    x: int
    y: int
    angle: float
    team: int = 0
    team_spawn: int = 0
    
    def __str__(self):
        return f"spawn {self.x} {self.y} {self.angle} {self.team} {self.team_spawn}"

@dataclass
class InlineEnemy:
    """Inline enemy definition (enemy X Y ANGLE TYPE)"""
    x: int
    y: int
    angle: float
    enemy_type: str
    
    def __str__(self):
        return f"enemy {self.x} {self.y} {self.angle} {self.enemy_type}"

@dataclass 
class Map:
    name: str = "Unnamed"
    tile_size: float = 2.0
    music: Optional[str] = None
    width: int = 0
    height: int = 0
    cells: list[list[Cell]] = field(default_factory=list)
    tile_defs: list[TileDef] = field(default_factory=list)
    tag_defs: list[TagDef] = field(default_factory=list)
    
    # Inline spawns and enemies (alternative to tags)
    inline_spawns: list[InlineSpawn] = field(default_factory=list)
    inline_enemies: list[InlineEnemy] = field(default_factory=list)
    
    # Lighting
    sun_direction: Optional[tuple[float, float, float]] = None
    sun_color: Optional[tuple[float, float, float]] = None
    ambient_color: Optional[tuple[float, float, float]] = None
    ambient_darkness: Optional[float] = None
    
    # Background
    background_gradient: Optional[BackgroundGradient] = None
    
    # Water
    water_level: Optional[int] = None
    water_color: Optional[tuple[float, float, float]] = None
    wave_strength: Optional[float] = None
    wind_direction: Optional[float] = None
    wind_strength: Optional[float] = None
    
    # Fog
    fog_level: Optional[int] = None
    fog_color: Optional[tuple[float, float, float]] = None

    # Toxic cloud
    toxic_enabled: Optional[bool] = None
    toxic_delay: Optional[float] = None
    toxic_duration: Optional[float] = None
    toxic_safe_zone: Optional[float] = None
    toxic_damage: Optional[int] = None
    toxic_interval: Optional[float] = None
    toxic_slowdown: Optional[float] = None
    toxic_color: Optional[tuple[float, float, float]] = None
    toxic_center: Optional[tuple[float, float]] = None
    
    def get_cell(self, x: int, y: int) -> Optional[Cell]:
        if 0 <= y < self.height and 0 <= x < self.width:
            return self.cells[y][x]
        return None
    
    def set_cell(self, x: int, y: int, cell: Cell):
        if 0 <= y < self.height and 0 <= x < self.width:
            self.cells[y][x] = cell
    
    def fill(self, height: int, tile: str):
        """Fill entire map with given cell type."""
        for y in range(self.height):
            for x in range(self.width):
                self.cells[y][x] = Cell(height, tile)
    
    def fill_rect(self, x1: int, y1: int, x2: int, y2: int, height: int, tile: str):
        """Fill a rectangle with given cell type."""
        for y in range(max(0, y1), min(self.height, y2 + 1)):
            for x in range(max(0, x1), min(self.width, x2 + 1)):
                self.cells[y][x] = Cell(height, tile)
    
    def place_tag(self, x: int, y: int, tag_name: str):
        """Add a tag to a cell."""
        cell = self.get_cell(x, y)
        if cell and tag_name not in cell.tags:
            cell.tags.append(tag_name)
    
    def resize(self, new_width: int, new_height: int, fill_height: int = 0, fill_tile: str = "."):
        """Resize the map, padding with fill cells if larger."""
        new_cells = []
        for y in range(new_height):
            row = []
            for x in range(new_width):
                if y < self.height and x < self.width:
                    row.append(self.cells[y][x])
                else:
                    row.append(Cell(fill_height, fill_tile))
            new_cells.append(row)
        self.cells = new_cells
        self.width = new_width
        self.height = new_height
    
    def pad(self, left: int, top: int, right: int, bottom: int, 
            fill_height: int = 0, fill_tile: str = "."):
        """Add padding around the map."""
        new_width = self.width + left + right
        new_height = self.height + top + bottom
        new_cells = []
        
        for y in range(new_height):
            row = []
            for x in range(new_width):
                src_x = x - left
                src_y = y - top
                if 0 <= src_x < self.width and 0 <= src_y < self.height:
                    row.append(self.cells[src_y][src_x])
                else:
                    row.append(Cell(fill_height, fill_tile))
            new_cells.append(row)
        
        self.cells = new_cells
        self.width = new_width
        self.height = new_height
    
    def get_tag_def(self, name: str) -> Optional[TagDef]:
        for tag in self.tag_defs:
            if tag.name == name:
                return tag
        return None

    def get_tile_for_symbol(self, symbol: str) -> Optional[str]:
        for td in self.tile_defs:
            if td.symbol == symbol:
                return td.name
        return None

    def set_toxic_cloud(self, enabled: bool = True, delay: float = 10.0,
                        duration: float = 90.0, safe_zone: float = 0.20,
                        damage: int = 1, interval: float = 5.0,
                        slowdown: float = 0.70,
                        color: tuple[float, float, float] = (0.2, 0.8, 0.3),
                        center: Optional[tuple[float, float]] = None):
        self.toxic_enabled = enabled
        self.toxic_delay = delay
        self.toxic_duration = duration
        self.toxic_safe_zone = safe_zone
        self.toxic_damage = damage
        self.toxic_interval = interval
        self.toxic_slowdown = slowdown
        self.toxic_color = color
        self.toxic_center = center

    def get_toxic_cloud(self):
        return {
            "enabled": self.toxic_enabled,
            "delay": self.toxic_delay,
            "duration": self.toxic_duration,
            "safe_zone": self.toxic_safe_zone,
            "damage": self.toxic_damage,
            "interval": self.toxic_interval,
            "slowdown": self.toxic_slowdown,
            "color": self.toxic_color,
            "center": self.toxic_center,
        }


def parse_cell(text: str) -> Cell:
    """Parse a cell like '2#' or '0.|P1,E1' or '-1:'"""
    # Match: optional minus, digits, single char tile, optional |tags
    match = re.match(r'^(-?\d+)(\S)(?:\|(.+))?$', text)
    if not match:
        raise ValueError(f"Invalid cell format: {text}")
    
    height = int(match.group(1))
    tile = match.group(2)
    tags = match.group(3).split(',') if match.group(3) else []
    return Cell(height, tile, tags)


def parse_map(content: str) -> Map:
    """Parse a map file content into a Map object."""
    m = Map()
    lines = content.split('\n')
    
    in_grid = False
    grid_lines = []
    
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1
        
        # Skip empty lines and comments outside grid
        if not in_grid:
            if not line or line.startswith('#'):
                continue
        
        # Grid section
        if line == '<grid>':
            in_grid = True
            continue
        elif line == '</grid>':
            in_grid = False
            continue
        
        if in_grid:
            if line:  # Skip empty lines in grid
                grid_lines.append(line)
            continue
        
        # Parse directives
        parts = line.split(None, 1)
        if not parts:
            continue
            
        cmd = parts[0]
        rest = parts[1] if len(parts) > 1 else ""
        
        if cmd == 'name':
            m.name = rest
        elif cmd == 'tile_size':
            m.tile_size = float(rest)
        elif cmd == 'music':
            m.music = rest
        elif cmd == 'tile':
            # tile . ground
            tp = rest.split()
            if len(tp) >= 2:
                m.tile_defs.append(TileDef(tp[0], tp[1]))
        elif cmd == 'tag':
            # tag P1 spawn angle=0.785 team=0
            tp = rest.split()
            if len(tp) >= 2:
                tag_name = tp[0]
                tag_type = tp[1]
                params = {}
                for p in tp[2:]:
                    if '=' in p:
                        k, v = p.split('=', 1)
                        params[k] = v
                m.tag_defs.append(TagDef(tag_name, tag_type, params))
        elif cmd == 'sun_direction':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 3:
                m.sun_direction = tuple(vals)
        elif cmd == 'sun_color':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 3:
                m.sun_color = tuple(vals)
        elif cmd == 'ambient_color':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 3:
                m.ambient_color = tuple(vals)
        elif cmd == 'ambient_darkness':
            m.ambient_darkness = float(rest)
        elif cmd == 'background_gradient':
            # Parse: vertical/horizontal R G B R G B
            parts = rest.split()
            if len(parts) >= 7:
                direction = parts[0]
                top_color = (float(parts[1]), float(parts[2]), float(parts[3]))
                bottom_color = (float(parts[4]), float(parts[5]), float(parts[6]))
                m.background_gradient = BackgroundGradient(direction, top_color, bottom_color)
            elif len(parts) == 1:
                # Simple string fallback
                m.background_gradient = BackgroundGradient(parts[0], (0.5, 0.5, 0.5), (0.5, 0.5, 0.5))
        elif cmd == 'water_level':
            m.water_level = int(rest)
        elif cmd == 'water_color':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 3:
                m.water_color = tuple(vals)
        elif cmd == 'wave_strength':
            m.wave_strength = float(rest)
        elif cmd == 'fog_level':
            m.fog_level = int(rest)
        elif cmd == 'fog_color':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 3:
                m.fog_color = tuple(vals)
        elif cmd == 'toxic_cloud':
            if rest == 'enabled':
                m.toxic_enabled = True
            elif rest == 'disabled':
                m.toxic_enabled = False
        elif cmd == 'toxic_delay':
            m.toxic_delay = float(rest)
        elif cmd == 'toxic_duration':
            m.toxic_duration = float(rest)
        elif cmd == 'toxic_safe_zone':
            m.toxic_safe_zone = float(rest)
        elif cmd == 'toxic_damage':
            m.toxic_damage = int(rest)
        elif cmd == 'toxic_interval':
            m.toxic_interval = float(rest)
        elif cmd == 'toxic_slowdown':
            m.toxic_slowdown = float(rest)
        elif cmd == 'toxic_color':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 3:
                m.toxic_color = tuple(vals)
        elif cmd == 'toxic_center':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 2:
                m.toxic_center = (vals[0], vals[1])
        elif cmd == 'wind_direction':
            m.wind_direction = float(rest)
        elif cmd == 'wind_strength':
            m.wind_strength = float(rest)
        elif cmd == 'spawn':
            # Inline spawn: spawn X Y ANGLE [TEAM [TEAM_SPAWN]]
            parts = rest.split()
            if len(parts) >= 3:
                x = int(parts[0])
                y = int(parts[1])
                angle = float(parts[2])
                team = int(parts[3]) if len(parts) > 3 else 0
                team_spawn = int(parts[4]) if len(parts) > 4 else 0
                m.inline_spawns.append(InlineSpawn(x, y, angle, team, team_spawn))
        elif cmd == 'enemy':
            # Inline enemy: enemy X Y ANGLE TYPE
            parts = rest.split()
            if len(parts) >= 4:
                x = int(parts[0])
                y = int(parts[1])
                angle = float(parts[2])
                enemy_type = parts[3]
                m.inline_enemies.append(InlineEnemy(x, y, angle, enemy_type))
    
    # Parse grid
    if grid_lines:
        for row_str in grid_lines:
            cell_strs = row_str.split()
            row = [parse_cell(c) for c in cell_strs]
            m.cells.append(row)
        m.height = len(m.cells)
        m.width = len(m.cells[0]) if m.cells else 0
    
    return m


def serialize_map(m: Map) -> str:
    """Serialize a Map object to file content."""
    lines = []
    
    lines.append(f"name {m.name}")
    lines.append(f"tile_size {m.tile_size}")
    if m.music:
        lines.append(f"music {m.music}")
    lines.append("")
    
    # Tile definitions
    if m.tile_defs:
        for td in m.tile_defs:
            lines.append(str(td))
        lines.append("")
    
    # Tag definitions
    if m.tag_defs:
        for tag in m.tag_defs:
            lines.append(str(tag))
        lines.append("")
    
    # Grid
    lines.append("<grid>")
    for row in m.cells:
        line = " ".join(str(c) for c in row)
        lines.append(line)
    lines.append("</grid>")
    lines.append("")
    
    # Lighting
    if m.sun_direction:
        lines.append(f"sun_direction {m.sun_direction[0]} {m.sun_direction[1]} {m.sun_direction[2]}")
    if m.sun_color:
        lines.append(f"sun_color {m.sun_color[0]} {m.sun_color[1]} {m.sun_color[2]}")
    if m.ambient_color:
        lines.append(f"ambient_color {m.ambient_color[0]} {m.ambient_color[1]} {m.ambient_color[2]}")
    if m.ambient_darkness is not None:
        lines.append(f"ambient_darkness {m.ambient_darkness}")
    
    # Background
    if m.background_gradient:
        lines.append(f"background_gradient {m.background_gradient}")
    
    # Water
    if m.water_level is not None:
        lines.append(f"water_level {m.water_level}")
    if m.water_color:
        lines.append(f"water_color {m.water_color[0]} {m.water_color[1]} {m.water_color[2]}")
    if m.wave_strength is not None and m.wave_strength != 1.0:
        lines.append(f"wave_strength {m.wave_strength}")
    if m.wind_direction is not None:
        lines.append(f"wind_direction {m.wind_direction}")
    if m.wind_strength is not None:
        lines.append(f"wind_strength {m.wind_strength}")
    
    # Fog
    if m.fog_level is not None:
        lines.append(f"fog_level {m.fog_level}")
    if m.fog_color:
        lines.append(f"fog_color {m.fog_color[0]} {m.fog_color[1]} {m.fog_color[2]}")

    # Toxic cloud
    if m.toxic_enabled is not None:
        state = "enabled" if m.toxic_enabled else "disabled"
        lines.append(f"toxic_cloud {state}")
    if m.toxic_delay is not None:
        lines.append(f"toxic_delay {m.toxic_delay}")
    if m.toxic_duration is not None:
        lines.append(f"toxic_duration {m.toxic_duration}")
    if m.toxic_safe_zone is not None:
        lines.append(f"toxic_safe_zone {m.toxic_safe_zone}")
    if m.toxic_damage is not None:
        lines.append(f"toxic_damage {m.toxic_damage}")
    if m.toxic_interval is not None:
        lines.append(f"toxic_interval {m.toxic_interval}")
    if m.toxic_slowdown is not None:
        lines.append(f"toxic_slowdown {m.toxic_slowdown}")
    if m.toxic_color:
        lines.append(f"toxic_color {m.toxic_color[0]} {m.toxic_color[1]} {m.toxic_color[2]}")
    if m.toxic_center:
        lines.append(f"toxic_center {m.toxic_center[0]} {m.toxic_center[1]}")
    
    # Inline spawns and enemies
    for spawn in m.inline_spawns:
        lines.append(str(spawn))
    for enemy in m.inline_enemies:
        lines.append(str(enemy))
    
    return "\n".join(lines) + "\n"


def load_map(path: str) -> Map:
    """Load a map from file."""
    with open(path, 'r') as f:
        return parse_map(f.read())


def save_map(m: Map, path: str):
    """Save a map to file."""
    with open(path, 'w') as f:
        f.write(serialize_map(m))


def print_map_info(m: Map):
    """Print map summary."""
    print(f"Name: {m.name}")
    print(f"Size: {m.width}x{m.height}")
    print(f"Tile size: {m.tile_size}")
    if m.music:
        print(f"Music: {m.music}")
    print(f"Tile defs: {len(m.tile_defs)}")
    print(f"Tag defs: {len(m.tag_defs)}")
    if m.water_level is not None:
        print(f"Water level: {m.water_level}")
    if m.toxic_enabled is not None:
        state = "enabled" if m.toxic_enabled else "disabled"
        print(f"Toxic cloud: {state}")
    
    # Count spawns and enemies
    spawns = []
    enemies = []
    for tag in m.tag_defs:
        if tag.type == 'spawn':
            spawns.append(tag.name)
        elif tag.type == 'enemy':
            enemies.append(tag.name)
    print(f"Spawns (tags): {spawns}")
    print(f"Enemies (tags): {enemies}")
    if m.inline_spawns:
        print(f"Inline spawns: {len(m.inline_spawns)}")
    if m.inline_enemies:
        print(f"Inline enemies: {len(m.inline_enemies)}")
    if m.wind_direction is not None:
        print(f"Wind direction: {m.wind_direction}")
    if m.wind_strength is not None:
        print(f"Wind strength: {m.wind_strength}")


def print_help():
    """Print comprehensive help message."""
    help_text = """Tank Game Map Tool - Parse, manipulate, and generate map files.

USAGE:
    ./tools/map_tool.py <command> [args]
    ./tools/map_tool.py --help

COMMANDS:
    info <map_file>       Show map info (size, spawns, enemies, etc.)
    validate <map_file>   Validate map and print re-serialized output

PYTHON API:
    The map tool is designed to be imported and used as a Python library
    for programmatic map manipulation. This is the preferred way to edit
    maps rather than manual text editing.

CLASSES:
    Cell(height: int, tile: str, tags: list[str])
        Represents a single map cell with height, terrain tile, and tags.
    
    TagDef(name: str, type: str, params: dict)
        Defines a tag (spawn, enemy, powerup, barrier) with parameters.
    
    TileDef(symbol: str, name: str)
        Maps a symbol (like '#') to a tile name (like 'wall').
    
    BackgroundGradient(direction: str, top_color: tuple, bottom_color: tuple)
        Background gradient with direction ("vertical"/"horizontal") and two RGB colors.
    
    InlineSpawn(x: int, y: int, angle: float, team: int, team_spawn: int)
        Inline spawn point (alternative to using tags).
    
    InlineEnemy(x: int, y: int, angle: float, enemy_type: str)
        Inline enemy definition (alternative to using tags).
    
    Map
        The main map container with cells, definitions, and properties.
        Properties: name, tile_size, width, height, cells, tile_defs, tag_defs
        Inline objects: inline_spawns, inline_enemies
        Lighting: sun_direction, sun_color, ambient_color, ambient_darkness
        Background: background_gradient (BackgroundGradient object)
        Water: water_level, water_color, wave_strength, wind_direction, wind_strength
        Fog: fog_level, fog_color
        Toxic: toxic_enabled, toxic_delay, toxic_duration, toxic_safe_zone, toxic_damage, toxic_interval, toxic_slowdown, toxic_color, toxic_center

MAP METHODS:
    get_cell(x, y) -> Cell           Get cell at position (or None)
    set_cell(x, y, cell)             Set cell at position
    fill(height, tile)               Fill entire map with cell type
    fill_rect(x1, y1, x2, y2, h, t)  Fill rectangle with cell type
    place_tag(x, y, tag_name)        Add a tag to a cell
    resize(w, h, fill_height, tile)  Resize map, padding if needed
    pad(l, t, r, b, height, tile)    Add padding around map
    get_tag_def(name) -> TagDef      Get tag definition by name
    get_tile_for_symbol(sym) -> str  Get tile name for symbol
    set_toxic_cloud(...)             Configure toxic cloud settings
    get_toxic_cloud() -> dict        Get toxic cloud settings

FUNCTIONS:
    load_map(path) -> Map            Load a map from file
    save_map(map, path)              Save a map to file
    parse_map(content) -> Map        Parse map from string content
    serialize_map(map) -> str        Serialize map to string

EXAMPLE USAGE:
    from tools.map_tool import (load_map, save_map, Map, Cell, TagDef, TileDef,
                                 BackgroundGradient, InlineSpawn, InlineEnemy)
    
    # Load and modify existing map
    m = load_map("assets/maps/arena.map")
    m.fill_rect(2, 2, 5, 5, 0, ".")      # Clear a 4x4 area to ground
    m.place_tag(3, 3, "P1")               # Add spawn point at (3,3)
    save_map(m, "assets/maps/arena.map")
    
    # Create a new map from scratch
    m = Map()
    m.name = "Test Arena"
    m.tile_size = 2.0
    m.width = 10
    m.height = 10
    m.cells = [[Cell(0, ".") for _ in range(10)] for _ in range(10)]
    
    # Add wall border
    for x in range(10):
        m.set_cell(x, 0, Cell(2, "#"))
        m.set_cell(x, 9, Cell(2, "#"))
    for y in range(10):
        m.set_cell(0, y, Cell(2, "#"))
        m.set_cell(9, y, Cell(2, "#"))
    
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
    m.wind_direction = 2.36  # radians
    m.wind_strength = 3.0
    
    # Use inline spawns (alternative to tags)
    m.inline_spawns.append(InlineSpawn(5, 5, 0.0, team=0))
    m.inline_enemies.append(InlineEnemy(8, 8, 3.14, "hunter"))
    
    save_map(m, "assets/maps/test.map")

CELL FORMAT:
    Cells in the grid are formatted as: <height><tile>[|<tags>]
    Examples:
        0.        Ground at height 0
        2#        Wall at height 2
        -1:       Water at height -1 (using ':' tile)
        0.|P1     Ground with spawn tag P1
        1.|E1,E2  Ground with multiple tags

MAP FILE FORMAT:
    See docs/map-format.md for the complete map file specification.
"""
    print(help_text)


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help", "help"):
        print_help()
        sys.exit(0)
    
    cmd = sys.argv[1]
    
    if cmd == "info" and len(sys.argv) >= 3:
        m = load_map(sys.argv[2])
        print_map_info(m)
    elif cmd == "validate" and len(sys.argv) >= 3:
        m = load_map(sys.argv[2])
        print_map_info(m)
        print("\nRe-serialized output:")
        print(serialize_map(m))
    else:
        print(f"Unknown command: {cmd}")
        print("Run with --help for usage information.")
        sys.exit(1)
