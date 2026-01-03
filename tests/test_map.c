/*
 * Map system tests
 */

#include "../src/game/pz_map.h"
#include "test_framework.h"

TEST(map_create)
{
    pz_map *map = pz_map_create(16, 16, 2.0f);
    ASSERT_NOT_NULL(map);
    ASSERT_EQ(16, map->width);
    ASSERT_EQ(16, map->height);
    ASSERT_NEAR(2.0f, map->tile_size, 0.01f);
    ASSERT_NEAR(32.0f, map->world_width, 0.01f);
    ASSERT_NEAR(32.0f, map->world_height, 0.01f);

    // Should have default tile definitions (ground, stone)
    ASSERT(map->tile_def_count >= 2);

    // Default cells should be ground at height 0
    pz_map_cell cell = pz_map_get_cell(map, 0, 0);
    ASSERT_EQ(0, cell.height);
    ASSERT_EQ(0, cell.tile_index); // ground

    cell = pz_map_get_cell(map, 8, 8);
    ASSERT_EQ(0, cell.height);

    pz_map_destroy(map);
}

TEST(map_cell_access)
{
    pz_map *map = pz_map_create(8, 8, 1.0f);
    ASSERT_NOT_NULL(map);

    // Add tile definitions
    int mud_idx = pz_map_add_tile_def(map, ':', "mud");
    int ice_idx = pz_map_add_tile_def(map, '*', "ice");
    ASSERT(mud_idx >= 0);
    ASSERT(ice_idx >= 0);

    // Set and get cells
    pz_map_set_cell(map, 0, 0,
        (pz_map_cell) { .height = 2, .tile_index = 1 }); // stone wall
    pz_map_set_cell(map, 1, 1,
        (pz_map_cell) { .height = 0, .tile_index = (uint8_t)mud_idx });
    pz_map_set_cell(
        map, 2, 2, (pz_map_cell) { .height = -1, .tile_index = 0 }); // pit
    pz_map_set_cell(map, 3, 3,
        (pz_map_cell) { .height = 0, .tile_index = (uint8_t)ice_idx });

    pz_map_cell cell = pz_map_get_cell(map, 0, 0);
    ASSERT_EQ(2, cell.height);
    ASSERT_EQ(1, cell.tile_index);

    cell = pz_map_get_cell(map, 1, 1);
    ASSERT_EQ(0, cell.height);
    ASSERT_EQ(mud_idx, cell.tile_index);

    cell = pz_map_get_cell(map, 2, 2);
    ASSERT_EQ(-1, cell.height);

    cell = pz_map_get_cell(map, 3, 3);
    ASSERT_EQ(0, cell.height);
    ASSERT_EQ(ice_idx, cell.tile_index);

    // Out of bounds returns high wall
    cell = pz_map_get_cell(map, -1, 0);
    ASSERT(cell.height > 0); // Should be solid

    pz_map_destroy(map);
}

TEST(map_height)
{
    pz_map *map = pz_map_create(8, 8, 1.0f);
    ASSERT_NOT_NULL(map);

    // Default height is 0
    ASSERT_EQ(0, pz_map_get_height(map, 0, 0));

    // Set and get height
    pz_map_set_height(map, 0, 0, 2);
    pz_map_set_height(map, 1, 1, 5);
    pz_map_set_height(map, 2, 2, -1); // pit

    ASSERT_EQ(2, pz_map_get_height(map, 0, 0));
    ASSERT_EQ(5, pz_map_get_height(map, 1, 1));
    ASSERT_EQ(-1, pz_map_get_height(map, 2, 2));

    pz_map_destroy(map);
}

