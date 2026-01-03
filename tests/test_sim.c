/*
 * Tests for the simulation system (pz_sim)
 */

#include "../src/core/pz_mem.h"
#include "../src/core/pz_sim.h"

#include <math.h>
#include <stdio.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        tests_run++;                                                           \
        if (cond) {                                                            \
            tests_passed++;                                                    \
        } else {                                                               \
            printf("FAIL: %s - %s\n", __func__, msg);                          \
        }                                                                      \
    } while (0)

/* ============================================================================
 * RNG Tests
 * ============================================================================
 */

static void
test_rng_seed(void)
{
    pz_rng rng1, rng2;

    // Same seed should produce same sequence
    pz_rng_seed(&rng1, 12345);
    pz_rng_seed(&rng2, 12345);

    for (int i = 0; i < 100; i++) {
        uint32_t a = pz_rng_next(&rng1);
        uint32_t b = pz_rng_next(&rng2);
        TEST_ASSERT(a == b, "Same seed should produce same sequence");
        if (a != b)
            break;
    }
}

static void
test_rng_different_seeds(void)
{
    pz_rng rng1, rng2;

    // Different seeds should produce different sequences
    pz_rng_seed(&rng1, 12345);
    pz_rng_seed(&rng2, 54321);

    int different = 0;
    for (int i = 0; i < 10; i++) {
        if (pz_rng_next(&rng1) != pz_rng_next(&rng2)) {
            different++;
        }
    }
    TEST_ASSERT(
        different > 0, "Different seeds should produce different values");
}

static void
test_rng_float_range(void)
{
    pz_rng rng;
    pz_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        float f = pz_rng_float(&rng);
        TEST_ASSERT(f >= 0.0f && f < 1.0f, "Float should be in [0, 1)");
        if (f < 0.0f || f >= 1.0f)
            break;
    }
}

static void
test_rng_int_range(void)
{
    pz_rng rng;
    pz_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        int val = pz_rng_int(&rng, 5, 10);
        TEST_ASSERT(val >= 5 && val <= 10, "Int should be in [5, 10]");
        if (val < 5 || val > 10)
            break;
    }
}

static void
test_rng_range(void)
{
    pz_rng rng;
    pz_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        float val = pz_rng_range(&rng, -5.0f, 5.0f);
        TEST_ASSERT(val >= -5.0f && val < 5.0f, "Range should be in [-5, 5)");
        if (val < -5.0f || val >= 5.0f)
            break;
    }
}

/* ============================================================================
 * Hash Tests
 * ============================================================================
 */

static void
test_hash_determinism(void)
{
    pz_state_hash h1, h2;

    pz_hash_init(&h1);
    pz_hash_init(&h2);

    // Same data should produce same hash
    int data[] = { 1, 2, 3, 4, 5 };
    pz_hash_data(&h1, data, sizeof(data));
    pz_hash_data(&h2, data, sizeof(data));

    TEST_ASSERT(pz_hash_finalize(&h1) == pz_hash_finalize(&h2),
        "Same data should produce same hash");
}

static void
test_hash_different_data(void)
{
    pz_state_hash h1, h2;

    pz_hash_init(&h1);
    pz_hash_init(&h2);

    int data1[] = { 1, 2, 3 };
    int data2[] = { 1, 2, 4 };

    pz_hash_data(&h1, data1, sizeof(data1));
    pz_hash_data(&h2, data2, sizeof(data2));

    TEST_ASSERT(pz_hash_finalize(&h1) != pz_hash_finalize(&h2),
        "Different data should produce different hash");
}

static void
test_hash_float(void)
{
    pz_state_hash h1, h2;

    pz_hash_init(&h1);
    pz_hash_init(&h2);

    pz_hash_float(&h1, 1.5f);
    pz_hash_float(&h2, 1.5f);

    TEST_ASSERT(pz_hash_finalize(&h1) == pz_hash_finalize(&h2),
        "Same float should produce same hash");

    pz_hash_init(&h1);
    pz_hash_init(&h2);

    pz_hash_float(&h1, 1.5f);
    pz_hash_float(&h2, 1.6f);

    TEST_ASSERT(pz_hash_finalize(&h1) != pz_hash_finalize(&h2),
        "Different floats should produce different hash");
}

/* ============================================================================
 * Simulation Tests
 * ============================================================================
 */

static void
test_sim_create_destroy(void)
{
    pz_sim *sim = pz_sim_create(12345);
    TEST_ASSERT(sim != NULL, "Create should succeed");
    if (!sim)
        return;

    TEST_ASSERT(pz_sim_tick(sim) == 0, "Initial tick should be 0");
    TEST_ASSERT(pz_sim_dt() > 0.0f, "dt should be positive");

    pz_sim_destroy(sim);
}

