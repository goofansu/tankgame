# Progress

## Phase 0: Project Setup
- [x] M0.1: repo structure, CMake, builds on macOS
- [x] M0.2: SDL2 integrated, window opens/closes
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

## Phase 4: First Entity - The Tank (partial, frontloaded gameplay)
- [x] M4.1: tank body mesh, turret mesh, entity shader, renders at origin
- [x] M4.4: input system (WASD + mouse aiming) - frontloaded
- [x] M4.6: tank movement (WASD control, mouse turret aim) - frontloaded
- [x] M4.7: tank-wall collision (separate axis, sliding along walls) - frontloaded
- [ ] M4.2: entity system foundation (formal entity manager)
- [ ] M4.3: tank entity structure (formal component)
- [ ] M4.5: simulation timestep + determinism harness

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

## Bug Fixes
- [x] Fixed pz_map_set_tile to auto-set height for WALL tiles (test_map fix)

---

## Next Up
- [ ] M6.5: Tank death and respawn
- [ ] M7B.1: Enemy types and spawn data (Level 1/2/3 enemies)
- [ ] M7B.2: Stationary enemy AI (aim + fire at player)
- [ ] M7B.3: Enemy death (no respawn)
- [ ] M7B.4: Campaign map 1 (1x Level 1 enemy)
- [ ] M7B.5: Win condition (all enemies defeated)

## Plan Changes
- Networking phases (13-15) deferred
- Focus on single-player campaign with enemy AI
- Enemies defined in map files (type, position)
- 3 enemy types with different stats/behaviors