TEST(map_tile_defs)
{
    pz_map *map = pz_map_create(8, 8, 1.0f);
    ASSERT_NOT_NULL(map);

    // Check default tile defs
    int ground_idx = pz_map_find_tile_def(map, '.');
    int stone_idx = pz_map_find_tile_def(map, '#');
    ASSERT(ground_idx >= 0);
    ASSERT(stone_idx >= 0);

    const pz_tile_def *ground = pz_map_get_tile_def_by_index(map, ground_idx);
    const pz_tile_def *stone = pz_map_get_tile_def_by_index(map, stone_idx);
    ASSERT_NOT_NULL(ground);
    ASSERT_NOT_NULL(stone);
    ASSERT_EQ('.', ground->symbol);
    ASSERT_EQ('#', stone->symbol);

    // Add custom tile def
    int lava_idx = pz_map_add_tile_def(map, 'L', "lava");
    ASSERT(lava_idx >= 0);
    ASSERT_EQ(lava_idx, pz_map_find_tile_def(map, 'L'));

    // Unknown tile returns -1
    ASSERT_EQ(-1, pz_map_find_tile_def(map, 'X'));

    pz_map_destroy(map);
}

TEST(map_coordinate_conversion)
{
    // 8x8 map with 2.0 unit tiles = 16x16 world centered at origin
    pz_map *map = pz_map_create(8, 8, 2.0f);
    ASSERT_NOT_NULL(map);

    // Tile (0,0) center should be at (-7, -7) = bottom-left quadrant
    pz_vec2 world = pz_map_tile_to_world(map, 0, 0);
    ASSERT_NEAR(-7.0f, world.x, 0.01f);
    ASSERT_NEAR(-7.0f, world.y, 0.01f);

    // Tile (4,4) center should be at (1, 1) = near center
    world = pz_map_tile_to_world(map, 4, 4);
    ASSERT_NEAR(1.0f, world.x, 0.01f);
    ASSERT_NEAR(1.0f, world.y, 0.01f);

    // World-to-tile conversion
    int tx, ty;
    pz_map_world_to_tile(map, (pz_vec2) { -7.0f, -7.0f }, &tx, &ty);
    ASSERT_EQ(0, tx);
    ASSERT_EQ(0, ty);

    pz_map_world_to_tile(map, (pz_vec2) { 0.0f, 0.0f }, &tx, &ty);
    ASSERT_EQ(4, tx);
    ASSERT_EQ(4, ty);

    pz_map_destroy(map);
}

TEST(map_solid_check)
{
    pz_map *map = pz_map_create(8, 8, 2.0f);
    ASSERT_NOT_NULL(map);

    pz_vec2 center = pz_map_tile_to_world(map, 4, 4);

    // Height 0 is passable
    pz_map_set_height(map, 4, 4, 0);
    ASSERT(!pz_map_is_solid(map, center));
    ASSERT(pz_map_is_passable(map, center));

    // Height > 0 (wall) is solid
    pz_map_set_height(map, 4, 4, 2);
    ASSERT(pz_map_is_solid(map, center));
    ASSERT(!pz_map_is_passable(map, center));

    // Height < 0 (pit) is also solid for tanks
    pz_map_set_height(map, 4, 4, -1);
    ASSERT(pz_map_is_solid(map, center));

    // But bullets can fly over pits
    ASSERT(!pz_map_blocks_bullets(map, center));

    // Walls block bullets
    pz_map_set_height(map, 4, 4, 2);
    ASSERT(pz_map_blocks_bullets(map, center));

    pz_map_destroy(map);
}

