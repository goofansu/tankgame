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

    // Default tiles should be ground
    ASSERT_EQ(PZ_TILE_GROUND, pz_map_get_tile(map, 0, 0));
    ASSERT_EQ(PZ_TILE_GROUND, pz_map_get_tile(map, 8, 8));

    pz_map_destroy(map);
}

TEST(map_tile_access)
{
    pz_map *map = pz_map_create(8, 8, 1.0f);
    ASSERT_NOT_NULL(map);

    // Set and get tiles
    pz_map_set_tile(map, 0, 0, PZ_TILE_WALL);
    pz_map_set_tile(map, 1, 1, PZ_TILE_WATER);
    pz_map_set_tile(map, 2, 2, PZ_TILE_MUD);
    pz_map_set_tile(map, 3, 3, PZ_TILE_ICE);

    ASSERT_EQ(PZ_TILE_WALL, pz_map_get_tile(map, 0, 0));
    ASSERT_EQ(PZ_TILE_WATER, pz_map_get_tile(map, 1, 1));
    ASSERT_EQ(PZ_TILE_MUD, pz_map_get_tile(map, 2, 2));
    ASSERT_EQ(PZ_TILE_ICE, pz_map_get_tile(map, 3, 3));

    // Out of bounds returns WALL (solid)
    ASSERT_EQ(PZ_TILE_WALL, pz_map_get_tile(map, -1, 0));
    ASSERT_EQ(PZ_TILE_WALL, pz_map_get_tile(map, 0, -1));
    ASSERT_EQ(PZ_TILE_WALL, pz_map_get_tile(map, 100, 0));

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

    ASSERT_EQ(2, pz_map_get_height(map, 0, 0));
    ASSERT_EQ(5, pz_map_get_height(map, 1, 1));

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

    // Ground is passable
    pz_map_set_tile(map, 4, 4, PZ_TILE_GROUND);
    pz_vec2 center = pz_map_tile_to_world(map, 4, 4);
    ASSERT(!pz_map_is_solid(map, center));
    ASSERT(pz_map_is_passable(map, center));

    // Wall is solid
    pz_map_set_tile(map, 4, 4, PZ_TILE_WALL);
    ASSERT(pz_map_is_solid(map, center));
    ASSERT(!pz_map_is_passable(map, center));

    // Water is solid (impassable)
    pz_map_set_tile(map, 4, 4, PZ_TILE_WATER);
    ASSERT(pz_map_is_solid(map, center));

    // Mud is passable (slow)
    pz_map_set_tile(map, 4, 4, PZ_TILE_MUD);
    ASSERT(!pz_map_is_solid(map, center));

    // Ice is passable
    pz_map_set_tile(map, 4, 4, PZ_TILE_ICE);
    ASSERT(!pz_map_is_solid(map, center));

    pz_map_destroy(map);
}

TEST(map_speed_multiplier)
{
    pz_map *map = pz_map_create(8, 8, 2.0f);
    ASSERT_NOT_NULL(map);

    pz_vec2 center = pz_map_tile_to_world(map, 4, 4);

    // Ground = normal speed
    pz_map_set_tile(map, 4, 4, PZ_TILE_GROUND);
    ASSERT_NEAR(1.0f, pz_map_get_speed_multiplier(map, center), 0.01f);

    // Mud = half speed
    pz_map_set_tile(map, 4, 4, PZ_TILE_MUD);
    ASSERT_NEAR(0.5f, pz_map_get_speed_multiplier(map, center), 0.01f);

    // Ice = slightly faster (but will have drift physics)
    pz_map_set_tile(map, 4, 4, PZ_TILE_ICE);
    ASSERT_NEAR(1.2f, pz_map_get_speed_multiplier(map, center), 0.01f);

    // Wall/water = impassable
    pz_map_set_tile(map, 4, 4, PZ_TILE_WALL);
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

    // Border should be walls
    ASSERT_EQ(PZ_TILE_WALL, pz_map_get_tile(map, 0, 0));
    ASSERT_EQ(PZ_TILE_WALL, pz_map_get_tile(map, 15, 0));
    ASSERT_EQ(PZ_TILE_WALL, pz_map_get_tile(map, 0, 15));
    ASSERT_EQ(PZ_TILE_WALL, pz_map_get_tile(map, 15, 15));

    // Interior should have some ground
    ASSERT_EQ(PZ_TILE_GROUND, pz_map_get_tile(map, 1, 1));

    // Should have spawn points
    ASSERT_EQ(4, map->spawn_count);

    // Print for visual verification
    printf("\n");
    pz_map_print(map);

    pz_map_destroy(map);
}

TEST_MAIN()
