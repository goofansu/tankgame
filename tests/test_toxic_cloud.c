/*
 * Tank Game - Toxic Cloud Tests
 */

#include "test_framework.h"

#include "../src/game/pz_toxic_cloud.h"

#define EPSILON 0.0001f

static pz_toxic_cloud *
create_test_cloud(void)
{
    pz_toxic_cloud_config config = { 0 };
    config.enabled = true;
    config.delay = 0.0f;
    config.duration = 10.0f;
    config.safe_zone_ratio = 0.20f;
    config.center = (pz_vec2) { 0.0f, 0.0f }; // defaults to map center
    return pz_toxic_cloud_create(&config, 20.0f, 10.0f);
}

TEST(toxic_boundary_progress)
{
    pz_toxic_cloud *cloud = create_test_cloud();
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    float radius = 0.0f;

    pz_toxic_cloud_get_boundary(cloud, &left, &right, &top, &bottom, &radius);
    ASSERT_NEAR(-10.0f, left, EPSILON);
    ASSERT_NEAR(10.0f, right, EPSILON);
    ASSERT_NEAR(-5.0f, top, EPSILON);
    ASSERT_NEAR(5.0f, bottom, EPSILON);
    ASSERT_NEAR(0.0f, radius, EPSILON);

    pz_toxic_cloud_update(cloud, 5.0f);
    pz_toxic_cloud_get_boundary(cloud, &left, &right, &top, &bottom, &radius);
    ASSERT_NEAR(-5.5f, left, EPSILON);
    ASSERT_NEAR(5.5f, right, EPSILON);
    ASSERT_NEAR(-3.0f, top, EPSILON);
    ASSERT_NEAR(3.0f, bottom, EPSILON);
    ASSERT_NEAR(1.5f, radius, EPSILON);

    pz_toxic_cloud_update(cloud, 5.0f);
    pz_toxic_cloud_get_boundary(cloud, &left, &right, &top, &bottom, &radius);
    ASSERT_NEAR(-1.0f, left, EPSILON);
    ASSERT_NEAR(1.0f, right, EPSILON);
    ASSERT_NEAR(-1.0f, top, EPSILON);
    ASSERT_NEAR(1.0f, bottom, EPSILON);
    ASSERT_NEAR(1.0f, radius, EPSILON);

    pz_toxic_cloud_destroy(cloud);
}

TEST(toxic_inside_checks)
{
    pz_toxic_cloud *cloud = create_test_cloud();
    pz_vec2 center = { 0.0f, 0.0f };
    pz_vec2 edge = { -10.0f, -5.0f };

    ASSERT(!pz_toxic_cloud_is_inside(cloud, edge));

    pz_toxic_cloud_update(cloud, 10.0f);
    ASSERT(!pz_toxic_cloud_is_inside(cloud, center));
    ASSERT(pz_toxic_cloud_is_inside(cloud, edge));
    ASSERT(pz_toxic_cloud_is_damaging(cloud, edge));

    pz_toxic_cloud_destroy(cloud);
}

TEST_MAIN();
