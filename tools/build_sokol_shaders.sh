#!/usr/bin/env bash
set -euo pipefail

os_name="$(uname -s)"
arch_name="$(uname -m)"

case "$os_name" in
    Darwin)
        bin_dir="osx"
        ;;
    Linux)
        bin_dir="linux"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        bin_dir="win32"
        ;;
    *)
        echo "Unsupported host OS: $os_name" >&2
        exit 1
        ;;
 esac

case "$arch_name" in
    arm64|aarch64)
        bin_dir="${bin_dir}_arm64"
        ;;
 esac

shdc="third_party/sokol-tools-bin/bin/${bin_dir}/sokol-shdc"
if [[ "$bin_dir" == win32* ]]; then
    shdc+=".exe"
fi

if [[ ! -x "$shdc" ]]; then
    echo "Missing sokol-shdc at $shdc" >&2
    echo "Clone https://github.com/floooh/sokol-tools-bin into third_party/" >&2
    exit 1
fi

"$shdc" --input shaders/tankgame.glsl \
    --output src/engine/render/pz_sokol_shaders.h \
    --slang glsl410:metal_macos \
    --reflection
