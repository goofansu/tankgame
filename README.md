# Tank Game

A multiplayer tank combat game built in C17 with SDL2 and OpenGL 3.3 with the help of Pi and Claude Opus.

Terrible and work in progress.

## Building

### Prerequisites
- CMake 3.16+
- C17 compiler (clang or gcc)
- SDL2

### macOS
```bash
brew install cmake sdl2
```

### Build
```bash
make build    # Build the project
make run      # Build and run
make clean    # Clean build directory
make debug    # Build with debug config
make release  # Build with release config
```

## Project Structure

```
tankgame/
├── src/
│   ├── main.c          # Entry point
│   ├── core/           # Foundation utilities
│   ├── engine/         # Core engine systems
│   │   └── render/     # Renderer API + backends
│   ├── game/           # Game-specific code
│   │   └── modes/      # Game mode logic
│   ├── editor/         # In-game editor
│   └── net/            # Networking
├── shaders/            # GLSL shaders
├── assets/             # Game assets
├── tests/              # Test suite
└── spec/               # Design documents
```

## Documentation

See `spec/` for design documents:
- `00-execution-plan.md` - Development milestones
- `01-game-design.md` - Game mechanics
- `02-engine-foundation.md` - Engine architecture
- `03-gameplay-tech.md` - Gameplay systems
- `04-network-protocol.md` - Networking
- `05-tooling-and-debugging.md` - Dev tools

## License

MIT
