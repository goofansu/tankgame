#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = [
#     "pillow",
#     "numpy",
#     "scipy",
# ]
# ///
"""
Make a texture tileable and apply cel-shading (posterization + edges) effect.

Takes an input image and:
1. Crops to square (center crop)
2. Makes it seamlessly tileable via edge blending
3. Applies cel-shading: posterization + edge detection for outlines
4. Downscales to target size (default 512x512)
"""

import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage


def center_crop_square(img: Image.Image) -> Image.Image:
    """Crop image to square using center crop."""
    w, h = img.size
    size = min(w, h)
    left = (w - size) // 2
    top = (h - size) // 2
    return img.crop((left, top, left + size, top + size))


def make_tileable(img: Image.Image, blend_ratio: float = 0.25) -> Image.Image:
    """
    Make image tileable by blending edges with their opposites.
    
    Uses cosine blending at all four edges so they wrap seamlessly.
    """
    arr = np.array(img, dtype=np.float32)
    h, w = arr.shape[:2]
    
    blend_w = int(w * blend_ratio)
    blend_h = int(h * blend_ratio)
    
    # Create horizontal blend (left-right wrapping)
    for i in range(blend_w):
        # Cosine blend factor (0 at edge, 1 at blend boundary)
        t = 0.5 - 0.5 * np.cos(np.pi * i / blend_w)
        # Blend left edge with wrapped right edge
        arr[:, i] = arr[:, i] * t + arr[:, w - blend_w + i] * (1 - t)
        # Blend right edge with wrapped left edge  
        arr[:, w - 1 - i] = arr[:, w - 1 - i] * t + arr[:, blend_w - 1 - i] * (1 - t)
    
    # Create vertical blend (top-bottom wrapping)
    for i in range(blend_h):
        t = 0.5 - 0.5 * np.cos(np.pi * i / blend_h)
        arr[i, :] = arr[i, :] * t + arr[h - blend_h + i, :] * (1 - t)
        arr[h - 1 - i, :] = arr[h - 1 - i, :] * t + arr[blend_h - 1 - i, :] * (1 - t)
    
    return Image.fromarray(arr.astype(np.uint8))


def detect_edges(img: Image.Image, threshold: float = 0.15, thickness: int = 1) -> np.ndarray:
    """
    Detect edges using Sobel operator.
    
    Returns a mask where edges are 1.0 and non-edges are 0.0.
    If thickness is 0, returns all zeros (no edges).
    """
    if thickness <= 0:
        return np.zeros((img.size[1], img.size[0]), dtype=np.float32)
    
    # Convert to grayscale for edge detection
    gray = np.array(img.convert('L'), dtype=np.float32) / 255.0
    
    # Sobel edge detection
    sobel_x = ndimage.sobel(gray, axis=1)
    sobel_y = ndimage.sobel(gray, axis=0)
    edges = np.hypot(sobel_x, sobel_y)
    
    # Normalize and threshold
    edges = edges / edges.max() if edges.max() > 0 else edges
    edge_mask = (edges > threshold).astype(np.float32)
    
    # Dilate edges for thickness
    if thickness > 1:
        struct = ndimage.generate_binary_structure(2, 1)
        edge_mask = ndimage.binary_dilation(edge_mask, struct, iterations=thickness-1)
        edge_mask = edge_mask.astype(np.float32)
    
    return edge_mask


def posterize_levels(img: Image.Image, levels: int = 4) -> Image.Image:
    """
    Posterize by reducing each channel to N discrete levels.
    
    This gives harder band edges than palette quantization.
    """
    arr = np.array(img, dtype=np.float32)
    
    # Quantize to discrete levels
    # e.g., levels=4 means values 0, 85, 170, 255
    step = 255.0 / (levels - 1)
    arr = np.round(arr / step) * step
    arr = np.clip(arr, 0, 255)
    
    return Image.fromarray(arr.astype(np.uint8))


def posterize_kmeans(img: Image.Image, num_colors: int = 8) -> Image.Image:
    """
    Posterize using k-means clustering for better color preservation.
    
    Groups similar colors together, preserving the image's color palette.
    """
    arr = np.array(img, dtype=np.float32)
    h, w, c = arr.shape
    pixels = arr.reshape(-1, c)
    
    # Simple k-means implementation
    np.random.seed(42)
    # Initialize centroids using random pixels
    indices = np.random.choice(len(pixels), num_colors, replace=False)
    centroids = pixels[indices].copy()
    
    for _ in range(20):  # iterations
        # Assign pixels to nearest centroid
        distances = np.linalg.norm(pixels[:, np.newaxis] - centroids, axis=2)
        labels = np.argmin(distances, axis=1)
        
        # Update centroids
        new_centroids = np.array([
            pixels[labels == k].mean(axis=0) if np.sum(labels == k) > 0 else centroids[k]
            for k in range(num_colors)
        ])
        
        if np.allclose(centroids, new_centroids):
            break
        centroids = new_centroids
    
    # Replace pixels with their centroid colors
    result = centroids[labels].reshape(h, w, c)
    return Image.fromarray(result.astype(np.uint8))


