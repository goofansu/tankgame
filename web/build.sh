#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Create web-optimized assets
echo "=== Preparing web assets ==="
WEB_ASSETS_DIR="$PROJECT_DIR/build-web-assets"
mkdir -p "$WEB_ASSETS_DIR"

# Copy all assets
rsync -a --delete "$PROJECT_DIR/assets/" "$WEB_ASSETS_DIR/"

# Strip the soundfont to reduce download size
echo "Stripping soundfont..."
if command -v python3 &> /dev/null; then
    python3 "$PROJECT_DIR/tools/strip_soundfont.py" \
        --input "$PROJECT_DIR/assets/sounds/soundfont.sf2" \
        --output "$WEB_ASSETS_DIR/sounds/soundfont.sf2" \
        --midi-dir "$PROJECT_DIR/assets/music"
else
    echo "Warning: python3 not found, using full soundfont"
fi

# Build
echo ""
echo "=== Building WebAssembly ==="
mkdir -p "$PROJECT_DIR/build-web"
cd "$PROJECT_DIR/build-web"

emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DPZ_WEB_BUILD=ON \
    -DPZ_WEB_ASSETS_DIR="$WEB_ASSETS_DIR"

cmake --build . --parallel

# Report sizes
echo ""
echo "=== Build complete ==="
if [ -f tankgame.data ]; then
    DATA_SIZE=$(ls -lh tankgame.data | awk '{print $5}')
    WASM_SIZE=$(ls -lh tankgame.wasm | awk '{print $5}')
    echo "  tankgame.wasm: $WASM_SIZE"
    echo "  tankgame.data: $DATA_SIZE"
fi
