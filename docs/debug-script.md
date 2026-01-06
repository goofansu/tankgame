# Debug Script System

A simple scripting system for automated testing and visual validation of the tank game. This is **not** a gameplay scripting language—it's specifically designed for:

- Automated visual regression testing
- Reproducing bugs with specific input sequences
- Validating rendering and gameplay changes

For gameplay scripting (if added later), use a proper language like Lua.

## Usage

There are three ways to run debug script commands:

### 1. Script File

```bash
./build/tankgame --debug-script-file path/to/script.dbgscript
```

### 2. Inline Script

Commands can be passed directly on the command line, separated by semicolons:

```bash
./build/tankgame --debug-script "frames 3; screenshot debug-temp/test.png; quit"
```

### 3. Live Command Pipe

While the game is running, commands can be injected via a named pipe:

```bash
echo "screenshot debug-temp/live.png" > /tmp/tankgame_cmd
echo "aim 5.0 3.0; fire; frames 30; screenshot debug-temp/shot.png; quit" > /tmp/tankgame_cmd
```

Commands sent via the pipe replace any currently executing script.

**Note:** Audio is automatically disabled when running debug scripts (via `--debug-script-file` or `--debug-script`).

## Environment Variables

These environment variables control audio independently of debug scripts:

| Variable | Description |
|----------|-------------|
| `PZ_MUSIC=0` | Disable music |
| `PZ_SOUNDS=0` | Disable sound effects |

Example:
```bash
PZ_MUSIC=0 ./build/tankgame                    # Play without music
PZ_SOUNDS=0 ./build/tankgame                   # Play without sound effects
PZ_MUSIC=0 PZ_SOUNDS=0 ./build/tankgame        # Silent mode
```

## Temporary Files

Place all temporary debug artifacts (screenshots, state dumps) in `debug-temp/` which is gitignored.

## Command Reference

### Comments, Whitespace, and Separators

```
# This is a comment
# Empty lines are ignored

# Commands can be separated by newlines OR semicolons
frames 10; screenshot test.png; quit
```

### Execution Control

| Command | Description |
|---------|-------------|
| `turbo on\|off` | Run at maximum speed, skipping frame timing. Default: `on` |
| `render on\|off` | Skip rendering when `off` for faster execution. Default: `on` |
| `frames <n>` | Advance N simulation frames |
| `quit` | Exit the game |

### Map and State

| Command | Description |
|---------|-------------|
| `map <path>` | Load a map file (path relative to working directory) |
| `seed <n>` | Set RNG seed for reproducibility |

**Map loading notes:**
- The `map` command should appear **before** any `frames` commands to ensure the map is loaded before gameplay begins
- Paths are relative to the working directory (typically the project root)
- Maps are located in `assets/maps/` with `.map` extension
- If no `map` command is given, the game uses its default map selection

**Available maps** (in `assets/maps/`):
- `test_arena.map` - Simple test map with powerups and barriers
- `toxic_dawn.map` - Map with toxic zone mechanic
- `night_arena.map`, `day_arena.map` - Day/night themed arenas
- `lava_crossing.map`, `volcanic_ridge.map` - Lava-themed maps
- `frozen_peaks.map`, `fjord.map` - Ice/water themed maps
- Run `ls assets/maps/` for full list

**Example:**
```
# Load a specific map at the start
map assets/maps/test_arena.map

# Then proceed with testing
frames 5
screenshot debug-temp/map_loaded.png
```

### Input Control

Input commands are **additive** by default. Use `+` prefix to add, `-` prefix to remove, or `stop` to clear all.

Directions match the screen and keyboard (WASD):
- `up` = W key = toward top of screen
- `down` = S key = toward bottom of screen  
- `left` = A key = toward left of screen
- `right` = D key = toward right of screen

| Command | Description |
|---------|-------------|
| `input +up` | Add upward movement (W) |
| `input +down` | Add downward movement (S) |
| `input +left` | Add left movement (A) |
| `input +right` | Add right movement (D) |
| `input -up` | Remove upward movement |
| `input -down` | Remove downward movement |
| `input -left` | Remove left movement |
| `input -right` | Remove right movement |
| `input stop` | Clear all movement |
| `input up` | Same as `+up` (+ is optional) |

