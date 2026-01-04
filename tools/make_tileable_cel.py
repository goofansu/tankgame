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


def smooth_step(x):
    """Hermite smooth interpolation"""
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def make_tileable(img: Image.Image, blend_ratio: float = 0.25, method: str = "edge") -> Image.Image:
    """
    Make image tileable using the specified method.
    
    Methods:
        edge     - Blend opposite edges (simple, can look mirrored)
        poisson  - Gradient domain blending (preserves structure)
        fft      - Frequency domain blending (smooth periodic)
        quilting - Find optimal seam through overlap (best for irregular textures)
    """
    methods = {
        "edge": _make_tileable_edge,
        "poisson": _make_tileable_poisson,
        "fft": _make_tileable_fft,
        "quilting": _make_tileable_quilting,
    }
    
    if method not in methods:
        raise ValueError(f"Unknown tiling method: {method}. Use one of: {list(methods.keys())}")
    
    return methods[method](img, blend_ratio)


def _make_tileable_edge(img: Image.Image, blend_ratio: float = 0.25) -> Image.Image:
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


def _poisson_solve_periodic(divergence):
    """Solve Poisson equation with periodic boundary conditions using FFT."""
    from scipy.fft import fft2, ifft2
    
    h, w = divergence.shape
    
    # Create frequency coordinates
    fy = np.fft.fftfreq(h)[:, np.newaxis]
    fx = np.fft.fftfreq(w)[np.newaxis, :]
    
    # Laplacian in frequency domain (discrete version)
    denom = 2 * np.cos(2 * np.pi * fx) + 2 * np.cos(2 * np.pi * fy) - 4
    denom[0, 0] = 1  # Avoid division by zero (DC component)
    
    # Solve in frequency domain
    div_fft = fft2(divergence)
    result_fft = div_fft / denom
    result_fft[0, 0] = 0  # Set DC to zero (mean is arbitrary)
    
    result = np.real(ifft2(result_fft))
    return result


def _make_tileable_poisson(img: Image.Image, blend_ratio: float = 0.25) -> Image.Image:
    """
    Gradient domain blending - preserves texture structure better.
    
    Instead of blending pixel values, we blend gradients and reconstruct.
    This maintains texture detail while making edges seamless.
    """
    arr = np.array(img, dtype=np.float32)
    h, w = arr.shape[:2]
    n_channels = arr.shape[2] if len(arr.shape) == 3 else 1
    blend_width = int(min(w, h) * blend_ratio)
    
    if n_channels == 1:
        arr = arr[:, :, np.newaxis]
    
    result = np.zeros_like(arr)
    
    for c in range(arr.shape[2]):
        channel = arr[:, :, c]
        
        # Compute gradients
        gy, gx = np.gradient(channel)
        
        # Create weight mask - lower weight at edges
        weight_x = np.ones(w)
        weight_y = np.ones(h)
        
        for i in range(blend_width):
            t = smooth_step(i / blend_width)
            weight_x[i] = t
            weight_x[w - 1 - i] = t
            weight_y[i] = t
            weight_y[h - 1 - i] = t
        
        weight = np.outer(weight_y, weight_x)
        
        # For x gradient at left/right edges
        for x in range(blend_width):
            t = smooth_step(x / blend_width)
            gx[:, x] = t * gx[:, x] + (1 - t) * gx[:, w - 1 - x]
            gx[:, w - 1 - x] = t * gx[:, w - 1 - x] + (1 - t) * gx[:, x]
        
        # For y gradient at top/bottom edges
        for y in range(blend_width):
            t = smooth_step(y / blend_width)
            gy[y, :] = t * gy[y, :] + (1 - t) * gy[h - 1 - y, :]
            gy[h - 1 - y, :] = t * gy[h - 1 - y, :] + (1 - t) * gy[y, :]
        
        # Reconstruct from gradients using Poisson solver
        divergence = np.gradient(gx, axis=1) + np.gradient(gy, axis=0)
        result_channel = _poisson_solve_periodic(divergence)
        
        # Normalize to original range
        result_channel = result_channel - result_channel.min()
        orig_range = channel.max() - channel.min()
        if result_channel.max() > 0:
            result_channel = result_channel / result_channel.max() * orig_range
        result_channel = result_channel + channel.min()
        
        # Blend with original using weight mask to preserve center
        result[:, :, c] = weight * channel + (1 - weight) * result_channel
    
    if n_channels == 1:
        result = result[:, :, 0]
    
    return Image.fromarray(np.clip(result, 0, 255).astype(np.uint8))


