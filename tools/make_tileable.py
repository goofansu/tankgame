#!/usr/bin/env python3
"""
Make a texture tileable using various seamless blending techniques.
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image, ImageFilter
    import numpy as np
    from scipy import ndimage
    from scipy.fft import fft2, ifft2
except ImportError as e:
    print(f"Error: Missing dependency - {e}")
    print("Install with: pip install Pillow numpy scipy")
    sys.exit(1)


def smooth_step(x):
    """Hermite smooth interpolation"""
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def make_tileable_edge(image: Image.Image, blend_width: int = None) -> Image.Image:
    """Blend opposite edges together (simple but can look mirrored)."""
    img = np.array(image, dtype=np.float32)
    h, w = img.shape[:2]
    
    if blend_width is None:
        blend_width = min(w, h) // 8
    
    result = img.copy()
    
    # Horizontal blend
    for x in range(blend_width):
        t = smooth_step(x / blend_width)
        left = img[:, x].copy()
        right = img[:, w - 1 - x].copy()
        avg = (left + right) / 2
        result[:, x] = t * left + (1 - t) * avg
        result[:, w - 1 - x] = t * right + (1 - t) * avg
    
    # Vertical blend
    for y in range(blend_width):
        t = smooth_step(y / blend_width)
        top = result[y].copy()
        bottom = result[h - 1 - y].copy()
        avg = (top + bottom) / 2
        result[y] = t * top + (1 - t) * avg
        result[h - 1 - y] = t * bottom + (1 - t) * avg
    
    return Image.fromarray(np.clip(result, 0, 255).astype(np.uint8))


def make_tileable_poisson(image: Image.Image, blend_width: int = None) -> Image.Image:
    """
    Gradient domain blending - preserves texture structure better.
    
    Instead of blending pixel values, we blend gradients and reconstruct.
    This maintains texture detail while making edges seamless.
    """
    img = np.array(image, dtype=np.float32)
    h, w = img.shape[:2]
    n_channels = img.shape[2] if len(img.shape) == 3 else 1
    
    if blend_width is None:
        blend_width = min(w, h) // 6
    
    if n_channels == 1:
        img = img[:, :, np.newaxis]
    
    result = np.zeros_like(img)
    
    for c in range(img.shape[2]):
        channel = img[:, :, c]
        
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
        
        # Blend gradients to make them tileable
        # At edges, blend with wrapped gradient from opposite side
        gx_wrapped = np.roll(gx, w // 2, axis=1)
        gy_wrapped = np.roll(gy, h // 2, axis=0)
        
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
        # div(grad) = laplacian, solve with periodic boundary
        divergence = np.gradient(gx, axis=1) + np.gradient(gy, axis=0)
        
        # Solve Poisson equation in frequency domain (periodic boundaries)
        result_channel = poisson_solve_periodic(divergence)
        
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


def poisson_solve_periodic(divergence):
    """Solve Poisson equation with periodic boundary conditions using FFT."""
    h, w = divergence.shape
    
    # Create frequency coordinates
    fy = np.fft.fftfreq(h)[:, np.newaxis]
    fx = np.fft.fftfreq(w)[np.newaxis, :]
    
    # Laplacian in frequency domain
    # -4*pi^2*(fx^2 + fy^2) for continuous, discrete version:
    denom = 2 * np.cos(2 * np.pi * fx) + 2 * np.cos(2 * np.pi * fy) - 4
    denom[0, 0] = 1  # Avoid division by zero (DC component)
    
    # Solve in frequency domain
    div_fft = fft2(divergence)
    result_fft = div_fft / denom
    result_fft[0, 0] = 0  # Set DC to zero (mean is arbitrary)
    
    result = np.real(ifft2(result_fft))
    return result


def make_tileable_fft(image: Image.Image, blend_width: int = None) -> Image.Image:
    """
    FFT-based method - make edges match in frequency domain.
    
    This smoothly wraps the image by working in frequency space,
    which naturally handles periodic boundaries.
    """
    img = np.array(image, dtype=np.float32)
    h, w = img.shape[:2]
    
    if blend_width is None:
        blend_width = min(w, h) // 4
    
    # Create smooth transition mask
    y_ramp = np.linspace(0, 1, h)
    x_ramp = np.linspace(0, 1, w)
    
    # Cosine window - smooth periodic transition
    y_window = 0.5 - 0.5 * np.cos(2 * np.pi * y_ramp)
    x_window = 0.5 - 0.5 * np.cos(2 * np.pi * x_ramp)
    
    window = np.outer(y_window, x_window)
    
    if len(img.shape) == 3:
        result = np.zeros_like(img)
        for c in range(img.shape[2]):
            channel = img[:, :, c]
            
            # Remove DC (mean)
            mean_val = channel.mean()
            centered = channel - mean_val
            
            # Apply window to make it periodic
            windowed = centered * window
            
            # The trick: create periodic version by blending windowed 
            # with shifted versions
            rolled_x = np.roll(centered, w // 2, axis=1)
            rolled_y = np.roll(centered, h // 2, axis=0)
            rolled_xy = np.roll(rolled_x, h // 2, axis=0)
            
            # Blend based on window
            blended = (centered * window + 
                      rolled_xy * (1 - window))
            
            result[:, :, c] = blended + mean_val
    else:
        mean_val = img.mean()
        centered = img - mean_val
        rolled_xy = np.roll(np.roll(centered, w // 2, axis=1), h // 2, axis=0)
        result = centered * window + rolled_xy * (1 - window) + mean_val
    
    return Image.fromarray(np.clip(result, 0, 255).astype(np.uint8))


def make_tileable_quilting(image: Image.Image, blend_width: int = None) -> Image.Image:
    """
    Simple quilting-inspired approach.
    
    Find the best seam path through the overlap region rather than
    blending everywhere. This preserves texture features better.
    """
    img = np.array(image, dtype=np.float32)
    h, w = img.shape[:2]
    
    if blend_width is None:
        blend_width = min(w, h) // 4
    
    # Roll image so edges meet in center
    rolled = np.roll(np.roll(img, w // 2, axis=1), h // 2, axis=0)
    
    # Now we need to find optimal seams at the center where edges meet
    center_y, center_x = h // 2, w // 2
    
    # For horizontal seam (at y = h/2), find minimum error path through x
    # For vertical seam (at x = w/2), find minimum error path through y
    
    # Create error surface - difference between adjacent rows/cols
    # at the seam locations
    
    result = rolled.copy()
    
    # Fix vertical seam (at x = w/2)
    seam_region = slice(center_x - blend_width, center_x + blend_width)
    
    # Compute error for vertical seam - difference when wrapping horizontally
    left_region = rolled[:, :blend_width]
    right_region = rolled[:, -blend_width:]
    
    if len(img.shape) == 3:
        error = np.sum((left_region - right_region[:, ::-1]) ** 2, axis=2)
    else:
        error = (left_region - right_region[:, ::-1]) ** 2
    
    # Find minimum cut using dynamic programming
    seam_x = find_min_seam_vertical(error)
    
    # Create mask based on seam
    mask = np.zeros((h, blend_width * 2), dtype=np.float32)
    for y in range(h):
        seam_pos = seam_x[y]
        mask[y, :seam_pos] = 1.0
        # Soft transition around seam
        for dx in range(-3, 4):
            x = seam_pos + dx
            if 0 <= x < blend_width * 2:
                t = 1.0 - abs(dx) / 4.0
                mask[y, x] = max(mask[y, x], 0.5 * t)
    
    # Apply seam mask at the vertical seam location
    x_start = center_x - blend_width
    x_end = center_x + blend_width
    
    left_source = np.roll(rolled, blend_width, axis=1)[:, x_start:x_end]
    right_source = np.roll(rolled, -blend_width, axis=1)[:, x_start:x_end]
    
    if len(img.shape) == 3:
        mask = mask[:, :, np.newaxis]
    
    result[:, x_start:x_end] = left_source * mask + right_source * (1 - mask)
    
    # Similarly fix horizontal seam
    y_start = center_y - blend_width
    y_end = center_y + blend_width
    
    top_region = rolled[:blend_width, :]
    bottom_region = rolled[-blend_width:, :]
    
    if len(img.shape) == 3:
        error_h = np.sum((top_region - bottom_region[::-1, :]) ** 2, axis=2)
    else:
        error_h = (top_region - bottom_region[::-1, :]) ** 2
    
    seam_y = find_min_seam_horizontal(error_h)
    
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
    
    if len(img.shape) == 3:
        mask_h = mask_h[:, :, np.newaxis]
    
    result[y_start:y_end, :] = top_source * mask_h + bottom_source * (1 - mask_h)
    
    return Image.fromarray(np.clip(result, 0, 255).astype(np.uint8))


def find_min_seam_vertical(error):
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


def find_min_seam_horizontal(error):
    """Find minimum cost horizontal seam."""
    return find_min_seam_vertical(error.T)


def create_tiled_preview(image: Image.Image, tiles: int = 2) -> Image.Image:
    """Create a tiled preview to verify seamless tiling."""
    w, h = image.size
    preview = Image.new(image.mode, (w * tiles, h * tiles))
    for y in range(tiles):
        for x in range(tiles):
            preview.paste(image, (x * w, y * h))
    return preview


def main():
    parser = argparse.ArgumentParser(
        description="Make a texture tileable",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s cobble.png                      # Default edge blend
  %(prog)s cobble.png --method quilting    # Best for irregular textures
  %(prog)s cobble.png --method poisson     # Gradient domain blending
  %(prog)s cobble.png --preview            # Save 2x2 tiled preview
  %(prog)s cobble.png --replace            # Overwrite original

Methods:
  edge     - Blend opposite edges (simple, can look mirrored)
  poisson  - Gradient domain blending (preserves structure)
  fft      - Frequency domain blending (smooth periodic)
  quilting - Find optimal seam through overlap (best for irregular textures)
        """
    )
    
    parser.add_argument("input", help="Input texture file")
    parser.add_argument("-o", "--output", help="Output file")
    parser.add_argument("--blend", type=int, help="Blend width in pixels")
    parser.add_argument("--method", choices=["edge", "poisson", "fft", "quilting"], 
                       default="edge", help="Tiling method")
    parser.add_argument("--preview", action="store_true", help="Save 2x2 preview")
    parser.add_argument("--replace", action="store_true", help="Replace original file")
    
    args = parser.parse_args()
    
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: File not found: {input_path}")
        sys.exit(1)
    
    if args.replace:
        output_path = input_path
    elif args.output:
        output_path = Path(args.output)
    else:
        output_path = input_path.with_stem(input_path.stem + "_tileable")
    
    print(f"Loading: {input_path}")
    image = Image.open(input_path)
    
    if image.mode not in ('RGB', 'RGBA', 'L'):
        image = image.convert('RGB')
    
    print(f"Size: {image.width}x{image.height}")
    print(f"Method: {args.method}")
    
    methods = {
        "edge": make_tileable_edge,
        "poisson": make_tileable_poisson,
        "fft": make_tileable_fft,
        "quilting": make_tileable_quilting,
    }
    
    result = methods[args.method](image, args.blend)
    
    print(f"Saving: {output_path}")
    result.save(output_path)
    
    if args.preview:
        preview_path = output_path.with_stem(output_path.stem + "_preview")
        preview = create_tiled_preview(result)
        print(f"Saving preview: {preview_path}")
        preview.save(preview_path)
    
    print("Done!")


if __name__ == "__main__":
    main()
