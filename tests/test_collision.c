/*
 * Tank Game - Collision primitive tests
 */

#include "test_framework.h"

#include "../src/game/pz_collision.h"

TEST(circle_circle_overlap)
{
    pz_circle a = pz_circle_new((pz_vec2) { 0.0f, 0.0f }, 1.0f);
    pz_circle b = pz_circle_new((pz_vec2) { 1.5f, 0.0f }, 1.0f);
    pz_vec2 normal = { 0.0f, 0.0f };
    float penetration = 0.0f;

    bool hit = pz_collision_circle_circle(a, b, &normal, &penetration);
    ASSERT(hit);
    ASSERT_NEAR(1.0f, normal.x, 0.0001f);
    ASSERT_NEAR(0.0f, normal.y, 0.0001f);
    ASSERT_NEAR(0.5f, penetration, 0.0001f);
}

TEST(circle_circle_same_center)
{
    pz_circle a = pz_circle_new((pz_vec2) { 0.0f, 0.0f }, 1.0f);
    pz_circle b = pz_circle_new((pz_vec2) { 0.0f, 0.0f }, 2.0f);
    pz_vec2 normal = { 0.0f, 0.0f };
    float penetration = 0.0f;

    bool hit = pz_collision_circle_circle(a, b, &normal, &penetration);
    ASSERT(hit);
    ASSERT_NEAR(1.0f, normal.x, 0.0001f);
    ASSERT_NEAR(0.0f, normal.y, 0.0001f);
    ASSERT_NEAR(3.0f, penetration, 0.0001f);
}

TEST(circle_aabb_no_overlap)
{
    pz_circle circle = pz_circle_new((pz_vec2) { 0.0f, 0.0f }, 1.0f);
    pz_aabb box
        = pz_aabb_new((pz_vec2) { 2.0f, -1.0f }, (pz_vec2) { 4.0f, 1.0f });

    bool hit = pz_collision_circle_aabb(circle, box, NULL);
    ASSERT(!hit);
}

TEST(circle_aabb_overlap)
{
    pz_circle circle = pz_circle_new((pz_vec2) { 1.5f, 0.0f }, 1.0f);
    pz_aabb box
        = pz_aabb_new((pz_vec2) { 2.0f, -1.0f }, (pz_vec2) { 4.0f, 1.0f });
    pz_vec2 push_out = { 0.0f, 0.0f };

    bool hit = pz_collision_circle_aabb(circle, box, &push_out);
    ASSERT(hit);
    ASSERT_NEAR(-0.5f, push_out.x, 0.0001f);
    ASSERT_NEAR(0.0f, push_out.y, 0.0001f);
}

TEST(circle_aabb_inside)
{
    pz_circle circle = pz_circle_new((pz_vec2) { 0.0f, 0.0f }, 0.5f);
    pz_aabb box
        = pz_aabb_new((pz_vec2) { -1.0f, -1.0f }, (pz_vec2) { 1.0f, 1.0f });
    pz_vec2 push_out = { 0.0f, 0.0f };

    bool hit = pz_collision_circle_aabb(circle, box, &push_out);
    ASSERT(hit);
    ASSERT_NEAR(-1.5f, push_out.x, 0.0001f);
    ASSERT_NEAR(0.0f, push_out.y, 0.0001f);
}

TEST(aabb_aabb_overlap)
{
    pz_aabb a = pz_aabb_new((pz_vec2) { 0.0f, 0.0f }, (pz_vec2) { 1.0f, 1.0f });
    pz_aabb b = pz_aabb_new((pz_vec2) { 0.5f, 0.5f }, (pz_vec2) { 2.0f, 2.0f });

    ASSERT(pz_collision_aabb_aabb(a, b));
}

TEST(aabb_aabb_no_overlap)
{
    pz_aabb a = pz_aabb_new((pz_vec2) { 0.0f, 0.0f }, (pz_vec2) { 1.0f, 1.0f });
    pz_aabb b = pz_aabb_new((pz_vec2) { 2.0f, 2.0f }, (pz_vec2) { 3.0f, 3.0f });

    ASSERT(!pz_collision_aabb_aabb(a, b));
}

TEST_MAIN();