def apply_cel_shading(
    img: Image.Image,
    num_colors: int = 8,
    edge_threshold: float = 0.2,
    edge_thickness: int = 1,
    edge_darkness: float = 0.5,
    pre_smooth: int = 5,
    use_kmeans: bool = True,
) -> Image.Image:
    """
    Apply cel-shading effect: flat colors + dark edge outlines.
    
    Args:
        img: Input image
        num_colors: Number of colors in the palette
        edge_threshold: Threshold for edge detection (0-1)
        edge_thickness: Thickness of edge lines in pixels
        edge_darkness: How dark the edges are (0=no change, 1=black)
        pre_smooth: Median filter size before processing (reduces noise)
        use_kmeans: Use k-means for better color clustering
    """
    # Pre-smooth to reduce noise and simplify
    if pre_smooth > 0:
        size = pre_smooth if pre_smooth % 2 == 1 else pre_smooth + 1
        img = img.filter(ImageFilter.MedianFilter(size=size))
    
    # Posterize colors first
    if use_kmeans:
        posterized = posterize_kmeans(img, num_colors=num_colors)
    else:
        # Calculate levels from num_colors (cube root for RGB)
        levels = max(2, int(round(num_colors ** (1/3))))
        posterized = posterize_levels(img, levels=levels)
    
    # Detect edges on the posterized result (finds color boundaries)
    edge_mask = detect_edges(posterized, threshold=edge_threshold, thickness=edge_thickness)
    
    # Apply edge darkening
    arr = np.array(posterized, dtype=np.float32)
    
    # Darken pixels where edges are detected
    # edge_darkness=1.0 means fully black edges, 0.0 means no darkening
    edge_factor = 1.0 - edge_mask[:, :, np.newaxis] * edge_darkness
    arr = arr * edge_factor
    
    return Image.fromarray(arr.astype(np.uint8))


def process_texture(
    input_path: Path,
    output_path: Path,
    num_colors: int = 8,
    output_size: int = 512,
    blend_ratio: float = 0.25,
    edge_threshold: float = 0.2,
    edge_thickness: int = 1,
    edge_darkness: float = 0.5,
    pre_smooth: int = 5,
) -> None:
    """Process a texture with the full pipeline."""
    
    # Load image
    img = Image.open(input_path).convert('RGB')
    print(f"Loaded: {input_path} ({img.size[0]}x{img.size[1]})")
    
    # Step 1: Center crop to square
    img = center_crop_square(img)
    print(f"Cropped to square: {img.size[0]}x{img.size[1]}")
    
    # Step 2: Make tileable (do this at high res for better blending)
    img = make_tileable(img, blend_ratio=blend_ratio)
    print(f"Made tileable (blend ratio: {blend_ratio})")
    
    # Step 3: Downscale to target size
    if img.size[0] != output_size:
        img = img.resize((output_size, output_size), Image.Resampling.LANCZOS)
        print(f"Resized to: {output_size}x{output_size}")
    
    # Step 4: Apply cel shading (posterize + edges)
    img = apply_cel_shading(
        img,
        num_colors=num_colors,
        edge_threshold=edge_threshold,
        edge_thickness=edge_thickness,
        edge_darkness=edge_darkness,
        pre_smooth=pre_smooth,
    )
    print(f"Applied cel shading (colors={num_colors}, edges={edge_threshold}/{edge_thickness})")
    
    # Save result
    img.save(output_path)
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Make a texture tileable and apply cel-shading effect."
    )
    parser.add_argument("input", type=Path, help="Input image path")
    parser.add_argument("output", type=Path, help="Output image path")
    parser.add_argument(
        "-c", "--colors", type=int, default=8,
        help="Number of colors in palette (default: 8)"
    )
    parser.add_argument(
        "-s", "--size", type=int, default=512,
        help="Output size in pixels (default: 512)"
    )
    parser.add_argument(
        "-b", "--blend", type=float, default=0.25,
        help="Blend ratio for tiling seams, 0.0-0.5 (default: 0.25)"
    )
    parser.add_argument(
        "--edge-threshold", type=float, default=0.2,
        help="Edge detection threshold 0-1 (default: 0.2)"
    )
    parser.add_argument(
        "--edge-thickness", type=int, default=1,
        help="Edge line thickness in pixels, 0 to disable (default: 1)"
    )
    parser.add_argument(
        "--edge-darkness", type=float, default=0.5,
        help="Edge darkness 0=none 1=black (default: 0.5)"
    )
    parser.add_argument(
        "--pre-smooth", type=int, default=5,
        help="Pre-smoothing filter size, 0 to disable (default: 5)"
    )
    
    args = parser.parse_args()
    
    if not args.input.exists():
        print(f"Error: Input file not found: {args.input}")
        return 1
    
    process_texture(
        input_path=args.input,
        output_path=args.output,
        num_colors=args.colors,
        output_size=args.size,
        blend_ratio=args.blend,
        edge_threshold=args.edge_threshold,
        edge_thickness=args.edge_thickness,
        edge_darkness=args.edge_darkness,
        pre_smooth=args.pre_smooth,
    )
    return 0


if __name__ == "__main__":
    exit(main())
