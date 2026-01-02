/*
 * Tank Game - Smoke Test
 *
 * Basic sanity tests to verify the test framework works.
 */

#include "test_framework.h"

TEST(smoke_true) { ASSERT(1 == 1); }

TEST(smoke_arithmetic)
{
    ASSERT_EQ(4, 2 + 2);
    ASSERT_EQ(10, 5 * 2);
}

TEST(smoke_float_comparison)
{
    float a = 0.1f + 0.2f;
    ASSERT_NEAR(0.3f, a, 0.0001f);
}

TEST(smoke_string)
{
    const char *hello = "hello";
    ASSERT_STR_EQ("hello", hello);
}

TEST(smoke_pointers)
{
    int x = 42;
    int *p = &x;
    ASSERT_NOT_NULL(p);

    int *null_ptr = NULL;
    ASSERT_NULL(null_ptr);
}

TEST_MAIN()
