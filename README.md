# Tank Game

<p align="center">
  <img src="screenshot.png" width="70%" alt="Tank Game Screenshot">
</p>

A multiplayer tank combat game built in C17 with Sokol (sokol_app + sokol_gfx) with the help of Pi and Claude Opus.

Terrible and work in progress.

## Building

### Prerequisites
- CMake 3.16+
- C17 compiler (clang or gcc)
- Git (for submodules)

### macOS
```bash
brew install cmake
git submodule update --init --recursive
```

### Build
```bash
make build    # Build the project
make run      # Build and run
make clean    # Clean build directory
make debug    # Build with debug config
make release  # Build with release config
make web      # Build WASM build with docker
```

## Tools

### Map Tool
The `tools/map_tool.py` script provides CLI commands and a Python API for map manipulation:
```bash
./tools/map_tool.py --help           # Full documentation and examples
./tools/map_tool.py info <map>       # Show map info
./tools/map_tool.py validate <map>   # Validate and re-serialize
```

## License

- License: [Apache-2.0](https://github.com/mitsuhiko/tankgame/blob/main/LICENSE)

This code is entirely LLM generated. It is unclear if LLM generated code
can be copyrighted.
