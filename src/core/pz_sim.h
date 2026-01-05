/*
 * Tank Game - Simulation System
 *
 * Fixed timestep simulation with deterministic RNG and state hashing.
 * Provides deterministic game updates that are independent of frame rate.
 */

#ifndef PZ_SIM_H
#define PZ_SIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Fixed Timestep Configuration
 * ============================================================================
 */

// Default simulation tick rate (60 ticks per second)
#define PZ_SIM_TICK_RATE 60
#define PZ_SIM_DT (1.0f / PZ_SIM_TICK_RATE)

// Maximum ticks per frame (prevents spiral of death)
#define PZ_SIM_MAX_TICKS_PER_FRAME 4

/* ============================================================================
 * Deterministic Random Number Generator
 * ============================================================================
 *
 * Uses xorshift32 for fast, reproducible random numbers.
 * Each simulation tick should use pz_rng_* functions for gameplay randomness.
 */

typedef struct pz_rng {
    uint32_t state;
} pz_rng;

// Initialize RNG with a seed
void pz_rng_seed(pz_rng *rng, uint32_t seed);

// Get next random uint32_t
uint32_t pz_rng_next(pz_rng *rng);

// Get random float in [0, 1)
float pz_rng_float(pz_rng *rng);

// Get random float in [min, max)
float pz_rng_range(pz_rng *rng, float min, float max);

// Get random int in [min, max] (inclusive)
int pz_rng_int(pz_rng *rng, int min, int max);

// Get random angle in radians [0, 2*PI)
float pz_rng_angle(pz_rng *rng);

/* ============================================================================
 * State Hashing
 * ============================================================================
 *
 * Used to verify determinism - same inputs should produce same state hash.
 */

typedef struct pz_state_hash {
    uint32_t value;
} pz_state_hash;

// Initialize hash
void pz_hash_init(pz_state_hash *hash);

// Hash arbitrary data
void pz_hash_data(pz_state_hash *hash, const void *data, size_t size);

// Hash a float (with tolerance for floating point representation)
void pz_hash_float(pz_state_hash *hash, float value);

// Hash a vec2
void pz_hash_vec2(pz_state_hash *hash, float x, float y);

// Finalize and get hash value
uint32_t pz_hash_finalize(const pz_state_hash *hash);

/* ============================================================================
 * Simulation Context
 * ============================================================================
 */

typedef struct pz_sim {
    // Timing
    double accumulator; // Accumulated time for fixed timestep
    uint64_t tick; // Current simulation tick number
    float alpha; // Interpolation factor [0, 1] for rendering

    // RNG
    pz_rng rng;
    uint32_t initial_seed;

    // State tracking
    pz_state_hash current_hash;
    uint32_t last_hash_value;

    // Stats
    int ticks_this_frame;
    int total_ticks;
} pz_sim;

// Create simulation context with seed
pz_sim *pz_sim_create(uint32_t seed);

// Destroy simulation context
void pz_sim_destroy(pz_sim *sim);

// Reset simulation to initial state
void pz_sim_reset(pz_sim *sim, uint32_t seed);

// Accumulate time and return number of ticks to run this frame
// Call this once per frame with frame delta time
int pz_sim_accumulate(pz_sim *sim, double dt);

// Get fixed timestep delta (PZ_SIM_DT)
float pz_sim_dt(void);

// Get current tick number
uint64_t pz_sim_tick(const pz_sim *sim);

// Get interpolation alpha for rendering between ticks
float pz_sim_alpha(const pz_sim *sim);

// Begin a simulation tick (resets per-tick hash)
void pz_sim_begin_tick(pz_sim *sim);

// End a simulation tick (finalizes hash)
void pz_sim_end_tick(pz_sim *sim);

// Get the RNG for this simulation
pz_rng *pz_sim_rng(pz_sim *sim);

// Hash state data during a tick
void pz_sim_hash(pz_sim *sim, const void *data, size_t size);
void pz_sim_hash_float(pz_sim *sim, float value);
void pz_sim_hash_vec2(pz_sim *sim, float x, float y);

// Get the hash from the last completed tick
uint32_t pz_sim_get_hash(const pz_sim *sim);

// Set RNG seed (useful for script reproducibility)
void pz_sim_set_seed(pz_sim *sim, uint32_t seed);

#endif // PZ_SIM_H
