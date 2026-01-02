/*
 * Tank Game - String Utility Tests
 */

#include "../src/core/pz_mem.h"
#include "../src/core/pz_str.h"
#include "test_framework.h"
#include <string.h>

/* ============================================================================
 * Basic String Operations
 * ============================================================================
 */

TEST(str_dup)
{
    pz_mem_init();

    char *s = pz_str_dup("hello");
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    // NULL input
    char *null_dup = pz_str_dup(NULL);
    ASSERT_NULL(null_dup);

    // Empty string
    char *empty = pz_str_dup("");
    ASSERT_NOT_NULL(empty);
    ASSERT_STR_EQ("", empty);
    pz_free(empty);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(str_ndup)
{
    pz_mem_init();

    // Normal case
    char *s = pz_str_ndup("hello world", 5);
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    // n larger than string
    s = pz_str_ndup("hi", 100);
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ("hi", s);
    pz_free(s);

    // n = 0
    s = pz_str_ndup("hello", 0);
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ("", s);
    pz_free(s);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(str_fmt)
{
    pz_mem_init();

    char *s = pz_str_fmt("Hello, %s! You have %d messages.", "Alice", 42);
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ("Hello, Alice! You have 42 messages.", s);
    pz_free(s);

    // No format args
    s = pz_str_fmt("Just a string");
    ASSERT_STR_EQ("Just a string", s);
    pz_free(s);

    // Empty
    s = pz_str_fmt("");
    ASSERT_STR_EQ("", s);
    pz_free(s);

    // Numbers
    s = pz_str_fmt("%d + %d = %d", 1, 2, 3);
    ASSERT_STR_EQ("1 + 2 = 3", s);
    pz_free(s);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

/* ============================================================================
 * Split and Join
 * ============================================================================
 */

TEST(str_split)
{
    pz_mem_init();

    size_t count = 0;
    char **parts = pz_str_split("a,b,c", ',', &count);

    ASSERT_NOT_NULL(parts);
    ASSERT_EQ(3, count);
    ASSERT_STR_EQ("a", parts[0]);
    ASSERT_STR_EQ("b", parts[1]);
    ASSERT_STR_EQ("c", parts[2]);
    ASSERT_NULL(parts[3]); // NULL terminated

    pz_str_split_free(parts);

    // No delimiter present
    parts = pz_str_split("hello", ',', &count);
    ASSERT_EQ(1, count);
    ASSERT_STR_EQ("hello", parts[0]);
    pz_str_split_free(parts);

    // Empty parts
    parts = pz_str_split("a,,c", ',', &count);
    ASSERT_EQ(3, count);
    ASSERT_STR_EQ("a", parts[0]);
    ASSERT_STR_EQ("", parts[1]);
    ASSERT_STR_EQ("c", parts[2]);
    pz_str_split_free(parts);

    // Delimiter at start/end
    parts = pz_str_split(",a,", ',', &count);
    ASSERT_EQ(3, count);
    ASSERT_STR_EQ("", parts[0]);
    ASSERT_STR_EQ("a", parts[1]);
    ASSERT_STR_EQ("", parts[2]);
    pz_str_split_free(parts);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(str_join)
{
    pz_mem_init();

    const char *parts[] = { "a", "b", "c" };
    char *joined = pz_str_join(parts, 3, ", ");
    ASSERT_STR_EQ("a, b, c", joined);
    pz_free(joined);

    // Single element
    const char *single[] = { "hello" };
    joined = pz_str_join(single, 1, ",");
    ASSERT_STR_EQ("hello", joined);
    pz_free(joined);

    // Empty separator
    joined = pz_str_join(parts, 3, "");
    ASSERT_STR_EQ("abc", joined);
    pz_free(joined);

    // Zero count
    joined = pz_str_join(parts, 0, ",");
    ASSERT_STR_EQ("", joined);
    pz_free(joined);

    // NULL separator treated as empty
    joined = pz_str_join(parts, 3, NULL);
    ASSERT_STR_EQ("abc", joined);
    pz_free(joined);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

/* ============================================================================
 * Trim
 * ============================================================================
 */

TEST(str_trim)
{
    pz_mem_init();

    char *s = pz_str_trim("  hello  ");
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    s = pz_str_trim("hello");
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    s = pz_str_trim("   ");
    ASSERT_STR_EQ("", s);
    pz_free(s);

    s = pz_str_trim("");
    ASSERT_STR_EQ("", s);
    pz_free(s);

    s = pz_str_trim("\t\n  hello \r\n");
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(str_ltrim)
{
    pz_mem_init();

    char *s = pz_str_ltrim("  hello  ");
    ASSERT_STR_EQ("hello  ", s);
    pz_free(s);

    s = pz_str_ltrim("hello");
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(str_rtrim)
{
    pz_mem_init();

    char *s = pz_str_rtrim("  hello  ");
    ASSERT_STR_EQ("  hello", s);
    pz_free(s);

    s = pz_str_rtrim("hello");
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

/* ============================================================================
 * Prefix/Suffix
 * ============================================================================
 */

TEST(str_starts_with)
{
    ASSERT(pz_str_starts_with("hello world", "hello"));
    ASSERT(pz_str_starts_with("hello", "hello"));
    ASSERT(pz_str_starts_with("hello", ""));
    ASSERT(!pz_str_starts_with("hello", "world"));
    ASSERT(!pz_str_starts_with("hi", "hello"));
    ASSERT(!pz_str_starts_with(NULL, "test"));
    ASSERT(!pz_str_starts_with("test", NULL));
}

TEST(str_ends_with)
{
    ASSERT(pz_str_ends_with("hello world", "world"));
    ASSERT(pz_str_ends_with("hello", "hello"));
    ASSERT(pz_str_ends_with("hello", ""));
    ASSERT(!pz_str_ends_with("hello", "world"));
    ASSERT(!pz_str_ends_with("hi", "hello"));
    ASSERT(!pz_str_ends_with(NULL, "test"));
    ASSERT(!pz_str_ends_with("test", NULL));
}

/* ============================================================================
 * Parsing
 * ============================================================================
 */

TEST(str_to_int)
{
    int val;

    ASSERT(pz_str_to_int("42", &val));
    ASSERT_EQ(42, val);

    ASSERT(pz_str_to_int("-123", &val));
    ASSERT_EQ(-123, val);

    ASSERT(pz_str_to_int("0", &val));
    ASSERT_EQ(0, val);

    // Invalid
    ASSERT(!pz_str_to_int("", &val));
    ASSERT(!pz_str_to_int("abc", &val));
    ASSERT(!pz_str_to_int("12abc", &val));
    ASSERT(!pz_str_to_int("12.5", &val));
    ASSERT(!pz_str_to_int(NULL, &val));
}

TEST(str_to_long)
{
    long val;

    ASSERT(pz_str_to_long("123456789", &val));
    ASSERT_EQ(123456789L, val);

    ASSERT(pz_str_to_long("-987654321", &val));
    ASSERT_EQ(-987654321L, val);

    ASSERT(!pz_str_to_long("abc", &val));
}

TEST(str_to_float)
{
    float val;

    ASSERT(pz_str_to_float("3.14", &val));
    ASSERT_NEAR(3.14f, val, 0.001f);

    ASSERT(pz_str_to_float("-2.5", &val));
    ASSERT_NEAR(-2.5f, val, 0.001f);

    ASSERT(pz_str_to_float("42", &val));
    ASSERT_NEAR(42.0f, val, 0.001f);

    ASSERT(!pz_str_to_float("abc", &val));
    ASSERT(!pz_str_to_float("", &val));
}

TEST(str_to_double)
{
    double val;

    ASSERT(pz_str_to_double("3.14159265359", &val));
    ASSERT_NEAR(3.14159265359, val, 0.0000001);

    ASSERT(!pz_str_to_double("abc", &val));
}

/* ============================================================================
 * Comparison
 * ============================================================================
 */

TEST(str_empty)
{
    ASSERT(pz_str_empty(NULL));
    ASSERT(pz_str_empty(""));
    ASSERT(!pz_str_empty("hello"));
    ASSERT(!pz_str_empty(" "));
}

TEST(str_cmp)
{
    ASSERT_EQ(0, pz_str_cmp("abc", "abc"));
    ASSERT(pz_str_cmp("abc", "abd") < 0);
    ASSERT(pz_str_cmp("abd", "abc") > 0);
    ASSERT(pz_str_cmp(NULL, "abc") < 0);
    ASSERT(pz_str_cmp("abc", NULL) > 0);
    ASSERT_EQ(0, pz_str_cmp(NULL, NULL));
}

TEST(str_casecmp)
{
    ASSERT_EQ(0, pz_str_casecmp("Hello", "hello"));
    ASSERT_EQ(0, pz_str_casecmp("HELLO", "hello"));
    ASSERT(pz_str_casecmp("abc", "ABD") < 0);
    ASSERT(pz_str_casecmp("ABD", "abc") > 0);
}

/* ============================================================================
 * Replace
 * ============================================================================
 */

TEST(str_replace)
{
    pz_mem_init();

    char *s = pz_str_replace("hello world", "world", "there");
    ASSERT_STR_EQ("hello there", s);
    pz_free(s);

    // Multiple occurrences
    s = pz_str_replace("abcabc", "abc", "x");
    ASSERT_STR_EQ("xx", s);
    pz_free(s);

    // Replacement longer than original
    s = pz_str_replace("ab", "ab", "hello");
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    // No match
    s = pz_str_replace("hello", "xyz", "abc");
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    // Replace with empty
    s = pz_str_replace("hello world", " ", "");
    ASSERT_STR_EQ("helloworld", s);
    pz_free(s);

    // Empty old string (no-op)
    s = pz_str_replace("hello", "", "x");
    ASSERT_STR_EQ("hello", s);
    pz_free(s);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST_MAIN()
