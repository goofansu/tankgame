#!/usr/bin/env python3
"""
Generate procedural textures for the tank game.
Style: Wooden toy box / tabletop game aesthetic.
"""

import numpy as np
from PIL import Image, ImageDraw, ImageFilter
import os
import random

# Ensure reproducible results
random.seed(42)
np.random.seed(42)

OUTPUT_DIR = "assets/textures"
TEXTURE_SIZE = 256


def ensure_dir():
    os.makedirs(OUTPUT_DIR, exist_ok=True)


def add_noise(img, amount=10):
    """Add subtle noise to an image."""
    arr = np.array(img, dtype=np.float32)
    noise = np.random.normal(0, amount, arr.shape)
    arr = np.clip(arr + noise, 0, 255).astype(np.uint8)
    return Image.fromarray(arr)


def wood_grain(width, height, base_color, grain_color, grain_density=0.15, vertical=True):
    """Generate a wood grain texture."""
    img = Image.new('RGB', (width, height), base_color)
    draw = ImageDraw.Draw(img)
    
    # Create wood grain lines
    num_grains = int(width * grain_density) if vertical else int(height * grain_density)
    
    for _ in range(num_grains):
        if vertical:
            x = random.randint(0, width - 1)
            # Wavy line
            points = []
            for y in range(0, height, 4):
                offset = int(np.sin(y * 0.05 + random.random() * 10) * 3)
                points.append((x + offset, y))
            if len(points) > 1:
                # Vary the grain color slightly
                r, g, b = grain_color
                variation = random.randint(-15, 15)
                color = (max(0, min(255, r + variation)),
                        max(0, min(255, g + variation)),
                        max(0, min(255, b + variation)))
                draw.line(points, fill=color, width=random.randint(1, 3))
        else:
            y = random.randint(0, height - 1)
            points = []
            for x in range(0, width, 4):
                offset = int(np.sin(x * 0.05 + random.random() * 10) * 3)
                points.append((x, y + offset))
            if len(points) > 1:
                r, g, b = grain_color
                variation = random.randint(-15, 15)
                color = (max(0, min(255, r + variation)),
                        max(0, min(255, g + variation)),
                        max(0, min(255, b + variation)))
                draw.line(points, fill=color, width=random.randint(1, 3))
    
    # Add some knots
    num_knots = random.randint(0, 3)
    for _ in range(num_knots):
        kx = random.randint(20, width - 20)
        ky = random.randint(20, height - 20)
        kr = random.randint(3, 8)
        # Draw concentric ovals for knot
        for i in range(kr, 0, -1):
            darkness = int((kr - i) / kr * 40)
            r, g, b = grain_color
            color = (max(0, r - darkness), max(0, g - darkness), max(0, b - darkness))
            draw.ellipse([kx - i * 2, ky - i, kx + i * 2, ky + i], fill=color)
    
    return img


def generate_ground_texture():
    """Light wood plank floor texture (pine/beech style)."""
    # Light warm wood colors
    base_color = (225, 195, 160)  # Light beige/tan
    grain_color = (195, 165, 130)  # Slightly darker grain
    
    img = wood_grain(TEXTURE_SIZE, TEXTURE_SIZE, base_color, grain_color, 
                     grain_density=0.12, vertical=True)
    
    # Add plank divisions (horizontal lines)
    draw = ImageDraw.Draw(img)
    plank_height = TEXTURE_SIZE // 4
    for y in range(plank_height, TEXTURE_SIZE, plank_height):
        # Dark line for gap
        draw.line([(0, y), (TEXTURE_SIZE, y)], fill=(160, 130, 100), width=2)
        # Slight highlight below
        draw.line([(0, y + 2), (TEXTURE_SIZE, y + 2)], fill=(235, 210, 180), width=1)
    
    # Add subtle noise
    img = add_noise(img, 5)
    
    # Slight blur for smoothness
    img = img.filter(ImageFilter.GaussianBlur(0.5))
    
    img.save(os.path.join(OUTPUT_DIR, "ground.png"))
    print("Generated: ground.png")


def generate_wall_texture():
    """Wooden toy block texture (light tan blocks)."""
    # Warm tan color like toy blocks
    base_color = (235, 215, 175)  # Light tan
    grain_color = (215, 190, 150)  # Subtle grain
    
    img = wood_grain(TEXTURE_SIZE, TEXTURE_SIZE, base_color, grain_color,
                     grain_density=0.08, vertical=True)
    
    # Add edge highlight on top/left
    draw = ImageDraw.Draw(img)
    highlight = (250, 235, 200)
    draw.line([(0, 0), (TEXTURE_SIZE, 0)], fill=highlight, width=3)
    draw.line([(0, 0), (0, TEXTURE_SIZE)], fill=highlight, width=3)
    
    # Add shadow on bottom/right
    shadow = (180, 155, 120)
    draw.line([(0, TEXTURE_SIZE-2), (TEXTURE_SIZE, TEXTURE_SIZE-2)], fill=shadow, width=3)
    draw.line([(TEXTURE_SIZE-2, 0), (TEXTURE_SIZE-2, TEXTURE_SIZE)], fill=shadow, width=3)
    
    img = add_noise(img, 4)
    img.save(os.path.join(OUTPUT_DIR, "wall.png"))
    print("Generated: wall.png")


def generate_wall_side_texture():
    """Side texture for walls - slightly darker with horizontal grain."""
    base_color = (210, 185, 145)  # Slightly darker tan
    grain_color = (185, 160, 120)
    
    img = wood_grain(TEXTURE_SIZE, TEXTURE_SIZE, base_color, grain_color,
                     grain_density=0.10, vertical=False)  # Horizontal grain
    
    img = add_noise(img, 4)
    img.save(os.path.join(OUTPUT_DIR, "wall_side.png"))
    print("Generated: wall_side.png")


