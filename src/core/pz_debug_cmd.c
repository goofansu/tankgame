/*
 * Tank Game - Debug Command Interface Implementation
 */

#include "pz_debug_cmd.h"
#include "../engine/render/pz_renderer.h"
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

// Parse and execute a single command line
// Returns false if the command is 'quit'
static bool
execute_command(const char *line, pz_renderer *renderer)
{
    // Skip empty lines and comments
    while (*line == ' ' || *line == '\t')
        line++;
    if (*line == '\0' || *line == '#' || *line == '\n')
        return true;

    // Parse command
    char cmd[64] = { 0 };
    char arg[256] = { 0 };

    // Simple parsing: command followed by optional argument
    int n = sscanf(line, "%63s %255[^\n]", cmd, arg);
    if (n < 1)
        return true;

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE, "Debug command: %s %s", cmd, arg);

    if (strcmp(cmd, "screenshot") == 0) {
        if (arg[0] && renderer) {
            pz_renderer_save_screenshot(renderer, arg);
        } else {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE,
                "screenshot command requires a path argument");
        }
    } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE, "Quit command received");
        return false;
    } else {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE, "Unknown debug command: %s", cmd);
    }

    return true;
}

bool
pz_debug_cmd_poll(pz_renderer *renderer)
{
    if (!s_cmd_file_path)
        return true;

    // Check if file exists and has content
    int64_t size = pz_file_size(s_cmd_file_path);
    if (size <= 0)
        return true;

    // Read the command file
    char *content = pz_file_read_text(s_cmd_file_path);
    if (!content)
        return true;

    // Clear the file immediately (so commands aren't re-executed)
    FILE *f = fopen(s_cmd_file_path, "w");
    if (f) {
        fclose(f);
    }

    // Execute each line as a command
    bool should_continue = true;
    char *line = content;
    char *next;

    while (line && *line) {
        // Find end of line
        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }

        if (!execute_command(line, renderer)) {
            should_continue = false;
            break;
        }

        line = next;
    }

    pz_free(content);
    return should_continue;
}
