/*
 * Campaign System
 *
 * Manages campaign progression through a sequence of maps.
 * Tracks player lives, current level, and win/lose conditions.
 */

#ifndef PZ_CAMPAIGN_H
#define PZ_CAMPAIGN_H

#include <stdbool.h>

// Maximum maps in a campaign
#define PZ_CAMPAIGN_MAX_MAPS 32

// Maximum length of campaign/map names
#define PZ_CAMPAIGN_NAME_LEN 64
#define PZ_CAMPAIGN_PATH_LEN 256

// Campaign data loaded from file
typedef struct pz_campaign {
    char name[PZ_CAMPAIGN_NAME_LEN];
    char maps[PZ_CAMPAIGN_MAX_MAPS][PZ_CAMPAIGN_PATH_LEN];
    int map_count;
} pz_campaign;

// Campaign progress (runtime state)
typedef struct pz_campaign_progress {
    int current_map; // Index of current map (0-based)
    int lives; // Player lives remaining
    int starting_lives; // Lives at campaign start
    bool level_complete; // Set when all enemies defeated
    bool game_over; // Set when lives reach 0
    bool campaign_complete; // Set when all maps completed
} pz_campaign_progress;

// Campaign manager - combines data and progress
typedef struct pz_campaign_manager {
    pz_campaign campaign;
    pz_campaign_progress progress;
    bool loaded;
} pz_campaign_manager;

/* ============================================================================
 * Campaign File Loading
 * ============================================================================
 */

// Create campaign manager
pz_campaign_manager *pz_campaign_create(void);

// Destroy campaign manager
void pz_campaign_destroy(pz_campaign_manager *mgr);

// Load campaign from file (returns true on success)
// File format:
//   # Comment
//   NAME Campaign Name
//   MAP path/to/map1.map
//   MAP path/to/map2.map
//   LIVES 3
bool pz_campaign_load(pz_campaign_manager *mgr, const char *path);

// Start the campaign (resets progress)
void pz_campaign_start(pz_campaign_manager *mgr, int starting_lives);

/* ============================================================================
 * Progression
 * ============================================================================
 */

// Get current map path (or NULL if no maps/campaign complete)
const char *pz_campaign_get_current_map(const pz_campaign_manager *mgr);

// Get campaign name
const char *pz_campaign_get_name(const pz_campaign_manager *mgr);

// Mark current level as complete and advance to next
// Returns true if there's a next level, false if campaign complete
bool pz_campaign_advance(pz_campaign_manager *mgr);

// Player died - decrement lives
// Returns true if player can continue (lives > 0), false if game over
bool pz_campaign_player_died(pz_campaign_manager *mgr);

// Add lives (from powerups, bonuses)
void pz_campaign_add_lives(pz_campaign_manager *mgr, int count);

// Reset current level (after death, if lives remain)
void pz_campaign_restart_level(pz_campaign_manager *mgr);

/* ============================================================================
 * Query State
 * ============================================================================
 */

// Get number of lives remaining
int pz_campaign_get_lives(const pz_campaign_manager *mgr);

// Get current level number (1-based for display)
int pz_campaign_get_level_number(const pz_campaign_manager *mgr);

// Get total number of levels
int pz_campaign_get_level_count(const pz_campaign_manager *mgr);

// Check if level is complete (waiting for transition)
bool pz_campaign_is_level_complete(const pz_campaign_manager *mgr);

// Check if game is over (no lives left)
bool pz_campaign_is_game_over(const pz_campaign_manager *mgr);

// Check if campaign is complete (all levels done)
bool pz_campaign_is_campaign_complete(const pz_campaign_manager *mgr);

#endif // PZ_CAMPAIGN_H
