# Gameplay Tech

## Entity System

Simple, not full ECS - just typed entities with common patterns:

```c
// Base for all game entities
typedef struct pz_entity {
    pz_list_node node;           // For entity lists
    pz_entity_type type;
    uint32_t     id;
    pz_vec2      pos;
    float        angle;
    bool         active;
    bool         marked_destroy;
} pz_entity;

// Entity manager
typedef struct pz_entity_manager {
    pz_list      all_entities;
    pz_list      tanks;
    pz_list      projectiles;
    pz_list      mines;
    pz_list      pickups;
    pz_list      destructibles;
    uint32_t     next_id;
} pz_entity_manager;
```

## Tank

```c
typedef struct pz_tank {
    pz_entity    base;
    
    // Gameplay
    int          team;
    int          hp;
    pz_weapon_type weapon;
    int          ammo;
    int          mine_count;
    int          crate_count;
    
    // Movement
    pz_vec2      velocity;
    float        turret_angle;
    float        speed_multiplier;   // For speed boost
    
    // Timers
    float        fire_cooldown;
    float        speed_boost_timer;
    float        invuln_timer;       // After respawn
    
    // Visual
    float        tread_accum;        // For track placement
    
    // Network
    int          owner_client_id;
} pz_tank;

pz_tank* pz_tank_create(pz_entity_manager* em, pz_vec2 pos, int team);
void     pz_tank_update(pz_tank* tank, const pz_tank_input* input, float dt);
void     pz_tank_damage(pz_tank* tank, int amount, pz_tank* attacker);
void     pz_tank_fire(pz_tank* tank, pz_entity_manager* em);
void     pz_tank_drop_mine(pz_tank* tank, pz_entity_manager* em);
```

## Projectile

```c
typedef struct pz_projectile {
    pz_entity    base;
    
    int          owner_tank_id;
    int          damage;
    float        speed;
    int          bounces_remaining;
    float        lifetime;
    pz_vec2      velocity;
} pz_projectile;

void pz_projectile_update(pz_projectile* proj, pz_map* map, pz_entity_manager* em, float dt);
```

Bouncing logic:
```c
// On wall collision:
pz_vec2 normal = pz_map_get_wall_normal(map, proj->base.pos, proj->velocity);
proj->velocity = pz_vec2_reflect(proj->velocity, normal);
proj->bounces_remaining--;
if (proj->bounces_remaining < 0) {
    proj->base.marked_destroy = true;
}
```

## Mine

```c
typedef struct pz_mine {
    pz_entity    base;
    
    pz_mine_type type;           // PROXIMITY, TIMED
    int          owner_tank_id;
    int          damage;
    float        arm_timer;
    float        fuse_timer;     // For timed mines
    float        trigger_radius;
    float        explosion_radius;
    bool         armed;
} pz_mine;

void pz_mine_update(pz_mine* mine, pz_entity_manager* em, float dt);
void pz_mine_detonate(pz_mine* mine, pz_entity_manager* em);
```

## Pickup

```c
typedef struct pz_pickup {
    pz_entity     base;
    
    pz_pickup_type type;
    float         respawn_time;
    float         respawn_timer;
    bool          available;
    
    // Type-specific data
    union {
        struct { pz_weapon_type weapon; int ammo; } ammo;
        struct { int amount; } health;
        struct { float duration; } speed_boost;
        struct { int count; } mines;
        struct { int count; } crates;
    };
} pz_pickup;
```

## Destructible

```c
typedef struct pz_destructible {
    pz_entity           base;
    
    pz_destructible_type type;      // CRATE, BARREL
    int                 hp;
    bool                explodes;   // Barrels
    int                 explosion_damage;
    float               explosion_radius;
} pz_destructible;
```

## Map System

### Map File Format

```
# Tank Pit - Desert Arena
# Comments start with #

MAP desert_arena
VERSION 1
SIZE 32 32
TILE_SIZE 1.0

# Tilesets define texture atlas regions
TILESET
  ground: sand.png 0 0 64 64
  wall: brick.png 0 0 64 64
  water: water.png 0 0 64 64

# Terrain layer (32x32)
# . = ground, # = wall, ~ = water, : = mud, * = ice
TERRAIN
################################
#..............................#
#..............................#
#...####......####......####...#
#...#..#......#..#......#..#...#
#...#..#......#..#......#..#...#
#...####......####......####...#
#..............................#
#..~~~~~~........~~~~~~........#
#..~~~~~~........~~~~~~........#
#..............................#
#..............................#
################################

# Height layer (0-9, how tall walls are)
HEIGHT
################################
#000000000000000000000000000000#
#000000000000000000000000000000#
#000222200000022220000002222000#
#000200200000020020000002002000#
#000200200000020020000002002000#
#000222200000022220000002222000#
#000000000000000000000000000000#
#000000000000000000000000000000#
#000000000000000000000000000000#
#000000000000000000000000000000#
#000000000000000000000000000000#
################################

# Spawns: type team x y angle
SPAWN ffa 0 5 5 0
SPAWN ffa 0 27 5 180
SPAWN ffa 0 5 27 0
SPAWN ffa 0 27 27 180
SPAWN team 1 5 16 0
SPAWN team 2 27 16 180

# Pickups: type x y [respawn_time] [extra params]
PICKUP health 16 16 30
PICKUP ammo 8 8 20 weapon=rapid
PICKUP ammo 24 24 20 weapon=bouncer
PICKUP speed 16 8 45
PICKUP mines 8 24 30 count=3

# Destructibles: type x y
DESTRUCTIBLE crate 10 10
DESTRUCTIBLE crate 22 10
DESTRUCTIBLE barrel 16 20

# Game mode objects
FLAG team_1 3 16
FLAG team_2 29 16
ZONE domination 16 16 4 4
```

