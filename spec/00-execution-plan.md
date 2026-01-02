# Execution Plan

This document breaks the project into small, verifiable milestones. Each milestone should be completable in a focused session and has clear validation criteria.

## Guiding Principles

1. **Always have something running** - Every milestone produces runnable output
2. **Test as we go** - Add tests for core functionality immediately
3. **Debug tools early** - Invest in visibility before complexity
4. **Vertical slices** - Get thin features working end-to-end before widening
5. **Defer polish** - Get it working, then make it good

---

## Phase 0: Project Setup
*Foundation before any code*

### M0.1: Repository Structure
- [ ] Create directory structure per docs/02-engine-foundation.md
- [ ] Set up CMakeLists.txt with Debug/Dev/Release configs
- [ ] Add .gitignore, README.md
- [ ] Verify builds on target platforms (macOS initially)

**Validation:** `make build` succeeds, `make run` produces output

### M0.2: Dependencies
- [ ] Integrate SDL2 (via find_package or FetchContent)
- [ ] Verify SDL2 links and initializes
- [ ] Create window, handle quit event

**Validation:** Window opens, closes cleanly on Cmd+Q

### M0.3: OpenGL Context (Core Profile)
- [ ] Initialize OpenGL 3.3 core context via SDL (macOS-friendly)
- [ ] Clear screen to a color
- [ ] Basic GL error checking helper
- [ ] Keep shaders in GLSL 330 to avoid ES-only features

**Validation:** Window shows solid color (e.g., cornflower blue)

### M0.4: Minimal C Test Harness
- [ ] Add `tests/` with a tiny test runner (no external deps)
- [ ] Wire test target into CMake + `ctest`
- [ ] Add one smoke test to prove the harness works

**Validation:** `ctest` runs and reports pass/fail

---

## Phase 1: Core Utilities
*The foundation everything builds on*

### M1.1: Memory System
- [ ] Implement `pz_alloc`, `pz_free`, `pz_realloc`, `pz_calloc`
- [ ] Add allocation tracking (count, total bytes)
- [ ] Implement `pz_mem_dump_leaks()` for debug builds
- [ ] Add memory category tagging

**Validation:** 
- Unit test: alloc/free cycles, leak detection catches intentional leak
- Print stats at shutdown, verify 0 leaks

### M1.2: Logging System
- [ ] Implement `pz_log()` with levels and categories
- [ ] Console output with colors (ANSI)
- [ ] Log to file option
- [ ] Compile-out TRACE/DEBUG in release

**Validation:** 
- Various log levels appear correctly
- Filter by category works

### M1.3: Math Library
- [ ] `pz_vec2`: add, sub, scale, dot, len, normalize, rotate, reflect
- [ ] `pz_vec3`: add, sub, scale, dot, cross, len, normalize
- [ ] `pz_vec4`: basic ops
- [ ] `pz_mat4`: identity, mul, translate, rotate_x/y/z, scale, perspective, look_at, inverse

**Validation:** 
- Unit tests for all operations
- Test perspective * look_at produces expected view-projection

### M1.4: Data Structures
- [ ] `pz_list`: intrusive doubly-linked list with macros
- [ ] `pz_array`: stretchy buffer (stb-style)
- [ ] `pz_hashmap`: string keys, void* values

**Validation:**
- Unit tests: insert, remove, iterate, edge cases
- Hashmap: collision handling, resize

### M1.5: String Utilities
- [ ] `pz_str_dup`, `pz_str_fmt` (allocating sprintf)
- [ ] `pz_str_split`, `pz_str_trim`
- [ ] `pz_str_starts_with`, `pz_str_ends_with`
- [ ] `pz_str_to_int`, `pz_str_to_float`

**Validation:** Unit tests for each function

### M1.6: Platform Layer
- [ ] `pz_platform_get_time()` - high precision timer
- [ ] `pz_platform_read_file()` - read entire file
- [ ] `pz_platform_write_file()` - write entire file
- [ ] `pz_platform_file_exists()`, `pz_platform_file_mtime()`

**Validation:** 
- Timer resolution test (measure known sleep)
- Read/write round-trip test

