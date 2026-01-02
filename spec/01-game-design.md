# Game Design Document

## Overview

**Title:** TBD (working title: "Panzer Pit")
**Genre:** Top-down multiplayer arena shooter
**Players:** 2-8 players, 2-8 teams
**Platform:** Desktop (Windows, macOS, Linux) + Web (WebGL)

## Visual Style

- Near-top-down camera (~15-20° tilt from vertical)
- Subtle perspective projection
- Simple 3D geometry: extruded boxes for walls, simple meshes for tanks
- Minimal textures with flat colors and basic shading
- Dynamic lighting for muzzle flash, explosions, ambient
- Accumulated track marks, bullet holes, scorch marks on ground

## Core Gameplay

### Tank Controls
- **Movement:** WASD or arrow keys
- **Turret aim:** Mouse position (or right stick on gamepad)
- **Fire:** Left click / button
- **Drop mine:** Right click / button
- **Use item:** Spacebar / button

### Tank Properties

| Property | Value |
|----------|-------|
| Hit Points | 10 |
| Max Speed | TBD (tune later) |
| Acceleration | Near-instant (small ramp) |
| Turn Rate | Turn in place, instant-ish |
| Collision | Stop on contact (no pushing) |

### Weapons

**Default Cannon:**

| Property | Value |
|----------|-------|
| Damage | 5 |
| Speed | Slow |
| Max Bounces | 1 |
| Lifetime | 3 seconds |
| Fire Rate | ~1/second |

**Pickup Weapons (examples):**

| Weapon | Damage | Speed | Bounces | Special |
|--------|--------|-------|---------|---------|
| Rapid Fire | 2 | Fast | 0 | High fire rate |
| Bouncer | 3 | Medium | 4 | More bounces |
| Heavy Shell | 10 | Slow | 0 | One-shot kill |
| Shotgun | 2×5 | Fast | 0 | Spread pattern |

Weapons are temporary (ammo count or time limit).

### Mines

| Type | Trigger | Arm Time | Radius | Damage |
|------|---------|----------|--------|--------|
| Proximity | Distance | 1s | Small | 5 |
| Timed | Timer | N/A | Medium | 7 |

- Can be shot to detonate early
- Visible to all players (but small)
- Limited inventory (pick up more from map)

### Pickups

| Pickup | Effect | Duration/Amount |
|--------|--------|-----------------|
| Health Pack | Restore HP | +5 HP |
| Speed Boost | Increase max speed | 10 seconds |
| Ammo Box | Change weapon type | 10 shots or 15 seconds |
| Mine Pack | Add mines to inventory | +3 mines |
| Crate Placer | Place destructible crates | 3 crates |

Pickups respawn after configurable delay (set per-pickup in map).

### Destructibles

- **Crates:** 5 HP, block movement and projectiles, leave debris
- **Barrels:** 3 HP, explode when destroyed (damage nearby)
- Player-placed crates are identical to map crates

### Terrain Types

| Terrain | Effect |
|---------|--------|
| Ground | Normal movement |
| Water | Impassable |
| Mud | Slow movement (50%) |
| Ice | Reduced friction (drift) |

## Game Modes

### Deathmatch
- Free-for-all or team-based
- First to X kills, or most kills in time limit
- Respawn after death (configurable delay)

### Capture the Flag
- 2 teams
- Grab enemy flag, return to your base
- First to X captures wins
- Flag drops on death, returns after timeout

### Domination
- 1-3 control zones on map
- Stand in zone to capture (progress bar)
- Earn points while controlling
- First to X points wins

## Map Requirements

Maps must define:
- Size (tile dimensions)
- Tile/terrain layout
- Wall and obstacle placement
- Destructible positions
- Spawn points (per team + FFA spawns)
- Pickup spawn locations with types and respawn timers
- Game mode specific: flag positions, control zones

## UI/HUD

- Health bar
- Current weapon + ammo count
- Mine inventory count
- Minimap (optional, maybe togglable)
- Score/objective display
- Kill feed