### Map Runtime Structure

```c
typedef struct pz_map {
    char         name[64];
    int          width;
    int          height;
    float        tile_size;
    
    // Layers
    pz_tile_type* terrain;        // width * height
    uint8_t*     height_map;      // For walls
    
    // Entities (loaded into pz_entity_manager)
    pz_spawn_point* spawns;
    int          spawn_count;
    
    // Mode-specific
    pz_vec2      flag_positions[PZ_MAX_TEAMS];
    pz_zone*     zones;
    int          zone_count;
    
    // Rendering
    GLuint       ground_texture;
    pz_mesh*     wall_mesh;       // Generated from height map
    
    // Hot-reload
    char         filepath[256];
    time_t       last_modified;
} pz_map;

pz_map* pz_map_load(const char* path);
void    pz_map_unload(pz_map* map);
bool    pz_map_check_reload(pz_map* map);  // Returns true if reloaded
void    pz_map_render(pz_map* map, pz_renderer* renderer);

// Collision queries
bool    pz_map_is_solid(pz_map* map, pz_vec2 pos);
bool    pz_map_line_of_sight(pz_map* map, pz_vec2 from, pz_vec2 to);
pz_vec2 pz_map_get_wall_normal(pz_map* map, pz_vec2 pos, pz_vec2 dir);
```

## Physics / Collision

Simple 2D collision (all top-down):

```c
typedef struct pz_collision_world {
    pz_map*            map;
    pz_entity_manager* entities;
} pz_collision_world;

// Collision shapes (all 2D)
typedef enum { 
    PZ_SHAPE_CIRCLE, 
    PZ_SHAPE_RECT 
} pz_shape_type;

typedef struct pz_collider {
    pz_shape_type type;
    union {
        struct { float radius; } circle;
        struct { float width, height; } rect;
    };
} pz_collider;

// Queries
bool pz_collision_test(pz_collision_world* world, pz_vec2 pos, pz_collider* collider, uint32_t mask);
bool pz_collision_raycast(pz_collision_world* world, pz_vec2 from, pz_vec2 to, uint32_t mask, pz_ray_hit* hit);
void pz_collision_resolve_tank(pz_collision_world* world, pz_tank* tank); // Push out of walls
```

Collision layers/masks:
```c
enum {
    PZ_COLLISION_WALL         = 1 << 0,
    PZ_COLLISION_WATER        = 1 << 1,
    PZ_COLLISION_TANK         = 1 << 2,
    PZ_COLLISION_DESTRUCTIBLE = 1 << 3,
    PZ_COLLISION_MINE         = 1 << 4,
};
```

## Game Modes

```c
typedef struct pz_game_mode {
    pz_game_mode_type type;
    
    // Virtual functions
    void (*init)(pz_game_mode* mode, pz_game* game);
    void (*update)(pz_game_mode* mode, pz_game* game, float dt);
    void (*on_kill)(pz_game_mode* mode, pz_game* game, pz_tank* killer, pz_tank* victim);
    void (*on_flag_taken)(pz_game_mode* mode, pz_game* game, int team, pz_tank* taker);
    void (*on_flag_captured)(pz_game_mode* mode, pz_game* game, int team);
    void (*on_zone_captured)(pz_game_mode* mode, pz_game* game, int zone_id, int team);
    bool (*is_finished)(pz_game_mode* mode, pz_game* game);
    int  (*get_winner)(pz_game_mode* mode, pz_game* game);
    void (*render_hud)(pz_game_mode* mode, pz_game* game, pz_renderer* renderer);
} pz_game_mode;

// Mode-specific state
typedef struct pz_deathmatch_mode {
    pz_game_mode base;
    int          scores[PZ_MAX_TEAMS];
    int          kill_limit;
    float        time_limit;
    float        time_remaining;
} pz_deathmatch_mode;

typedef struct pz_ctf_mode {
    pz_game_mode  base;
    int           scores[2];
    int           capture_limit;
    pz_flag_state flags[2];
} pz_ctf_mode;

typedef struct pz_domination_mode {
    pz_game_mode  base;
    float         scores[PZ_MAX_TEAMS];
    float         score_limit;
    pz_zone_state zones[PZ_MAX_ZONES];
} pz_domination_mode;
```

## Camera

```c
typedef struct pz_camera {
    pz_vec3      position;
    pz_vec3      target;         // Look-at point
    float        fov;
    float        near, far;
    
    // For screen-to-world conversion
    pz_mat4      view;
    pz_mat4      projection;
    pz_mat4      view_projection;
    pz_mat4      inverse_vp;
} pz_camera;

void    pz_camera_init(pz_camera* cam, float map_width, float map_height);
void    pz_camera_update(pz_camera* cam, float aspect_ratio);
pz_vec2 pz_camera_screen_to_world(pz_camera* cam, pz_vec2 screen_pos, pz_vec2 screen_size);
```

The camera is fixed over the map center, looking down at the ~15-20° angle.

## Editor

```c
typedef struct pz_editor {
    bool            active;
    pz_map*         map;
    
    // Tools
    pz_editor_tool  current_tool;   // PAINT, ERASE, PLACE_SPAWN, etc.
    pz_tile_type    selected_tile;
    int             brush_size;
    
    // Selection
    int             selected_entity;
    
    // UI state
    bool            show_grid;
    bool            show_spawns;
    bool            show_pickups;
} pz_editor;

void pz_editor_init(pz_editor* ed, pz_map* map);
void pz_editor_update(pz_editor* ed, pz_input* input, float dt);
void pz_editor_render(pz_editor* ed, pz_renderer* renderer);
void pz_editor_save(pz_editor* ed);   // Saves to map->filepath
```

Toggle with F1, immediate hot-reload on save.
