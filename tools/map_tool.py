#!/usr/bin/env python3
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
class Map:
    name: str = "Unnamed"
    tile_size: float = 2.0
    music: Optional[str] = None
    width: int = 0
    height: int = 0
    cells: list[list[Cell]] = field(default_factory=list)
    tile_defs: list[TileDef] = field(default_factory=list)
    tag_defs: list[TagDef] = field(default_factory=list)
    
    # Lighting
    sun_direction: Optional[tuple[float, float, float]] = None
    sun_color: Optional[tuple[float, float, float]] = None
    ambient_color: Optional[tuple[float, float, float]] = None
    ambient_darkness: Optional[float] = None
    
    # Background
    background_gradient: Optional[str] = None
    
    # Water
    water_level: Optional[int] = None
    water_color: Optional[tuple[float, float, float]] = None
    
    # Fog
    fog_level: Optional[int] = None
    fog_color: Optional[tuple[float, float, float]] = None
    
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
            m.background_gradient = rest
        elif cmd == 'water_level':
            m.water_level = int(rest)
        elif cmd == 'water_color':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 3:
                m.water_color = tuple(vals)
        elif cmd == 'fog_level':
            m.fog_level = int(rest)
        elif cmd == 'fog_color':
            vals = [float(x) for x in rest.split()]
            if len(vals) == 3:
                m.fog_color = tuple(vals)
    
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
    
    # Fog
    if m.fog_level is not None:
        lines.append(f"fog_level {m.fog_level}")
    if m.fog_color:
        lines.append(f"fog_color {m.fog_color[0]} {m.fog_color[1]} {m.fog_color[2]}")
    
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
    
    # Count spawns and enemies
    spawns = []
    enemies = []
    for tag in m.tag_defs:
        if tag.type == 'spawn':
            spawns.append(tag.name)
        elif tag.type == 'enemy':
            enemies.append(tag.name)
    print(f"Spawns: {spawns}")
    print(f"Enemies: {enemies}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: map_tool.py <command> [args]")
        print("Commands:")
        print("  info <map_file>       - Show map info")
        print("  validate <map_file>   - Validate and re-save")
        sys.exit(1)
    
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
        sys.exit(1)