---

## Phase 2: Rendering Foundation
*Get triangles on screen*

### M2.1: Renderer Abstraction + Shader System
- [ ] Define backend-agnostic `pz_renderer` API (buffers, textures, render targets, pipelines)
- [ ] Implement OpenGL 3.3 backend behind the API
- [ ] Add a null renderer for tests/headless runs
- [ ] Ensure no GL types leak outside renderer headers
- [ ] Load shader source from file
- [ ] Compile vertex + fragment shader
- [ ] Link program, error reporting
- [ ] Uniform setters (float, vec2, vec3, vec4, mat4)
- [ ] Hot-reload on file change

**Validation:** 
- Renderer init selects backend; null backend runs without GL
- Load a test shader, compile succeeds
- Modify shader file, see it reload

### M2.2: Basic Rendering
- [ ] Create VAO/VBO helpers
- [ ] Render a colored triangle
- [ ] Render a textured quad

**Validation:** Triangle and quad visible on screen

### M2.3: Texture System
- [ ] Load PNG/JPG via stb_image
- [ ] Upload to GL texture
- [ ] Texture cache (don't load same file twice)
- [ ] Hot-reload on file change

**Validation:** Textured quad shows image, modify image file, see update

### M2.4: Camera
- [ ] Implement `pz_camera` with perspective projection
- [ ] Position camera for near-top-down view (~15-20° tilt)
- [ ] `pz_camera_screen_to_world()` for mouse picking
- [ ] View-projection matrix uniform upload

**Validation:** 
- Render a grid of quads on ground plane
- Camera looks correct (slight perspective, angled view)
- Click on screen, verify world coordinate calculation

### M2.5: Debug Overlay Foundation
- [ ] Basic immediate-mode text rendering (bitmap font)
- [ ] FPS counter
- [ ] Frame time graph
- [ ] Toggle with F2

**Validation:** Press F2, see FPS and frame graph

---

## Phase 3: Game World Foundation
*A world to play in*

### M3.1: Ground Plane
- [ ] Render a textured ground plane
- [ ] Tile the texture across the map
- [ ] Simple vertex colors or basic lighting

**Validation:** See a tiled floor filling the view

### M3.2: Map Data Structure
- [ ] Define `pz_map` structure (terrain grid, size)
- [ ] Build a hardcoded test map in code (terrain only)
- [ ] Add minimal helpers to query terrain and bounds
- [ ] Defer on-disk map format until editor work (M11.5)

**Validation:** Build a test map, print stats and sample queries

### M3.3: Map Rendering - Ground
- [ ] Generate mesh from terrain data
- [ ] Different tiles for different terrain types
- [ ] Render terrain layer

**Validation:** Load map, see terrain rendered correctly

### M3.4: Wall Geometry
- [ ] Parse height map from map file
- [ ] Generate 3D wall meshes from height data
- [ ] Render walls with basic shading

**Validation:** Walls appear as 3D boxes at correct positions

### M3.5: Map Hot-Reload
- [ ] Watch map file for changes
- [ ] Reload and regenerate geometry on change
- [ ] Keep camera position on reload

**Validation:** Edit map file in text editor, see changes immediately

---

## Phase 4: First Entity - The Tank
*Something that moves*

### M4.1: Simple 3D Mesh Loading
- [ ] Define simple mesh format (or hardcode tank geometry)
- [ ] Tank body mesh (box with details)
- [ ] Tank turret mesh (separate piece)

**Validation:** Render static tank mesh at origin

### M4.2: Entity System Foundation
- [ ] Define `pz_entity` base structure
- [ ] Define `pz_entity_manager` with typed lists
- [ ] Entity create/destroy lifecycle

**Validation:** Create entity, verify in list, destroy, verify removed

### M4.3: Tank Entity
- [ ] Define `pz_tank` structure
- [ ] Spawn tank at position
- [ ] Render tank body + turret (turret rotates independently)

**Validation:** Tank visible on map, turret points different direction than body

### M4.4: Input System
- [ ] Implement `pz_input` (keyboard state, mouse position)
- [ ] Track pressed/released this frame
- [ ] Mouse world position via camera

**Validation:** Print key presses, mouse world coords in debug overlay

### M4.5: Simulation Timestep + Determinism Harness
- [ ] Fixed timestep game loop (separate sim tick from render)
- [ ] Define `pz_tank_input` (buttons + aim) and feed sim from it
- [ ] Deterministic RNG (seeded) for gameplay-facing randomness
- [ ] Stable update order (no hash iteration) and per-tick state hash
- [ ] Input record/replay for short sessions (testing only, not lockstep networking)

**Validation:** 
- Record inputs for a short session, replay produces same state hash
- Tank position and turret angle match on replay

### M4.6: Tank Movement
- [ ] WASD to set movement direction (via `pz_tank_input`)
- [ ] Tank rotates toward movement direction (turn in place)
- [ ] Tank moves forward/backward with small acceleration
- [ ] Mouse aims turret

**Validation:** Drive tank around with WASD, aim with mouse

### M4.7: Tank-Wall Collision
- [ ] Simple AABB collision for tank
- [ ] Query map for solid tiles
- [ ] Stop tank at walls (slide along)

**Validation:** Tank cannot drive through walls

---

## Phase 5: Physics and Collision
*Things interacting*

### M5.1: Collision Shapes
- [ ] Define `pz_collider` (circle, rect)
- [ ] Circle vs circle test
- [ ] Circle vs AABB test
- [ ] AABB vs AABB test

**Validation:** Unit tests for all collision pairs

### M5.2: Collision World
- [ ] `pz_collision_world` that knows about map + entities
- [ ] Spatial query: what's at this point?
- [ ] Sweep query: what would I hit moving from A to B?

**Validation:** Debug draw collision shapes (F3)

### M5.3: Raycast
- [ ] Ray vs AABB
- [ ] Ray vs circle
- [ ] Ray vs map (tile-based)
- [ ] Return hit point, normal, entity

**Validation:** 
- Click to cast ray, draw result
- Line-of-sight check between two points

### M5.4: Tank-Tank Collision
- [ ] Tanks collide with each other
- [ ] Tanks stop (no pushing)

**Validation:** Two tanks, drive into each other, both stop

---

## Phase 6: Weapons and Projectiles
*Pew pew*

### M6.1: Projectile Entity
- [ ] Define `pz_projectile` structure
- [ ] Simple sphere/capsule mesh
- [ ] Move in straight line
- [ ] Destroy after lifetime

**Validation:** Spawn projectile, watch it fly and disappear

### M6.2: Tank Firing
- [ ] Fire on left click
- [ ] Spawn projectile at turret tip
- [ ] Projectile inherits turret direction
- [ ] Fire cooldown

**Validation:** Click to fire, see projectile spawn and fly

### M6.3: Projectile-Wall Collision
- [ ] Detect wall hit
- [ ] Bounce off wall (reflect velocity)
- [ ] Track bounce count, destroy after max

**Validation:** Fire at wall, projectile bounces off

### M6.4: Projectile-Tank Collision
- [ ] Detect tank hit (ignore self)
- [ ] Apply damage
- [ ] Destroy projectile
- [ ] Visual feedback (flash, sound placeholder)

**Validation:** Fire at other tank, damage applied, projectile disappears

### M6.5: Tank Death and Respawn
- [ ] Tank destroyed at 0 HP
- [ ] Death visual (placeholder explosion)
- [ ] Respawn after delay at spawn point

**Validation:** Kill tank, see death effect, tank respawns

---

## Phase 7: Track Accumulation
*Visual polish that matters*

### M7.1: Track Texture System
- [ ] Create FBO for track accumulation
- [ ] Render track marks as textured quads
- [ ] Blend additively into accumulation texture

**Validation:** Tank moves, leaves dark track marks

### M7.2: Track Rendering
- [ ] Sample track texture when rendering ground
- [ ] Properly UV map to world coordinates

**Validation:** Tracks persist, visible on ground

### M7.3: Track Fidelity
- [ ] Place track marks at regular intervals based on distance
- [ ] Rotate track marks to match tank angle
- [ ] Both treads leave separate marks

**Validation:** Tracks look like tank treads, not just blobs

---

## Phase 8: Audio Foundation
*The game has sound*

### M8.1: Audio System Setup
- [ ] Initialize miniaudio (or SDL_mixer)
- [ ] Load WAV/OGG files
- [ ] Play sound once

**Validation:** Play a test sound on keypress

### M8.2: Spatial Audio
- [ ] Calculate pan based on world position
- [ ] Calculate volume based on distance
- [ ] Listener position at camera/player

**Validation:** Sound pans as source moves across screen

### M8.3: Game Sounds
- [ ] Tank engine loop
- [ ] Fire sound
- [ ] Impact/bounce sound
- [ ] Explosion sound

**Validation:** All key actions have sound feedback

---

## Phase 9: Mines and Pickups
*More gameplay elements*

### M9.1: Mine Entity
- [ ] Define `pz_mine` structure
- [ ] Place mine on right-click
- [ ] Mine mesh/sprite
- [ ] Arm delay

**Validation:** Place mine, see it appear

### M9.2: Mine Triggering
- [ ] Proximity detection (armed mines)
- [ ] Timed mines (fuse countdown)
- [ ] Explosion effect and damage radius

**Validation:** Drive over mine, it explodes, damage dealt

### M9.3: Shooting Mines
- [ ] Projectile-mine collision
- [ ] Mine detonates when shot

**Validation:** Shoot mine, it explodes

### M9.4: Pickup Entity
- [ ] Define `pz_pickup` structure
- [ ] Parse pickup positions from map
- [ ] Render pickups (floating, rotating)
- [ ] Respawn timer

**Validation:** Pickups visible on map

### M9.5: Pickup Collection
- [ ] Tank-pickup collision
- [ ] Apply effect (health, ammo, speed, mines, crates)
- [ ] Pickup disappears, respawn timer starts

**Validation:** Drive over pickup, effect applied, pickup respawns later

### M9.6: Weapon Switching
- [ ] Track current weapon type and ammo
- [ ] Different weapon properties (damage, speed, bounces)
- [ ] Ammo depletion returns to default weapon

**Validation:** Pickup weapon, fire different projectiles, runs out, back to default

---

## Phase 10: Destructibles
*Things that break*

### M10.1: Destructible Entity
- [ ] Define `pz_destructible` structure
- [ ] Parse from map
- [ ] Crates and barrels

**Validation:** Crates visible on map

### M10.2: Destructible Collision
- [ ] Block tank movement
- [ ] Block projectiles

**Validation:** Can't drive or shoot through crates

### M10.3: Destroying Destructibles
- [ ] Track HP
- [ ] Projectile hit reduces HP
- [ ] Destroyed at 0 HP
- [ ] Barrel explosion damages nearby

**Validation:** Shoot crate until destroyed, barrel explodes

### M10.4: Crate Placing
- [ ] Pickup gives crate placer
- [ ] Place crate with key
- [ ] Placed crate is normal destructible

**Validation:** Pick up crate placer, place crates, they work normally

---

## Phase 11: Map Editor
*Build levels in-game*

### M11.1: Editor Mode Toggle
- [ ] F1 enters/exits editor mode
- [ ] Game pauses in editor
- [ ] Camera can pan/zoom in editor
- [ ] Show grid overlay

**Validation:** Toggle editor, game pauses, can look around

### M11.2: Tile Painting
- [ ] Tool palette (terrain types)
- [ ] Click to paint tiles
- [ ] Brush size

**Validation:** Paint terrain, see changes immediately

### M11.3: Wall Height Editing
- [ ] Scroll wheel or keys to adjust height
- [ ] Wall geometry updates live

**Validation:** Raise/lower walls in editor

### M11.4: Entity Placement
- [ ] Place spawn points
- [ ] Place pickups (with type selection)
- [ ] Place destructibles
- [ ] Delete entities

**Validation:** Place entities, they appear in game

### M11.5: Map Format + Saving
- [ ] Define simple text map format (versioned header, no deps)
- [ ] Load map from disk on startup
- [ ] Save to text format
- [ ] Ctrl+S saves
- [ ] Autosave option

**Validation:** Edit map, save, reload game, map persists

### M11.6: Game Mode Objects
- [ ] Place flag positions
- [ ] Place/resize control zones
- [ ] Mode-specific properties

**Validation:** Place CTF flags, domination zones

---

## Phase 12: Game Modes (Offline)
*Single-player/local rules*

### M12.1: Mode Framework
- [ ] Define `pz_game_mode` interface
- [ ] Mode init, update, event hooks
- [ ] HUD rendering hook

**Validation:** Mode can be created and runs update

### M12.2: Deathmatch Mode
- [ ] Score tracking
- [ ] Kill limit or time limit
- [ ] Win condition check
- [ ] Score display HUD

**Validation:** Play deathmatch vs simple AI/self, score tracks, game ends

### M12.3: Capture the Flag Mode
- [ ] Flag entities
- [ ] Flag pickup, carrying, dropping
- [ ] Flag capture and return
- [ ] Score and win condition

**Validation:** Play CTF, can capture flags, game tracks score

### M12.4: Domination Mode
- [ ] Control zone entities
- [ ] Zone capture progress
- [ ] Points while holding
- [ ] Multi-zone support

**Validation:** Play domination, can capture zones, score accumulates

---

## Phase 13: Networking - Foundation
*Prepare for multiplayer (no lockstep requirement)*

### M13.1: Game State Serialization
- [ ] Serialize full game state to bytes (versioned)
- [ ] Deserialize back to game state
- [ ] Verify round-trip

**Validation:** Serialize, deserialize, compare states

### M13.2: Snapshot History + State Hash
- [ ] Store last N snapshots with tick numbers
- [ ] Compute lightweight state hash for regression and debugging
- [ ] Diff helper to report the first divergence in tests

**Validation:** 
- Replay with the same inputs yields identical hashes
- Divergence reports the tick where it started

### M13.3: Input Buffering for Network
- [ ] Queue `pz_tank_input` by tick (local + remote)
- [ ] Simulation consumes queued inputs, with neutral fallback
- [ ] Basic input delay simulation for testing

**Validation:** Run with artificial input delay, sim remains stable

---

## Phase 14: Networking - picoquic Integration
*Real network code*

### M14.1: picoquic Vendor
- [ ] Add picoquic as submodule/vendor
- [ ] Add picotls dependency
- [ ] Build picoquic with our project
- [ ] Link successfully

**Validation:** Project builds with picoquic

### M14.2: QUIC Server
- [ ] Create QUIC server context
- [ ] Generate self-signed cert
- [ ] Accept connections
- [ ] Callback on connect/disconnect

**Validation:** Server runs, client can connect (using picoquic test client)

### M14.3: QUIC Client
- [ ] Create QUIC client context
- [ ] Connect to server
- [ ] Handle connection callbacks

**Validation:** Our client connects to our server

### M14.4: Datagram Send/Receive
- [ ] Send datagram from client
- [ ] Receive datagram on server
- [ ] Send datagram from server
- [ ] Receive on client

**Validation:** Echo test - client sends, server echoes, client receives

### M14.5: Stream Send/Receive
- [ ] Open reliable stream
- [ ] Send data on stream
- [ ] Receive on other end
- [ ] Handle stream close

**Validation:** Send message on stream, receive correctly

### M14.6: Network Wrapper API
- [ ] Implement `pz_net_*` wrapper over picoquic
- [ ] Clean callback interface
- [ ] Connection management

**Validation:** Game code uses wrapper, not raw picoquic

---

## Phase 15: Networking - Game Protocol
*Multiplayer gameplay*

### M15.1: Connection Handshake
- [ ] Client sends connect request (name, team)
- [ ] Server validates, assigns client ID
- [ ] Server sends accept with game state
- [ ] Client enters game

**Validation:** Connect to server, see other player

### M15.2: Input Transmission
- [ ] Client sends input each tick
- [ ] Server buffers inputs
- [ ] Server applies inputs to simulation

**Validation:** Input reaches server (log on server)

### M15.3: State Broadcast
- [ ] Server serializes game delta
- [ ] Server sends to all clients
- [ ] Client receives and applies

**Validation:** Move on one client, see it on other

### M15.4: Client-Side Prediction
- [ ] Client predicts local tank
- [ ] Store input history
- [ ] Reconcile on server update

**Validation:** Responsive movement even with latency

### M15.5: Entity Interpolation
- [ ] Buffer server snapshots
- [ ] Interpolate other entities
- [ ] Render at interpolated positions

**Validation:** Other players move smoothly

### M15.6: Network Debug Tools
- [ ] Latency simulator
- [ ] Packet loss simulator
- [ ] Network debug overlay

**Validation:** Enable 200ms latency, game still playable

---

## Phase 16: Polish and Game Feel
*Make it feel good*

### M16.1: Screen Shake
- [ ] Camera shake on explosions
- [ ] Configurable intensity/duration

### M16.2: Particle System
- [ ] Simple particle emitter
- [ ] Muzzle flash
- [ ] Explosion particles
- [ ] Dust/debris

### M16.3: Hit Feedback
- [ ] Flash on damage
- [ ] Damage numbers (optional)
- [ ] Sound feedback

### M16.4: UI Polish
- [ ] Main menu
- [ ] Server browser / direct connect
- [ ] In-game HUD polish
- [ ] End-of-round screen

---

## Milestone Dependencies

```
M0.1 → M0.2 → M0.3 → M0.4
              ↓
M1.1 → M1.2 → M1.3 → M1.4 → M1.5 → M1.6
              ↓
M2.1 → M2.2 → M2.3 → M2.4 → M2.5
              ↓
M3.1 → M3.2 → M3.3 → M3.4 → M3.5
              ↓
M4.1 → M4.2 → M4.3 → M4.4 → M4.5 → M4.6 → M4.7
              ↓
M5.1 → M5.2 → M5.3 → M5.4
              ↓
M6.1 → M6.2 → M6.3 → M6.4 → M6.5
              ↓
        ┌─────┼─────┐
        ↓     ↓     ↓
      M7.x  M8.x  M9.x → M10.x
              ↓
            M11.x
              ↓
            M12.x
              ↓
M13.1 → M13.2 → M13.3
              ↓
M14.1 → M14.2 → M14.3 → M14.4 → M14.5 → M14.6
              ↓
M15.1 → M15.2 → M15.3 → M15.4 → M15.5 → M15.6
              ↓
            M16.x
```

---

## Testing Strategy

### Unit Tests (run on every build)
- Math functions
- Data structures
- Serialization round-trips
- Collision primitives
- Renderer API tests using null backend (no GL required)

### Integration Tests (run on CI)
- Map load/save
- Entity lifecycle
- Physics simulation
- Network protocol

### Manual Tests (each milestone)
- Visual verification
- Gameplay feel
- Debug tools work

### Regression Tests
- Record gameplay sessions
- Replay and verify determinism
- Snapshot comparison for rendering (per backend)

---

## Time Estimates (Rough)

| Phase | Milestones | Estimated Time |
|-------|------------|----------------|
| 0 | 4 | 1-2 hours |
| 1 | 6 | 4-6 hours |
| 2 | 5 | 4-6 hours |
| 3 | 5 | 4-6 hours |
| 4 | 7 | 6-8 hours |
| 5 | 4 | 3-4 hours |
| 6 | 5 | 4-6 hours |
| 7 | 3 | 2-3 hours |
| 8 | 3 | 2-3 hours |
| 9 | 6 | 4-6 hours |
| 10 | 4 | 3-4 hours |
| 11 | 6 | 6-8 hours |
| 12 | 4 | 4-6 hours |
| 13 | 3 | 2-3 hours |
| 14 | 6 | 8-12 hours |
| 15 | 6 | 8-12 hours |
| 16 | 4 | 4-6 hours |

**Total: ~66-96 hours** of focused development

---

## Next Steps

Ready to begin with **Phase 0: Project Setup**?

We'll create the directory structure, CMakeLists.txt, and get a window on screen.
