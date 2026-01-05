/*
 * Tank Game - Debug Command Interface Implementation
 *
 * This module handles the command pipe that allows external processes
 * to send debug script commands to the running game.
 *
 * Commands written to the pipe are parsed as debug script commands
 * and injected into the active script (or create a new script).
 */

#include "pz_debug_cmd.h"
#include "pz_debug_script.h"
#include "pz_log.h"
#include "pz_mem.h"
#include "pz_platform.h"
#include "pz_str.h"

#include <stdio.h>
#include <string.h>

static char *s_cmd_file_path = NULL;

// Default path for the command file
static const char *DEFAULT_CMD_PATH = "/tmp/tankgame_cmd";

const char *
pz_debug_cmd_default_path(void)
{
    return DEFAULT_CMD_PATH;
}

void
pz_debug_cmd_init(const char *cmd_file_path)
{
    if (cmd_file_path) {
        s_cmd_file_path = pz_str_dup(cmd_file_path);
    } else {
        s_cmd_file_path = pz_str_dup(DEFAULT_CMD_PATH);
    }

    // Clear/create the command file
    FILE *f = fopen(s_cmd_file_path, "w");
    if (f) {
        fclose(f);
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
        "Debug command interface initialized: %s", s_cmd_file_path);
}

void
pz_debug_cmd_shutdown(void)
{
    if (s_cmd_file_path) {
        // Remove the command file
        pz_file_delete(s_cmd_file_path);
        pz_free(s_cmd_file_path);
        s_cmd_file_path = NULL;
    }
}

char *
pz_debug_cmd_poll_commands(void)
{
    if (!s_cmd_file_path)
        return NULL;

    // Check if file exists and has content
    int64_t size = pz_file_size(s_cmd_file_path);
    if (size <= 0)
        return NULL;

    // Read the command file
    char *content = pz_file_read_text(s_cmd_file_path);
    if (!content)
        return NULL;

    // Clear the file immediately (so commands aren't re-executed)
    FILE *f = fopen(s_cmd_file_path, "w");
    if (f) {
        fclose(f);
    }

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE, "Debug command: received from pipe");

    return content;
}
