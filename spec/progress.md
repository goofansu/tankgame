# Progress

## Phase 0: Project Setup
- [x] M0.1: repo structure, CMake, builds on macOS
- [x] M0.2: Sokol app/gfx integrated, window opens/closes
- [x] M0.3: OpenGL 3.3 context, cornflower blue clear
- [x] M0.4: test framework, smoke tests pass

## Phase 1: Core Utilities
- [x] M1.1: memory system with leak detection
- [x] M1.2: logging system with levels/categories
- [x] M1.3: math library (vec2, vec3, vec4, mat4)
- [x] M1.4: data structures (list, array, hashmap)
- [x] M1.5: string utilities
- [x] M1.6: platform layer (timer, file I/O, paths)

## Phase 2: Rendering Foundation
- [x] M2.1: renderer API, null backend, GL33 backend, shaders, test triangle renders
- [x] M2.2: VAO/VBO helpers, colored triangle, textured quad
- [x] M2.3: texture loading (stb_image), texture manager with caching, hot-reload
- [x] M2.4: camera system, perspective projection, screen-to-world, ground grid renders
- [x] M2.5: debug overlay (bitmap font, FPS counter, frame time graph, F2 toggle)

## Phase 3: Game World Foundation
- [x] M3.1: ground rendering using map (wood textures, terrain types visible)
- [x] M3.2: map data structure (pz_map with terrain, height, spawns, coordinate conversion)
- [x] M3.3: terrain mesh batched by type, different textures per terrain
- [x] M3.4: 3D wall geometry from height map with lighting
- [x] M3.5: map hot-reload

## Quality Improvements (pre-Phase 4)
- [x] Wider map (24x14) for better 16:9 fit
- [x] MSAA (4x multisampling) for anti-aliased edges
- [x] Mipmapping on terrain textures for cleaner distant tiles
- [x] Camera auto-fit to show entire map
- [x] Fixed map parser to correctly load terrain types

## Phase 4: First Entity - The Tank (complete)
- [x] M4.1: tank body mesh, turret mesh, entity shader, renders at origin
- [x] M4.2: entity system (specialized managers: pz_tank_manager, pz_projectile_manager, pz_ai_manager)
- [x] M4.3: tank entity structure (pz_tank with health, loadout, rendering params)
- [x] M4.4: input system (WASD + mouse aiming) - frontloaded
- [x] M4.5: simulation timestep + determinism harness (pz_sim with fixed timestep, RNG, state hashing)
- [x] M4.6: tank movement (WASD control, mouse turret aim) - frontloaded
- [x] M4.7: tank-wall collision (separate axis, sliding along walls) - frontloaded

## Phase 6: Weapons and Projectiles (frontloaded)
- [x] M6.1: projectile entity, mesh, movement, lifetime
- [x] M6.2: tank firing (left mouse button, cooldown, spawn at turret tip)
- [x] M6.3: projectile-wall collision with bouncing (1 bounce)
- [x] M6.4: projectile-tank collision (damage, flash, tank system refactor)
- [ ] M6.5: tank death and respawn

## Phase 7: Track Accumulation (frontloaded)
- [x] M7.1: FBO accumulation texture, track rendering pipeline
- [x] M7.2: ground shader samples track texture using world coordinates
- [x] M7.3: track marks placed at distance intervals for each tread

## Dynamic Lighting System
- [x] 2D light map with shadow casting from walls and tanks
- [x] Spotlight lights attached to tanks (follow turret direction)
- [x] Multiple colored lights with additive blending
- [x] Shadows cast by rectangular occluders (walls, tanks)
- [x] Light map applied to ground, wall sides, tanks, and projectiles
- [x] Wall tops remain unlit by dynamic lights (ambient only)

## Bug Fixes
- [x] Fixed pz_map_set_tile to auto-set height for WALL tiles (test_map fix)

## UI/HUD System
- [x] SDF font rendering system (pz_font.h/c) using stb_truetype
- [x] Russo One and Caveat Brush fonts loaded
- [x] Player health display (bottom-right, color-coded by health %)

---

## Enemy AI System
- [x] M7B.1: Enemy types and spawn data (Level 1/2/3 enemies)
- [x] M7B.2: Level 1 enemy AI (stationary, aim + fire at player)
- [x] M7B.2+: Level 2 enemy AI (uses cover, peeks to fire, retreats)
- [x] M7B.3: Enemy death (no respawn)
- [ ] M7B.4: Campaign map 1 (1x Level 1 enemy)
- [x] M7B.5: Win condition (all enemies defeated)

## Phase 6 Completion
- [x] M6.5: Tank death and respawn (player respawns, enemies stay dead)

## Campaign System
- [x] Campaign file format (plain text, NAME/MAP/LIVES commands)
- [x] Campaign manager (load, start, advance, track lives)
- [x] Map session system (encapsulates all map-dependent state)
- [x] Level transitions (SPACE to advance, R to restart)
- [x] Lives system (lose life on death, game over when 0)
- [x] HUD updates (Level X/Y, Lives: N, enemies count)
- [x] State overlays (Level Complete, Game Over, Campaign Complete)
- [x] Main campaign with night_arena → day_arena

## Next Up
- [ ] Phase 5: Physics and Collision (M5.1-M5.4)
- [ ] Phase 8: Audio Foundation

## Plan Changes
- Networking phases (13-15) deferred
- Focus on single-player campaign with enemy AI
- Enemies defined in map files (type, position)
- 3 enemy types with different stats/behaviors
