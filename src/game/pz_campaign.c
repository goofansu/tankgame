/*
 * Campaign System Implementation
 */

#include "pz_campaign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_platform.h"

// Create campaign manager
pz_campaign_manager *
pz_campaign_create(void)
{
    pz_campaign_manager *mgr = calloc(1, sizeof(pz_campaign_manager));
    if (!mgr) {
        return NULL;
    }
    return mgr;
}

// Destroy campaign manager
void
pz_campaign_destroy(pz_campaign_manager *mgr)
{
    if (mgr) {
        free(mgr);
    }
}

// Helper: trim whitespace from both ends of a string in place
static char *
trim_whitespace(char *str)
{
    // Trim leading
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    // Trim trailing
    char *end = str + strlen(str) - 1;
    while (end > str
        && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        end--;
    }
    end[1] = '\0';

    return str;
}

// Load campaign from file
bool
pz_campaign_load(pz_campaign_manager *mgr, const char *path)
{
    if (!mgr || !path) {
        return false;
    }

    // Reset state
    memset(&mgr->campaign, 0, sizeof(mgr->campaign));
    memset(&mgr->progress, 0, sizeof(mgr->progress));
    mgr->loaded = false;

    // Read file
    size_t size;
    char *content = (char *)pz_file_read(path, &size);
    if (!content) {
        PZ_LOG_ERROR(PZ_LOG_CAT_GAME, "Failed to read campaign file: %s", path);
        return false;
    }

    // Default values
    int starting_lives = 3;

    // Parse line by line
    char *line = content;
    char *next;
    int line_num = 0;

    while (line && *line) {
        line_num++;

        // Find end of line
        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }

        // Trim the line
        char *trimmed = trim_whitespace(line);

        // Skip empty lines and comments
        if (*trimmed == '\0' || *trimmed == '#') {
            line = next;
            continue;
        }

        // Parse command
        if (strncmp(trimmed, "NAME ", 5) == 0) {
            // Campaign name
            const char *name = trim_whitespace(trimmed + 5);
            strncpy(mgr->campaign.name, name, PZ_CAMPAIGN_NAME_LEN - 1);
            mgr->campaign.name[PZ_CAMPAIGN_NAME_LEN - 1] = '\0';
        } else if (strncmp(trimmed, "MAP ", 4) == 0) {
            // Map path
            if (mgr->campaign.map_count >= PZ_CAMPAIGN_MAX_MAPS) {
                PZ_LOG_WARN(PZ_LOG_CAT_GAME,
                    "Too many maps in campaign (max %d)", PZ_CAMPAIGN_MAX_MAPS);
            } else {
                const char *map_path = trim_whitespace(trimmed + 4);
                strncpy(mgr->campaign.maps[mgr->campaign.map_count], map_path,
                    PZ_CAMPAIGN_PATH_LEN - 1);
                mgr->campaign
                    .maps[mgr->campaign.map_count][PZ_CAMPAIGN_PATH_LEN - 1]
                    = '\0';
                mgr->campaign.map_count++;
            }
        } else if (strncmp(trimmed, "LIVES ", 6) == 0) {
            // Starting lives
            starting_lives = atoi(trimmed + 6);
            if (starting_lives < 1) {
                starting_lives = 1;
            }
        } else {
            PZ_LOG_WARN(PZ_LOG_CAT_GAME, "Unknown command at line %d: %s",
                line_num, trimmed);
        }

        line = next;
    }

    free(content);

    // Validate
    if (mgr->campaign.map_count == 0) {
        PZ_LOG_ERROR(
            PZ_LOG_CAT_GAME, "No maps defined in campaign file: %s", path);
        return false;
    }

    // Set default name if none provided
    if (mgr->campaign.name[0] == '\0') {
        strncpy(mgr->campaign.name, "Unnamed Campaign", PZ_CAMPAIGN_NAME_LEN);
    }

    mgr->progress.starting_lives = starting_lives;
    mgr->loaded = true;

    PZ_LOG_INFO(PZ_LOG_CAT_GAME, "Loaded campaign '%s' with %d maps, %d lives",
        mgr->campaign.name, mgr->campaign.map_count, starting_lives);

    return true;
}