**Diagonal movement example:**
```
input +down
input +right
frames 60          # Moving diagonally down-right (into arena)
input -right
frames 30          # Now just moving down
input stop
frames 10          # Stopped
```

### Aiming and Firing

| Command | Description |
|---------|-------------|
| `aim <x> <y>` | Aim at world coordinates (sticky) |
| `fire` | Fire once (single press) |
| `hold_fire on\|off` | Hold fire button continuously |

### Debug Cheats

| Command | Description |
|---------|-------------|
| `god on\|off` | Enable/disable player invincibility |

### Output

| Command | Description |
|---------|-------------|
| `screenshot <path>` | Save screenshot to file |
| `dump <path>` | Dump game state to text file |

## Examples

### Basic Screenshot Test

```
# Wait for map to load
frames 5
screenshot debug-temp/01_initial.png
quit
```

### Movement Validation

```
# Test tank movement
frames 5
screenshot debug-temp/01_start.png
dump debug-temp/01_start.txt

# Move forward for 1 second
input +forward
frames 60
screenshot debug-temp/02_moved_forward.png
dump debug-temp/02_moved_forward.txt

# Verify position changed
input stop
quit
```

### Diagonal Movement and Firing

```
frames 5

# Move diagonally
input +forward
input +right
frames 30

# Stop horizontal, keep moving forward
input -right
frames 30

# Stop, aim, and fire
input stop
aim 10.0 5.0
fire
frames 5
screenshot debug-temp/fired.png

# Let projectile travel
frames 30
screenshot debug-temp/projectile.png

dump debug-temp/state.txt
quit
```

### Fast Forward with Render Skip

```
# Skip rendering to quickly reach a game state
turbo on
render off
frames 300

# Now render and capture
render on
screenshot debug-temp/after_300_frames.png
quit
```

### Stress Test with Continuous Fire

```
frames 5
input +forward
hold_fire on

# Fire while moving for 2 seconds
frames 120

hold_fire off
input stop
screenshot debug-temp/after_shooting.png
dump debug-temp/projectiles.txt
quit
```

### Testing with Invincibility

```
# Enable god mode for testing without dying
god on

frames 5
input +down
input +right

# Run around without taking damage
frames 300

screenshot debug-temp/god_mode_test.png
dump debug-temp/state.txt
quit
```

### Loading a Specific Map

```
# Load a specific map for testing
map assets/maps/toxic_dawn.map

# Set seed for reproducible enemy behavior
seed 42

# Enable god mode to observe without interference
god on

# Wait for everything to initialize
frames 5
screenshot debug-temp/toxic_start.png

# Fast-forward to observe toxic zone behavior
turbo on
render off
frames 1800

# Capture the result
render on
screenshot debug-temp/toxic_late.png
dump debug-temp/toxic_state.txt
quit
```

## State Dump Format

The `dump` command creates a text file with game state:

```
# Tank Game State Dump
frame: 66

[player]
pos: 5.123 3.456
vel: 0.250 0.100
body_angle: 1.570
turret_angle: 0.785
health: 10
flags: 0x00000009
fire_cooldown: 0.000

[tanks]
total: 3
enemies_alive: 2
enemies_dead: 0

[enemies]
0: pos=(7.000, -1.000) health=10 status=alive
1: pos=(12.500, 8.000) health=10 status=alive

[projectiles]
active: 1
  pos=(6.500, 2.000) vel=(5.000, 3.000) bounces=2
```

## Tips

1. **Use `turbo on` + `render off`** to quickly skip to interesting game states
2. **Use `seed <n>`** for reproducible tests
3. **Check state dumps** to verify positions and counts programmatically
4. **Place files in `debug-temp/`** to keep the repo clean
5. **Use the `read` tool** to view PNG screenshots directly

## File Extension

Use `.dbgscript` for debug script files to distinguish from potential future gameplay scripts.