TEST(map_speed_multiplier)
{
    pz_map *map = pz_map_create(8, 8, 2.0f);
    ASSERT_NOT_NULL(map);

    // Add terrain types
    int mud_idx = pz_map_add_tile_def(map, ':', "mud");
    int ice_idx = pz_map_add_tile_def(map, '*', "ice");

    pz_vec2 center = pz_map_tile_to_world(map, 4, 4);

    // Ground = normal speed
    pz_map_set_cell(map, 4, 4, (pz_map_cell) { .height = 0, .tile_index = 0 });
    ASSERT_NEAR(1.0f, pz_map_get_speed_multiplier(map, center), 0.01f);

    // Mud = half speed
    pz_map_set_cell(map, 4, 4,
        (pz_map_cell) { .height = 0, .tile_index = (uint8_t)mud_idx });
    ASSERT_NEAR(0.5f, pz_map_get_speed_multiplier(map, center), 0.01f);

    // Ice = slightly faster
    pz_map_set_cell(map, 4, 4,
        (pz_map_cell) { .height = 0, .tile_index = (uint8_t)ice_idx });
    ASSERT_NEAR(1.2f, pz_map_get_speed_multiplier(map, center), 0.01f);

    // Wall = impassable
    pz_map_set_cell(map, 4, 4, (pz_map_cell) { .height = 2, .tile_index = 1 });
    ASSERT_NEAR(0.0f, pz_map_get_speed_multiplier(map, center), 0.01f);

    pz_map_destroy(map);
}

TEST(map_bounds)
{
    pz_map *map = pz_map_create(8, 8, 2.0f);
    ASSERT_NOT_NULL(map);

    // Tile bounds
    ASSERT(pz_map_in_bounds(map, 0, 0));
    ASSERT(pz_map_in_bounds(map, 7, 7));
    ASSERT(!pz_map_in_bounds(map, -1, 0));
    ASSERT(!pz_map_in_bounds(map, 8, 0));
    ASSERT(!pz_map_in_bounds(map, 0, 8));

    // World bounds (16x16 centered at origin = -8 to +8)
    ASSERT(pz_map_in_bounds_world(map, (pz_vec2) { 0, 0 }));
    ASSERT(pz_map_in_bounds_world(map, (pz_vec2) { -7.9f, -7.9f }));
    ASSERT(pz_map_in_bounds_world(map, (pz_vec2) { 7.9f, 7.9f }));
    ASSERT(!pz_map_in_bounds_world(map, (pz_vec2) { -8.1f, 0 }));
    ASSERT(!pz_map_in_bounds_world(map, (pz_vec2) { 8.1f, 0 }));

    pz_map_destroy(map);
}

TEST(map_test_creation)
{
    pz_map *map = pz_map_create_test();
    ASSERT_NOT_NULL(map);
    ASSERT_EQ(16, map->width);
    ASSERT_EQ(16, map->height);

    // Border should be walls (height > 0)
    ASSERT(pz_map_get_height(map, 0, 0) > 0);
    ASSERT(pz_map_get_height(map, 15, 0) > 0);
    ASSERT(pz_map_get_height(map, 0, 15) > 0);
    ASSERT(pz_map_get_height(map, 15, 15) > 0);

    // Interior should have some ground (height = 0)
    ASSERT_EQ(0, pz_map_get_height(map, 1, 1));

    // Should have spawn points
    ASSERT_EQ(4, map->spawn_count);

    // Print for visual verification
    printf("\n");
    pz_map_print(map);

    pz_map_destroy(map);
}

TEST(map_v2_format)
{
    // Test loading a v2 format map
    pz_map *map = pz_map_load("assets/maps/test_arena.map");
    if (!map) {
        printf(
            "Note: Skipping v2 format test (map file not found in test env)\n");
        return;
    }

    ASSERT_EQ(2, map->version);
    ASSERT_EQ(24, map->width);
    ASSERT_EQ(14, map->height);

    // Check tile definitions were loaded
    ASSERT(map->tile_def_count >= 2);
    ASSERT(pz_map_find_tile_def(map, '.') >= 0);
    ASSERT(pz_map_find_tile_def(map, '#') >= 0);

    // Border should be walls
    ASSERT(pz_map_get_height(map, 0, 0) > 0);
    ASSERT(pz_map_get_height(map, 23, 0) > 0);

    // Should have spawns from tags
    ASSERT(map->spawn_count > 0);

    // Should have enemies from tags
    ASSERT(map->enemy_count > 0);

    pz_map_destroy(map);
}

TEST_MAIN()
