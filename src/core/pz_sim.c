/*
 * Tank Game - Simulation System Implementation
 */

#include "pz_sim.h"
#include "pz_mem.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Deterministic RNG (xorshift32)
 * ============================================================================
 */

void
pz_rng_seed(pz_rng *rng, uint32_t seed)
{
    // Ensure non-zero seed (xorshift requires non-zero state)
    rng->state = seed ? seed : 1;
}

uint32_t
pz_rng_next(pz_rng *rng)
{
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

float
pz_rng_float(pz_rng *rng)
{
    // Convert to float in [0, 1)
    return (float)(pz_rng_next(rng) >> 8) / (float)(1 << 24);
}

float
pz_rng_range(pz_rng *rng, float min, float max)
{
    return min + pz_rng_float(rng) * (max - min);
}

int
pz_rng_int(pz_rng *rng, int min, int max)
{
    if (min >= max)
        return min;
    uint32_t range = (uint32_t)(max - min + 1);
    return min + (int)(pz_rng_next(rng) % range);
}

float
pz_rng_angle(pz_rng *rng)
{
    return pz_rng_float(rng) * 6.283185307179586f; // 2 * PI
}

/* ============================================================================
 * State Hashing (FNV-1a)
 * ============================================================================
 */

#define FNV_PRIME 0x01000193
#define FNV_OFFSET 0x811c9dc5

void
pz_hash_init(pz_state_hash *hash)
{
    hash->value = FNV_OFFSET;
}

void
pz_hash_data(pz_state_hash *hash, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size; i++) {
        hash->value ^= bytes[i];
        hash->value *= FNV_PRIME;
    }
}

void
pz_hash_float(pz_state_hash *hash, float value)
{
    // Quantize to avoid floating point representation differences
    // Use fixed-point with 1/1024 precision
    int32_t quantized = (int32_t)(value * 1024.0f);
    pz_hash_data(hash, &quantized, sizeof(quantized));
}

void
pz_hash_vec2(pz_state_hash *hash, float x, float y)
{
    pz_hash_float(hash, x);
    pz_hash_float(hash, y);
}

uint32_t
pz_hash_finalize(const pz_state_hash *hash)
{
    return hash->value;
}

/* ============================================================================
 * Simulation Context
 * ============================================================================
 */

pz_sim *
pz_sim_create(uint32_t seed)
{
    pz_sim *sim = pz_alloc(sizeof(pz_sim));
    if (!sim)
        return NULL;

    memset(sim, 0, sizeof(pz_sim));
    pz_sim_reset(sim, seed);

    return sim;
}

void
pz_sim_destroy(pz_sim *sim)
{
    if (sim) {
        pz_free(sim);
    }
}

void
pz_sim_reset(pz_sim *sim, uint32_t seed)
{
    sim->accumulator = 0.0;
    sim->tick = 0;
    sim->alpha = 0.0f;
    sim->initial_seed = seed;
    pz_rng_seed(&sim->rng, seed);
    pz_hash_init(&sim->current_hash);
    sim->last_hash_value = 0;
    sim->ticks_this_frame = 0;
    sim->total_ticks = 0;
}

int
pz_sim_accumulate(pz_sim *sim, double dt)
{
    // Clamp dt to prevent spiral of death on long frames
    if (dt > 0.25) {
        dt = 0.25;
    }

    sim->accumulator += dt;
    sim->ticks_this_frame = 0;

    int ticks = 0;
    while (sim->accumulator >= PZ_SIM_DT) {
        sim->accumulator -= PZ_SIM_DT;
        ticks++;

        // Prevent spiral of death
        if (ticks >= PZ_SIM_MAX_TICKS_PER_FRAME) {
            // Discard excess accumulated time
            sim->accumulator = 0.0;
            break;
        }
    }

    sim->ticks_this_frame = ticks;
    sim->alpha = (float)(sim->accumulator / PZ_SIM_DT);

    return ticks;
}

float
pz_sim_dt(void)
{
    return PZ_SIM_DT;
}

uint64_t
pz_sim_tick(const pz_sim *sim)
{
    return sim->tick;
}

float
pz_sim_alpha(const pz_sim *sim)
{
    return sim->alpha;
}

void
pz_sim_begin_tick(pz_sim *sim)
{
    pz_hash_init(&sim->current_hash);
    // Hash the tick number for ordering verification
    pz_hash_data(&sim->current_hash, &sim->tick, sizeof(sim->tick));
}

void
pz_sim_end_tick(pz_sim *sim)
{
    sim->last_hash_value = pz_hash_finalize(&sim->current_hash);
    sim->tick++;
    sim->total_ticks++;
}

pz_rng *
pz_sim_rng(pz_sim *sim)
{
    return &sim->rng;
}

void
pz_sim_hash(pz_sim *sim, const void *data, size_t size)
{
    pz_hash_data(&sim->current_hash, data, size);
}

void
pz_sim_hash_float(pz_sim *sim, float value)
{
    pz_hash_float(&sim->current_hash, value);
}

void
pz_sim_hash_vec2(pz_sim *sim, float x, float y)
{
    pz_hash_vec2(&sim->current_hash, x, y);
}

uint32_t
pz_sim_get_hash(const pz_sim *sim)
{
    return sim->last_hash_value;
}