def _make_tileable_fft(img: Image.Image, blend_ratio: float = 0.25) -> Image.Image:
    """
    FFT-based method - make edges match in frequency domain.
    
    This smoothly wraps the image by working in frequency space,
    which naturally handles periodic boundaries.
    """
    arr = np.array(img, dtype=np.float32)
    h, w = arr.shape[:2]
    
    # Create smooth transition mask using cosine window
    y_ramp = np.linspace(0, 1, h)
    x_ramp = np.linspace(0, 1, w)
    
    y_window = 0.5 - 0.5 * np.cos(2 * np.pi * y_ramp)
    x_window = 0.5 - 0.5 * np.cos(2 * np.pi * x_ramp)
    
    window = np.outer(y_window, x_window)
    
    if len(arr.shape) == 3:
        result = np.zeros_like(arr)
        for c in range(arr.shape[2]):
            channel = arr[:, :, c]
            mean_val = channel.mean()
            centered = channel - mean_val
            rolled_xy = np.roll(np.roll(centered, w // 2, axis=1), h // 2, axis=0)
            result[:, :, c] = centered * window + rolled_xy * (1 - window) + mean_val
    else:
        mean_val = arr.mean()
        centered = arr - mean_val
        rolled_xy = np.roll(np.roll(centered, w // 2, axis=1), h // 2, axis=0)
        result = centered * window + rolled_xy * (1 - window) + mean_val
    
    return Image.fromarray(np.clip(result, 0, 255).astype(np.uint8))


def _find_min_seam_vertical(error):
    """Find minimum cost vertical seam using dynamic programming."""
    h, w = error.shape
    dp = error.copy()
    
    # Build up costs
    for y in range(1, h):
        for x in range(w):
            candidates = [dp[y-1, x]]
            if x > 0:
                candidates.append(dp[y-1, x-1])
            if x < w - 1:
                candidates.append(dp[y-1, x+1])
            dp[y, x] += min(candidates)
    
    # Backtrack to find seam
    seam = np.zeros(h, dtype=np.int32)
    seam[-1] = np.argmin(dp[-1])
    
    for y in range(h - 2, -1, -1):
        x = seam[y + 1]
        candidates = [(dp[y, x], x)]
        if x > 0:
            candidates.append((dp[y, x-1], x-1))
        if x < w - 1:
            candidates.append((dp[y, x+1], x+1))
        seam[y] = min(candidates)[1]
    
    return seam


def _make_tileable_quilting(img: Image.Image, blend_ratio: float = 0.25) -> Image.Image:
    """
    Quilting-inspired approach using optimal seam finding.
    
    Finds the best seam path through the overlap region rather than
    blending everywhere. This preserves texture features better.
    """
    arr = np.array(img, dtype=np.float32)
    h, w = arr.shape[:2]
    blend_width = int(min(w, h) * blend_ratio)
    
    # Roll image so edges meet in center
    rolled = np.roll(np.roll(arr, w // 2, axis=1), h // 2, axis=0)
    
    center_y, center_x = h // 2, w // 2
    result = rolled.copy()
    
    # Fix vertical seam (at x = w/2)
    left_region = rolled[:, :blend_width]
    right_region = rolled[:, -blend_width:]
    
    if len(arr.shape) == 3:
        error = np.sum((left_region - right_region[:, ::-1]) ** 2, axis=2)
    else:
        error = (left_region - right_region[:, ::-1]) ** 2
    
    seam_x = _find_min_seam_vertical(error)
    
    # Create mask based on seam
    mask = np.zeros((h, blend_width * 2), dtype=np.float32)
    for y in range(h):
        seam_pos = seam_x[y]
        mask[y, :seam_pos] = 1.0
        for dx in range(-3, 4):
            x = seam_pos + dx
            if 0 <= x < blend_width * 2:
                t = 1.0 - abs(dx) / 4.0
                mask[y, x] = max(mask[y, x], 0.5 * t)
    
    x_start = center_x - blend_width
    x_end = center_x + blend_width
    
    left_source = np.roll(rolled, blend_width, axis=1)[:, x_start:x_end]
    right_source = np.roll(rolled, -blend_width, axis=1)[:, x_start:x_end]
    
    if len(arr.shape) == 3:
        mask_3d = mask[:, :, np.newaxis]
        result[:, x_start:x_end] = left_source * mask_3d + right_source * (1 - mask_3d)
    else:
        result[:, x_start:x_end] = left_source * mask + right_source * (1 - mask)
    
    # Fix horizontal seam
    y_start = center_y - blend_width
    y_end = center_y + blend_width
    
    top_region = rolled[:blend_width, :]
    bottom_region = rolled[-blend_width:, :]
    
    if len(arr.shape) == 3:
        error_h = np.sum((top_region - bottom_region[::-1, :]) ** 2, axis=2)
    else:
        error_h = (top_region - bottom_region[::-1, :]) ** 2
    
    seam_y = _find_min_seam_vertical(error_h.T)
    
    mask_h = np.zeros((blend_width * 2, w), dtype=np.float32)
    for x in range(w):
        seam_pos = seam_y[x]
        mask_h[:seam_pos, x] = 1.0
        for dy in range(-3, 4):
            y = seam_pos + dy
            if 0 <= y < blend_width * 2:
                t = 1.0 - abs(dy) / 4.0
                mask_h[y, x] = max(mask_h[y, x], 0.5 * t)
    
    top_source = np.roll(result, blend_width, axis=0)[y_start:y_end, :]
    bottom_source = np.roll(result, -blend_width, axis=0)[y_start:y_end, :]
    
    if len(arr.shape) == 3:
        mask_h_3d = mask_h[:, :, np.newaxis]
        result[y_start:y_end, :] = top_source * mask_h_3d + bottom_source * (1 - mask_h_3d)
    else:
        result[y_start:y_end, :] = top_source * mask_h + bottom_source * (1 - mask_h)
    
    return Image.fromarray(np.clip(result, 0, 255).astype(np.uint8))


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
