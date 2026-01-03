#!/usr/bin/env python3
"""Validate a .map file for consistency between terrain and heights."""

import sys

def parse_map(filename):
    with open(filename) as f:
        lines = f.readlines()
    
    terrain = []
    heights = []
    size = None
    in_terrain = False
    in_heights = False
    
    for line in lines:
        line = line.rstrip('\n')
        
        if line.startswith('size '):
            parts = line.split()
            size = (int(parts[1]), int(parts[2]))
        elif line == 'terrain':
            in_terrain = True
            in_heights = False
            continue
        elif line == 'heights':
            in_terrain = False
            in_heights = True
            continue
        elif line.startswith('#') and not in_terrain and not in_heights:
            continue
        elif line.startswith('spawn') or line.startswith('enemy') or line.startswith('ambient'):
            in_terrain = False
            in_heights = False
            continue
        
        if in_terrain and line:
            # Terrain rows start with # (wall) or other tile chars
            if len(line) >= size[0] if size else True:
                terrain.append(line)
        elif in_heights and line and line[0].isdigit():
            heights.append(line)
    
    return size, terrain, heights

def main():
    if len(sys.argv) < 2:
        print("Usage: validate_map.py <mapfile>")
        sys.exit(1)
    
    filename = sys.argv[1]
    size, terrain, heights = parse_map(filename)
    
    print(f"Map: {filename}")
    print(f"Declared size: {size[0]}x{size[1]}")
    print(f"Terrain rows: {len(terrain)}")
    print(f"Heights rows: {len(heights)}")
    print()
    
    errors = []
    
    # Check row counts
    if len(terrain) != size[1]:
        errors.append(f"Terrain has {len(terrain)} rows, expected {size[1]}")
    if len(heights) != size[1]:
        errors.append(f"Heights has {len(heights)} rows, expected {size[1]}")
    
    # Check each row
    print("Terrain row lengths:")
    for i, row in enumerate(terrain):
        marker = " " if len(row) == size[0] else " <-- WRONG"
        print(f"  Row {i:2d}: {len(row):2d} chars{marker}")
        if len(row) != size[0]:
            errors.append(f"Terrain row {i} has {len(row)} chars, expected {size[0]}")
    
    print()
    print("Heights row lengths:")
    for i, row in enumerate(heights):
        marker = " " if len(row) == size[0] else " <-- WRONG"
        print(f"  Row {i:2d}: {len(row):2d} chars{marker}")
        if len(row) != size[0]:
            errors.append(f"Heights row {i} has {len(row)} chars, expected {size[0]}")
    
    # Check terrain vs heights alignment
    print()
    print("Terrain vs Heights comparison:")
    wall_chars = {'#'}
    
    for row_idx in range(min(len(terrain), len(heights))):
        t_row = terrain[row_idx]
        h_row = heights[row_idx]
        
        for col_idx in range(min(len(t_row), len(h_row))):
            t_char = t_row[col_idx]
            h_char = h_row[col_idx]
            
            # Wall in terrain should be height 2
            if t_char == '#' and h_char != '2':
                errors.append(f"({col_idx},{row_idx}): Wall '#' but height={h_char} (expected 2)")
            
            # Floor tiles should be height 0
            if t_char in '.~:*' and h_char not in '0':
                errors.append(f"({col_idx},{row_idx}): Floor '{t_char}' but height={h_char} (expected 0)")
    
    print()
    if errors:
        print(f"ERRORS FOUND ({len(errors)}):")
        for e in errors:
            print(f"  - {e}")
        sys.exit(1)
    else:
        print("OK - No errors found")
        sys.exit(0)

if __name__ == '__main__':
    main()