def generate_border_texture():
    """Dark wood border texture (like picture frame)."""
    base_color = (120, 85, 60)  # Dark brown
    grain_color = (90, 60, 40)  # Darker grain
    
    img = wood_grain(TEXTURE_SIZE, TEXTURE_SIZE, base_color, grain_color,
                     grain_density=0.15, vertical=True)
    
    img = add_noise(img, 6)
    img = img.filter(ImageFilter.GaussianBlur(0.5))
    img.save(os.path.join(OUTPUT_DIR, "border.png"))
    print("Generated: border.png")


def generate_water_texture():
    """Dark hole/pit texture - like holes cut in wood."""
    # Dark interior
    img = Image.new('RGB', (TEXTURE_SIZE, TEXTURE_SIZE), (40, 35, 30))
    draw = ImageDraw.Draw(img)
    
    # Add some depth shading - lighter at edges
    for i in range(20):
        inset = i * 2
        alpha = int(255 * (i / 20) * 0.3)
        color = (40 + alpha // 4, 35 + alpha // 4, 30 + alpha // 4)
        draw.rectangle([inset, inset, TEXTURE_SIZE - inset, TEXTURE_SIZE - inset], 
                      outline=color, width=2)
    
    # Add subtle ripple pattern
    for y in range(0, TEXTURE_SIZE, 16):
        for x in range(0, TEXTURE_SIZE, 16):
            if random.random() < 0.3:
                brightness = random.randint(45, 55)
                draw.ellipse([x, y, x + 8, y + 4], fill=(brightness, brightness - 5, brightness - 10))
    
    img = add_noise(img, 8)
    img = img.filter(ImageFilter.GaussianBlur(1))
    img.save(os.path.join(OUTPUT_DIR, "water.png"))
    print("Generated: water.png")


def generate_mud_texture():
    """Cork/dark wood texture for mud areas."""
    # Cork-like brown
    base_color = (165, 130, 95)
    
    img = Image.new('RGB', (TEXTURE_SIZE, TEXTURE_SIZE), base_color)
    draw = ImageDraw.Draw(img)
    
    # Add cork-like speckles
    for _ in range(2000):
        x = random.randint(0, TEXTURE_SIZE - 1)
        y = random.randint(0, TEXTURE_SIZE - 1)
        size = random.randint(1, 4)
        darkness = random.randint(-30, 30)
        r, g, b = base_color
        color = (max(0, min(255, r + darkness)),
                max(0, min(255, g + darkness)),
                max(0, min(255, b + darkness)))
        draw.ellipse([x, y, x + size, y + size], fill=color)
    
    img = add_noise(img, 10)
    img = img.filter(ImageFilter.GaussianBlur(0.8))
    img.save(os.path.join(OUTPUT_DIR, "mud.png"))
    print("Generated: mud.png")


def generate_ice_texture():
    """Lacquered/shiny wood texture for ice."""
    # Light, slightly blue-tinted wood
    base_color = (220, 225, 235)  # Pale with hint of blue
    grain_color = (200, 205, 215)
    
    img = wood_grain(TEXTURE_SIZE, TEXTURE_SIZE, base_color, grain_color,
                     grain_density=0.06, vertical=True)
    
    # Add shine streaks
    draw = ImageDraw.Draw(img)
    for _ in range(5):
        x = random.randint(0, TEXTURE_SIZE)
        draw.line([(x, 0), (x + 20, TEXTURE_SIZE)], fill=(240, 245, 255), width=random.randint(2, 5))
    
    img = add_noise(img, 3)
    img = img.filter(ImageFilter.GaussianBlur(0.8))
    img.save(os.path.join(OUTPUT_DIR, "ice.png"))
    print("Generated: ice.png")


def generate_track_texture():
    """Tank track mark texture."""
    # Transparent background with dark track marks
    img = Image.new('RGBA', (64, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Draw tread pattern - series of short dashes
    track_color = (60, 50, 40, 180)  # Semi-transparent dark brown
    
    # Left tread
    for x in range(4, 60, 8):
        draw.rectangle([x, 2, x + 4, 6], fill=track_color)
    
    # Right tread  
    for x in range(4, 60, 8):
        draw.rectangle([x, 26, x + 4, 30], fill=track_color)
    
    img.save(os.path.join(OUTPUT_DIR, "track.png"))
    print("Generated: track.png")


def generate_spawn_marker():
    """Spawn point indicator texture."""
    size = 64
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Draw a circular marker
    center = size // 2
    # Outer ring
    draw.ellipse([4, 4, size - 4, size - 4], outline=(100, 200, 100, 200), width=3)
    # Inner circle
    draw.ellipse([center - 8, center - 8, center + 8, center + 8], fill=(100, 200, 100, 150))
    # Direction arrow (pointing up)
    draw.polygon([(center, 8), (center - 6, 20), (center + 6, 20)], fill=(150, 255, 150, 200))
    
    img.save(os.path.join(OUTPUT_DIR, "spawn_marker.png"))
    print("Generated: spawn_marker.png")


def main():
    ensure_dir()
    print(f"Generating textures in {OUTPUT_DIR}/...")
    print()
    
    generate_ground_texture()
    generate_wall_texture()
    generate_wall_side_texture()
    generate_border_texture()
    generate_water_texture()
    generate_mud_texture()
    generate_ice_texture()
    generate_track_texture()
    generate_spawn_marker()
    
    print()
    print("Done! All textures generated.")


if __name__ == "__main__":
    main()
