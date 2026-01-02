/*
 * Tank Game - Math Library Tests
 */

#include "../src/core/pz_math.h"
#include "test_framework.h"

#define EPSILON 0.0001f

// ============================================================================
// vec2 tests
// ============================================================================

TEST(vec2_add)
{
    pz_vec2 a = pz_vec2_new(1.0f, 2.0f);
    pz_vec2 b = pz_vec2_new(3.0f, 4.0f);
    pz_vec2 r = pz_vec2_add(a, b);
    ASSERT_NEAR(4.0f, r.x, EPSILON);
    ASSERT_NEAR(6.0f, r.y, EPSILON);
}

TEST(vec2_sub)
{
    pz_vec2 a = pz_vec2_new(5.0f, 7.0f);
    pz_vec2 b = pz_vec2_new(2.0f, 3.0f);
    pz_vec2 r = pz_vec2_sub(a, b);
    ASSERT_NEAR(3.0f, r.x, EPSILON);
    ASSERT_NEAR(4.0f, r.y, EPSILON);
}

TEST(vec2_scale)
{
    pz_vec2 v = pz_vec2_new(2.0f, 3.0f);
    pz_vec2 r = pz_vec2_scale(v, 2.0f);
    ASSERT_NEAR(4.0f, r.x, EPSILON);
    ASSERT_NEAR(6.0f, r.y, EPSILON);
}

TEST(vec2_dot)
{
    pz_vec2 a = pz_vec2_new(1.0f, 2.0f);
    pz_vec2 b = pz_vec2_new(3.0f, 4.0f);
    float d = pz_vec2_dot(a, b);
    ASSERT_NEAR(11.0f, d, EPSILON); // 1*3 + 2*4 = 11
}

TEST(vec2_len)
{
    pz_vec2 v = pz_vec2_new(3.0f, 4.0f);
    ASSERT_NEAR(5.0f, pz_vec2_len(v), EPSILON);
}

TEST(vec2_normalize)
{
    pz_vec2 v = pz_vec2_new(3.0f, 4.0f);
    pz_vec2 n = pz_vec2_normalize(v);
    ASSERT_NEAR(0.6f, n.x, EPSILON);
    ASSERT_NEAR(0.8f, n.y, EPSILON);
    ASSERT_NEAR(1.0f, pz_vec2_len(n), EPSILON);
}

TEST(vec2_rotate)
{
    pz_vec2 v = pz_vec2_new(1.0f, 0.0f);
    pz_vec2 r = pz_vec2_rotate(v, PZ_PI / 2.0f); // 90 degrees
    ASSERT_NEAR(0.0f, r.x, EPSILON);
    ASSERT_NEAR(1.0f, r.y, EPSILON);
}

TEST(vec2_reflect)
{
    pz_vec2 v = pz_vec2_new(1.0f, -1.0f);
    pz_vec2 n = pz_vec2_new(0.0f, 1.0f); // horizontal surface
    pz_vec2 r = pz_vec2_reflect(v, n);
    ASSERT_NEAR(1.0f, r.x, EPSILON);
    ASSERT_NEAR(1.0f, r.y, EPSILON);
}

// ============================================================================
// vec3 tests
// ============================================================================

TEST(vec3_add)
{
    pz_vec3 a = pz_vec3_new(1.0f, 2.0f, 3.0f);
    pz_vec3 b = pz_vec3_new(4.0f, 5.0f, 6.0f);
    pz_vec3 r = pz_vec3_add(a, b);
    ASSERT_NEAR(5.0f, r.x, EPSILON);
    ASSERT_NEAR(7.0f, r.y, EPSILON);
    ASSERT_NEAR(9.0f, r.z, EPSILON);
}

TEST(vec3_cross)
{
    pz_vec3 x = pz_vec3_new(1.0f, 0.0f, 0.0f);
    pz_vec3 y = pz_vec3_new(0.0f, 1.0f, 0.0f);
    pz_vec3 z = pz_vec3_cross(x, y);
    ASSERT_NEAR(0.0f, z.x, EPSILON);
    ASSERT_NEAR(0.0f, z.y, EPSILON);
    ASSERT_NEAR(1.0f, z.z, EPSILON);
}

TEST(vec3_dot)
{
    pz_vec3 a = pz_vec3_new(1.0f, 2.0f, 3.0f);
    pz_vec3 b = pz_vec3_new(4.0f, 5.0f, 6.0f);
    float d = pz_vec3_dot(a, b);
    ASSERT_NEAR(32.0f, d, EPSILON); // 1*4 + 2*5 + 3*6 = 32
}

TEST(vec3_normalize)
{
    pz_vec3 v = pz_vec3_new(1.0f, 2.0f, 2.0f);
    pz_vec3 n = pz_vec3_normalize(v);
    ASSERT_NEAR(1.0f, pz_vec3_len(n), EPSILON);
}