static void
test_sim_fixed_timestep(void)
{
    pz_sim *sim = pz_sim_create(0);
    if (!sim)
        return;

    // Accumulate exactly one tick's worth of time
    int ticks = pz_sim_accumulate(sim, PZ_SIM_DT);
    TEST_ASSERT(ticks == 1, "One dt should produce one tick");

    // Accumulate half a tick
    ticks = pz_sim_accumulate(sim, PZ_SIM_DT * 0.5);
    TEST_ASSERT(ticks == 0, "Half dt should produce no ticks");

    // Accumulate another half (total now = 1 full tick)
    ticks = pz_sim_accumulate(sim, PZ_SIM_DT * 0.5);
    TEST_ASSERT(ticks == 1, "Accumulated time should produce tick");

    // Accumulate multiple ticks at once
    ticks = pz_sim_accumulate(sim, PZ_SIM_DT * 3);
    TEST_ASSERT(ticks == 3, "3x dt should produce 3 ticks");

    pz_sim_destroy(sim);
}

static void
test_sim_max_ticks_per_frame(void)
{
    pz_sim *sim = pz_sim_create(0);
    if (!sim)
        return;

    // Accumulate way too much time (simulating a long frame/pause)
    int ticks = pz_sim_accumulate(sim, 1.0); // 1 second = 60 ticks normally
    TEST_ASSERT(ticks <= PZ_SIM_MAX_TICKS_PER_FRAME,
        "Should be capped to max ticks per frame");

    pz_sim_destroy(sim);
}

static void
test_sim_tick_lifecycle(void)
{
    pz_sim *sim = pz_sim_create(42);
    if (!sim)
        return;

    // Run a few ticks
    for (int i = 0; i < 5; i++) {
        pz_sim_begin_tick(sim);

        // Hash some state
        float pos_x = 1.0f + i;
        float pos_y = 2.0f + i;
        pz_sim_hash_vec2(sim, pos_x, pos_y);

        pz_sim_end_tick(sim);
    }

    TEST_ASSERT(pz_sim_tick(sim) == 5, "Should have run 5 ticks");
    TEST_ASSERT(pz_sim_get_hash(sim) != 0, "Hash should be non-zero");

    pz_sim_destroy(sim);
}

static void
test_sim_determinism(void)
{
    // Run the same simulation twice, verify same hash
    uint32_t hash1, hash2;

    for (int run = 0; run < 2; run++) {
        pz_sim *sim = pz_sim_create(12345);
        if (!sim)
            return;

        // Simulate 10 ticks with RNG usage
        for (int i = 0; i < 10; i++) {
            pz_sim_begin_tick(sim);

            // Use RNG (this must be deterministic)
            pz_rng *rng = pz_sim_rng(sim);
            float random_val = pz_rng_float(rng);
            pz_sim_hash_float(sim, random_val);

            // Hash some computed state
            float pos_x = 1.0f + random_val;
            float pos_y = 2.0f + random_val * 0.5f;
            pz_sim_hash_vec2(sim, pos_x, pos_y);

            pz_sim_end_tick(sim);
        }

        if (run == 0) {
            hash1 = pz_sim_get_hash(sim);
        } else {
            hash2 = pz_sim_get_hash(sim);
        }

        pz_sim_destroy(sim);
    }

    TEST_ASSERT(
        hash1 == hash2, "Same seed + inputs should produce same final hash");
}

static void
test_sim_alpha(void)
{
    pz_sim *sim = pz_sim_create(0);
    if (!sim)
        return;

    // Alpha should be 0 at start
    TEST_ASSERT(pz_sim_alpha(sim) == 0.0f, "Initial alpha should be 0");

    // Accumulate half a tick
    pz_sim_accumulate(sim, PZ_SIM_DT * 0.5);
    float alpha = pz_sim_alpha(sim);
    TEST_ASSERT(alpha > 0.4f && alpha < 0.6f, "Alpha should be ~0.5");

    pz_sim_destroy(sim);
}

/* ============================================================================
 * Main
 * ============================================================================
 */

int
main(void)
{
    pz_mem_init();

    printf("Running simulation tests...\n\n");

    // RNG tests
    test_rng_seed();
    test_rng_different_seeds();
    test_rng_float_range();
    test_rng_int_range();
    test_rng_range();

    // Hash tests
    test_hash_determinism();
    test_hash_different_data();
    test_hash_float();

    // Simulation tests
    test_sim_create_destroy();
    test_sim_fixed_timestep();
    test_sim_max_ticks_per_frame();
    test_sim_tick_lifecycle();
    test_sim_determinism();
    test_sim_alpha();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);

    pz_mem_dump_leaks();
    pz_mem_shutdown();

    return (tests_passed == tests_run) ? 0 : 1;
}