// Start the campaign
void
pz_campaign_start(pz_campaign_manager *mgr, int starting_lives)
{
    if (!mgr) {
        return;
    }

    mgr->progress.current_map = 0;
    mgr->progress.lives
        = starting_lives > 0 ? starting_lives : mgr->progress.starting_lives;
    mgr->progress.starting_lives = mgr->progress.lives;
    mgr->progress.level_complete = false;
    mgr->progress.game_over = false;
    mgr->progress.campaign_complete = false;

    PZ_LOG_INFO(PZ_LOG_CAT_GAME, "Starting campaign with %d lives",
        mgr->progress.lives);
}

// Get current map path
const char *
pz_campaign_get_current_map(const pz_campaign_manager *mgr)
{
    if (!mgr || !mgr->loaded) {
        return NULL;
    }

    if (mgr->progress.current_map >= mgr->campaign.map_count) {
        return NULL;
    }

    return mgr->campaign.maps[mgr->progress.current_map];
}

// Get campaign name
const char *
pz_campaign_get_name(const pz_campaign_manager *mgr)
{
    if (!mgr || !mgr->loaded) {
        return NULL;
    }
    return mgr->campaign.name;
}

// Advance to next level
bool
pz_campaign_advance(pz_campaign_manager *mgr)
{
    if (!mgr) {
        return false;
    }

    mgr->progress.level_complete = false;
    mgr->progress.current_map++;

    if (mgr->progress.current_map >= mgr->campaign.map_count) {
        mgr->progress.campaign_complete = true;
        PZ_LOG_INFO(PZ_LOG_CAT_GAME, "Campaign complete!");
        return false;
    }

    PZ_LOG_INFO(PZ_LOG_CAT_GAME, "Advancing to level %d/%d",
        mgr->progress.current_map + 1, mgr->campaign.map_count);
    return true;
}

// Player died
bool
pz_campaign_player_died(pz_campaign_manager *mgr)
{
    if (!mgr) {
        return false;
    }

    mgr->progress.lives--;

    if (mgr->progress.lives <= 0) {
        mgr->progress.lives = 0;
        mgr->progress.game_over = true;
        PZ_LOG_INFO(PZ_LOG_CAT_GAME, "Game Over! No lives remaining.");
        return false;
    }

    PZ_LOG_INFO(PZ_LOG_CAT_GAME, "Player died. %d lives remaining.",
        mgr->progress.lives);
    return true;
}

// Add lives
void
pz_campaign_add_lives(pz_campaign_manager *mgr, int count)
{
    if (!mgr || count <= 0) {
        return;
    }

    mgr->progress.lives += count;
    PZ_LOG_INFO(
        PZ_LOG_CAT_GAME, "Extra life! Now have %d lives.", mgr->progress.lives);
}

// Restart current level
void
pz_campaign_restart_level(pz_campaign_manager *mgr)
{
    if (!mgr) {
        return;
    }

    mgr->progress.level_complete = false;
    // current_map stays the same - we're restarting it
}

// Get lives
int
pz_campaign_get_lives(const pz_campaign_manager *mgr)
{
    return mgr ? mgr->progress.lives : 0;
}

// Get level number (1-based)
int
pz_campaign_get_level_number(const pz_campaign_manager *mgr)
{
    if (!mgr) {
        return 0;
    }
    int level = mgr->progress.current_map + 1;
    // Cap at map_count when campaign is complete (current_map may exceed it)
    if (level > mgr->campaign.map_count) {
        level = mgr->campaign.map_count;
    }
    return level;
}

// Get total levels
int
pz_campaign_get_level_count(const pz_campaign_manager *mgr)
{
    return mgr ? mgr->campaign.map_count : 0;
}

// Check level complete
bool
pz_campaign_is_level_complete(const pz_campaign_manager *mgr)
{
    return mgr ? mgr->progress.level_complete : false;
}

// Check game over
bool
pz_campaign_is_game_over(const pz_campaign_manager *mgr)
{
    return mgr ? mgr->progress.game_over : false;
}

// Check campaign complete
bool
pz_campaign_is_campaign_complete(const pz_campaign_manager *mgr)
{
    return mgr ? mgr->progress.campaign_complete : false;
}