// ============================================================================
// mat4 tests
// ============================================================================

TEST(mat4_identity)
{
    pz_mat4 m = pz_mat4_identity();
    ASSERT_NEAR(1.0f, m.m[0], EPSILON);
    ASSERT_NEAR(0.0f, m.m[1], EPSILON);
    ASSERT_NEAR(0.0f, m.m[4], EPSILON);
    ASSERT_NEAR(1.0f, m.m[5], EPSILON);
    ASSERT_NEAR(1.0f, m.m[10], EPSILON);
    ASSERT_NEAR(1.0f, m.m[15], EPSILON);
}

TEST(mat4_identity_mul)
{
    pz_mat4 id = pz_mat4_identity();
    pz_mat4 t = pz_mat4_translate(pz_vec3_new(1.0f, 2.0f, 3.0f));
    pz_mat4 r = pz_mat4_mul(id, t);
    // Result should equal t
    ASSERT_NEAR(t.m[12], r.m[12], EPSILON);
    ASSERT_NEAR(t.m[13], r.m[13], EPSILON);
    ASSERT_NEAR(t.m[14], r.m[14], EPSILON);
}

TEST(mat4_translate)
{
    pz_mat4 t = pz_mat4_translate(pz_vec3_new(5.0f, 10.0f, 15.0f));
    pz_vec4 p = pz_vec4_new(0.0f, 0.0f, 0.0f, 1.0f);
    pz_vec4 r = pz_mat4_mul_vec4(t, p);
    ASSERT_NEAR(5.0f, r.x, EPSILON);
    ASSERT_NEAR(10.0f, r.y, EPSILON);
    ASSERT_NEAR(15.0f, r.z, EPSILON);
}

TEST(mat4_scale)
{
    pz_mat4 s = pz_mat4_scale(pz_vec3_new(2.0f, 3.0f, 4.0f));
    pz_vec4 p = pz_vec4_new(1.0f, 1.0f, 1.0f, 1.0f);
    pz_vec4 r = pz_mat4_mul_vec4(s, p);
    ASSERT_NEAR(2.0f, r.x, EPSILON);
    ASSERT_NEAR(3.0f, r.y, EPSILON);
    ASSERT_NEAR(4.0f, r.z, EPSILON);
}

TEST(mat4_rotate_z)
{
    pz_mat4 r = pz_mat4_rotate_z(PZ_PI / 2.0f); // 90 degrees
    pz_vec4 p = pz_vec4_new(1.0f, 0.0f, 0.0f, 1.0f);
    pz_vec4 result = pz_mat4_mul_vec4(r, p);
    ASSERT_NEAR(0.0f, result.x, EPSILON);
    ASSERT_NEAR(1.0f, result.y, EPSILON);
    ASSERT_NEAR(0.0f, result.z, EPSILON);
}

TEST(mat4_perspective_look_at)
{
    // Create a view-projection matrix
    pz_mat4 proj
        = pz_mat4_perspective(PZ_PI / 4.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    pz_mat4 view = pz_mat4_look_at(pz_vec3_new(0.0f, 0.0f, 5.0f), // eye
        pz_vec3_new(0.0f, 0.0f, 0.0f), // target
        pz_vec3_new(0.0f, 1.0f, 0.0f) // up
    );
    pz_mat4 vp = pz_mat4_mul(proj, view);

    // A point at origin should end up near center of clip space
    pz_vec4 origin = pz_vec4_new(0.0f, 0.0f, 0.0f, 1.0f);
    pz_vec4 clip = pz_mat4_mul_vec4(vp, origin);

    // After perspective divide, should be near center
    float ndc_x = clip.x / clip.w;
    float ndc_y = clip.y / clip.w;
    ASSERT_NEAR(0.0f, ndc_x, EPSILON);
    ASSERT_NEAR(0.0f, ndc_y, EPSILON);
}

TEST(mat4_inverse)
{
    pz_mat4 t = pz_mat4_translate(pz_vec3_new(5.0f, 10.0f, 15.0f));
    pz_mat4 inv = pz_mat4_inverse(t);
    pz_mat4 result = pz_mat4_mul(t, inv);

    // Should be identity
    ASSERT_NEAR(1.0f, result.m[0], EPSILON);
    ASSERT_NEAR(0.0f, result.m[1], EPSILON);
    ASSERT_NEAR(0.0f, result.m[4], EPSILON);
    ASSERT_NEAR(1.0f, result.m[5], EPSILON);
    ASSERT_NEAR(1.0f, result.m[10], EPSILON);
    ASSERT_NEAR(0.0f, result.m[12], EPSILON);
    ASSERT_NEAR(0.0f, result.m[13], EPSILON);
    ASSERT_NEAR(0.0f, result.m[14], EPSILON);
    ASSERT_NEAR(1.0f, result.m[15], EPSILON);
}

TEST_MAIN()
