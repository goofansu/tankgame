/*
 * Tank Game - Debug Script Execution System Implementation
 */

#include "pz_debug_script.h"
#include "../game/pz_projectile.h"
#include "../game/pz_tank.h"
#include "pz_log.h"
#include "pz_mem.h"
#include "pz_platform.h"
#include "pz_str.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Command types
typedef enum {
    CMD_NONE,
    CMD_TURBO,
    CMD_RENDER,
    CMD_FRAMES,
    CMD_MAP,
    CMD_SEED,
    CMD_INPUT,
    CMD_AIM,
    CMD_FIRE,
    CMD_HOLD_FIRE,
    CMD_SCREENSHOT,
    CMD_DUMP,
    CMD_QUIT,
} script_cmd_type;

// Parsed command
typedef struct {
    script_cmd_type type;
    union {
        bool bool_val; // turbo, render, hold_fire
        int int_val; // frames
        uint32_t seed_val; // seed
        char str_val[256]; // map, screenshot, dump paths
        struct {
            float x, y;
        } aim_val; // aim
        struct {
            float x, y;
            int mode; // 0=replace (stop), 1=add, -1=subtract
        } input_val; // input (move direction)
    };
} script_cmd;

// Script context
struct pz_debug_script {
    script_cmd *commands;
    int command_count;
    int current_cmd;

    // Execution state
    int frames_remaining; // For 'frames N' command
    bool done;

    // Mode flags
    bool turbo;
    bool render;

    // Input state
    pz_debug_script_input input;

    // Action data (for returning to caller)
    char action_path[256];
    uint32_t action_seed;
};

// Parse a single line into a command
static bool
parse_command(const char *line, script_cmd *cmd)
{
    // Skip leading whitespace
    while (*line && isspace(*line))
        line++;

    // Skip empty lines and comments
    if (*line == '\0' || *line == '#') {
        cmd->type = CMD_NONE;
        return true;
    }

    char keyword[64] = { 0 };
    char arg1[256] = { 0 };
    char arg2[64] = { 0 };

    int n = sscanf(line, "%63s %255s %63s", keyword, arg1, arg2);
    if (n < 1) {
        cmd->type = CMD_NONE;
        return true;
    }

    // Convert keyword to lowercase for comparison
    for (char *p = keyword; *p; p++)
        *p = (char)tolower(*p);

    if (strcmp(keyword, "turbo") == 0) {
        cmd->type = CMD_TURBO;
        cmd->bool_val = (strcmp(arg1, "on") == 0 || strcmp(arg1, "1") == 0);
    } else if (strcmp(keyword, "render") == 0) {
        cmd->type = CMD_RENDER;
        cmd->bool_val = (strcmp(arg1, "on") == 0 || strcmp(arg1, "1") == 0);
    } else if (strcmp(keyword, "frames") == 0) {
        cmd->type = CMD_FRAMES;
        cmd->int_val = atoi(arg1);
        if (cmd->int_val <= 0) {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE,
                "Debug script: invalid frame count '%s', using 1", arg1);
            cmd->int_val = 1;
        }
    } else if (strcmp(keyword, "map") == 0) {
        cmd->type = CMD_MAP;
        strncpy(cmd->str_val, arg1, sizeof(cmd->str_val) - 1);
    } else if (strcmp(keyword, "seed") == 0) {
        cmd->type = CMD_SEED;
        cmd->seed_val = (uint32_t)strtoul(arg1, NULL, 10);
    } else if (strcmp(keyword, "input") == 0) {
        cmd->type = CMD_INPUT;

        // Check for +/- prefix (additive mode)
        const char *dir = arg1;
        int mode = 1; // default: add

        if (dir[0] == '+') {
            mode = 1;
            dir++;
        } else if (dir[0] == '-') {
            mode = -1;
            dir++;
        }

        // Convert direction to lowercase
        char dir_lower[64] = { 0 };
        for (int i = 0; dir[i] && i < 63; i++)
            dir_lower[i] = (char)tolower(dir[i]);

        cmd->input_val.x = 0.0f;
        cmd->input_val.y = 0.0f;
        cmd->input_val.mode = mode;

        if (strcmp(dir_lower, "up") == 0) {
            cmd->input_val.y = -1.0f; // W key, toward top of screen
        } else if (strcmp(dir_lower, "down") == 0) {
            cmd->input_val.y = 1.0f; // S key, toward bottom of screen
        } else if (strcmp(dir_lower, "left") == 0) {
            cmd->input_val.x = -1.0f; // A key
        } else if (strcmp(dir_lower, "right") == 0) {
            cmd->input_val.x = 1.0f; // D key
        } else if (strcmp(dir_lower, "stop") == 0) {
            cmd->input_val.mode = 0; // replace with zero
        } else {
            pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE,
                "Debug script: unknown input direction '%s'", arg1);
        }
    } else if (strcmp(keyword, "aim") == 0) {
        cmd->type = CMD_AIM;
        cmd->aim_val.x = (float)atof(arg1);
        cmd->aim_val.y = (float)atof(arg2);
    } else if (strcmp(keyword, "fire") == 0) {
        cmd->type = CMD_FIRE;
    } else if (strcmp(keyword, "hold_fire") == 0) {
        cmd->type = CMD_HOLD_FIRE;
        cmd->bool_val = (strcmp(arg1, "on") == 0 || strcmp(arg1, "1") == 0);
    } else if (strcmp(keyword, "screenshot") == 0) {
        cmd->type = CMD_SCREENSHOT;
        strncpy(cmd->str_val, arg1, sizeof(cmd->str_val) - 1);
    } else if (strcmp(keyword, "dump") == 0) {
        cmd->type = CMD_DUMP;
        strncpy(cmd->str_val, arg1, sizeof(cmd->str_val) - 1);
    } else if (strcmp(keyword, "quit") == 0) {
        cmd->type = CMD_QUIT;
    } else {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_CORE,
            "Debug script: unknown command '%s'", keyword);
        cmd->type = CMD_NONE;
    }

    return true;
}

// Internal: count commands in text (newlines and semicolons are separators)
static int
count_commands(const char *text)
{
    int count = 1;
    for (const char *p = text; *p; p++) {
        if (*p == '\n' || *p == ';')
            count++;
    }
    return count;
}

// Internal: parse script text into commands array
// Returns number of commands parsed
static int
parse_script_text(const char *text, script_cmd *commands, int max_commands)
{
    // Make a mutable copy
    char *content = pz_str_dup(text);
    if (!content)
        return 0;

    int command_count = 0;
    char *pos = content;

    while (pos && *pos && command_count < max_commands) {
        // Find next separator (newline or semicolon)
        char *next_newline = strchr(pos, '\n');
        char *next_semi = strchr(pos, ';');
        char *next = NULL;

        if (next_newline && next_semi) {
            next = (next_newline < next_semi) ? next_newline : next_semi;
        } else {
            next = next_newline ? next_newline : next_semi;
        }

        if (next) {
            *next = '\0';
            next++;
        }

        script_cmd cmd = { 0 };
        if (parse_command(pos, &cmd) && cmd.type != CMD_NONE) {
            commands[command_count++] = cmd;
        }

        pos = next;
    }

    pz_free(content);
    return command_count;
}

// Internal: create a new script with allocated command array
static pz_debug_script *
create_script(int max_commands)
{
    pz_debug_script *script = pz_alloc(sizeof(pz_debug_script));
    memset(script, 0, sizeof(*script));

    script->commands = pz_alloc(sizeof(script_cmd) * max_commands);
    script->command_count = 0;
    script->current_cmd = 0;
    script->frames_remaining = 0;
    script->done = false;

    // Default modes for script execution
    script->turbo = true; // Fast by default
    script->render = true; // Render by default

    return script;
}

pz_debug_script *
pz_debug_script_load(const char *path)
{
    char *content = pz_file_read_text(path);
    if (!content) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_CORE,
            "Debug script: failed to load '%s'", path);
        return NULL;
    }

    int max_commands = count_commands(content);
    pz_debug_script *script = create_script(max_commands);

    script->command_count
        = parse_script_text(content, script->commands, max_commands);

    pz_free(content);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
        "Debug script: loaded '%s' with %d commands", path,
        script->command_count);

    return script;
}

pz_debug_script *
pz_debug_script_create_from_string(const char *script_text)
{
    if (!script_text || !*script_text) {
        return NULL;
    }

    int max_commands = count_commands(script_text);
    pz_debug_script *script = create_script(max_commands);

    script->command_count
        = parse_script_text(script_text, script->commands, max_commands);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
        "Debug script: created from string with %d commands",
        script->command_count);

    return script;
}

pz_debug_script *
pz_debug_script_inject(pz_debug_script *script, const char *commands)
{
    if (!commands || !*commands) {
        return script;
    }

    // If no existing script, create a new one
    if (!script) {
        return pz_debug_script_create_from_string(commands);
    }

    // Replace the script's commands
    int max_commands = count_commands(commands);

    // Reallocate command array if needed
    pz_free(script->commands);
    script->commands = pz_alloc(sizeof(script_cmd) * max_commands);

    script->command_count
        = parse_script_text(commands, script->commands, max_commands);
    script->current_cmd = 0;
    script->frames_remaining = 0;
    script->done = false;

    // Keep existing turbo/render settings, but reset input
    script->input.move_x = 0.0f;
    script->input.move_y = 0.0f;
    script->input.aim_x = 0.0f;
    script->input.aim_y = 0.0f;
    script->input.has_aim = false;
    script->input.fire = false;
    script->input.hold_fire = false;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE, "Debug script: injected %d commands",
        script->command_count);

    return script;
}

void
pz_debug_script_destroy(pz_debug_script *script)
{
    if (!script)
        return;

    pz_free(script->commands);
    pz_free(script);
}

bool
pz_debug_script_is_done(const pz_debug_script *script)
{
    return !script || script->done;
}

bool
pz_debug_script_should_render(const pz_debug_script *script)
{
    return !script || script->render;
}

bool
pz_debug_script_is_turbo(const pz_debug_script *script)
{
    return script && script->turbo;
}

const pz_debug_script_input *
pz_debug_script_get_input(const pz_debug_script *script)
{
    return script ? &script->input : NULL;
}

pz_debug_script_action
pz_debug_script_update(pz_debug_script *script)
{
    if (!script || script->done) {
        return PZ_DEBUG_SCRIPT_CONTINUE;
    }

    // Clear single-frame fire
    script->input.fire = false;

    // If we're counting down frames, just continue
    if (script->frames_remaining > 0) {
        script->frames_remaining--;
        return PZ_DEBUG_SCRIPT_CONTINUE;
    }

    // Process commands until we hit one that needs a frame
    while (script->current_cmd < script->command_count) {
        script_cmd *cmd = &script->commands[script->current_cmd];
        script->current_cmd++;

        switch (cmd->type) {
        case CMD_NONE:
            // Skip
            break;

        case CMD_TURBO:
            script->turbo = cmd->bool_val;
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE, "Debug script: turbo %s",
                cmd->bool_val ? "on" : "off");
            break;

        case CMD_RENDER:
            script->render = cmd->bool_val;
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE, "Debug script: render %s",
                cmd->bool_val ? "on" : "off");
            break;

        case CMD_FRAMES:
            script->frames_remaining
                = cmd->int_val - 1; // -1 because this frame counts
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE,
                "Debug script: advancing %d frames", cmd->int_val);
            return PZ_DEBUG_SCRIPT_CONTINUE;

        case CMD_MAP:
            strncpy(script->action_path, cmd->str_val,
                sizeof(script->action_path) - 1);
            pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
                "Debug script: loading map '%s'", cmd->str_val);
            return PZ_DEBUG_SCRIPT_LOAD_MAP;

        case CMD_SEED:
            script->action_seed = cmd->seed_val;
            pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
                "Debug script: setting seed %u", cmd->seed_val);
            return PZ_DEBUG_SCRIPT_SET_SEED;

        case CMD_INPUT:
            if (cmd->input_val.mode == 0) {
                // Replace mode (stop)
                script->input.move_x = 0.0f;
                script->input.move_y = 0.0f;
            } else if (cmd->input_val.mode == 1) {
                // Add mode
                script->input.move_x += cmd->input_val.x;
                script->input.move_y += cmd->input_val.y;
            } else {
                // Subtract mode
                script->input.move_x -= cmd->input_val.x;
                script->input.move_y -= cmd->input_val.y;
            }
            // Clamp to valid range
            if (script->input.move_x > 1.0f)
                script->input.move_x = 1.0f;
            if (script->input.move_x < -1.0f)
                script->input.move_x = -1.0f;
            if (script->input.move_y > 1.0f)
                script->input.move_y = 1.0f;
            if (script->input.move_y < -1.0f)
                script->input.move_y = -1.0f;
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE,
                "Debug script: input now (%.1f, %.1f)", script->input.move_x,
                script->input.move_y);
            break;

        case CMD_AIM:
            script->input.aim_x = cmd->aim_val.x;
            script->input.aim_y = cmd->aim_val.y;
            script->input.has_aim = true;
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE,
                "Debug script: aim at (%.2f, %.2f)", cmd->aim_val.x,
                cmd->aim_val.y);
            break;

        case CMD_FIRE:
            script->input.fire = true;
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE, "Debug script: fire");
            break;

        case CMD_HOLD_FIRE:
            script->input.hold_fire = cmd->bool_val;
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_CORE, "Debug script: hold_fire %s",
                cmd->bool_val ? "on" : "off");
            break;

        case CMD_SCREENSHOT:
            strncpy(script->action_path, cmd->str_val,
                sizeof(script->action_path) - 1);
            pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
                "Debug script: screenshot '%s'", cmd->str_val);
            return PZ_DEBUG_SCRIPT_SCREENSHOT;

        case CMD_DUMP:
            strncpy(script->action_path, cmd->str_val,
                sizeof(script->action_path) - 1);
            pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE,
                "Debug script: dump state to '%s'", cmd->str_val);
            return PZ_DEBUG_SCRIPT_DUMP;

        case CMD_QUIT:
            pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE, "Debug script: quit");
            script->done = true;
            return PZ_DEBUG_SCRIPT_QUIT;
        }
    }

    // Reached end of script
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE, "Debug script: completed");
    script->done = true;
    return PZ_DEBUG_SCRIPT_QUIT;
}

const char *
pz_debug_script_get_map_path(const pz_debug_script *script)
{
    return script ? script->action_path : NULL;
}

const char *
pz_debug_script_get_screenshot_path(const pz_debug_script *script)
{
    return script ? script->action_path : NULL;
}

const char *
pz_debug_script_get_dump_path(const pz_debug_script *script)
{
    return script ? script->action_path : NULL;
}

uint32_t
pz_debug_script_get_seed(const pz_debug_script *script)
{
    return script ? script->action_seed : 0;
}

void
pz_debug_script_dump_state(const char *path, pz_tank_manager *tank_mgr,
    pz_projectile_manager *proj_mgr, pz_tank *player, int frame_count)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_CORE,
            "Debug script: failed to open dump file '%s'", path);
        return;
    }

    fprintf(f, "# Tank Game State Dump\n");
    fprintf(f, "frame: %d\n\n", frame_count);

    // Player state
    if (player) {
        fprintf(f, "[player]\n");
        fprintf(f, "pos: %.3f %.3f\n", player->pos.x, player->pos.y);
        fprintf(f, "vel: %.3f %.3f\n", player->vel.x, player->vel.y);
        fprintf(f, "body_angle: %.3f\n", player->body_angle);
        fprintf(f, "turret_angle: %.3f\n", player->turret_angle);
        fprintf(f, "health: %d\n", player->health);
        fprintf(f, "flags: 0x%08x\n", player->flags);
        fprintf(f, "fire_cooldown: %.3f\n", player->fire_cooldown);
        fprintf(f, "\n");
    }

    // Tank counts
    if (tank_mgr) {
        int alive_enemies = 0;
        int dead_enemies = 0;

        for (int i = 0; i < PZ_MAX_TANKS; i++) {
            pz_tank *tank = &tank_mgr->tanks[i];
            if (!(tank->flags & PZ_TANK_FLAG_ACTIVE))
                continue;
            if (tank->flags & PZ_TANK_FLAG_PLAYER)
                continue;

            if (tank->flags & PZ_TANK_FLAG_DEAD)
                dead_enemies++;
            else
                alive_enemies++;
        }

        fprintf(f, "[tanks]\n");
        fprintf(f, "total: %d\n", tank_mgr->tank_count);
        fprintf(f, "enemies_alive: %d\n", alive_enemies);
        fprintf(f, "enemies_dead: %d\n", dead_enemies);
        fprintf(f, "\n");

        // Individual enemy positions
        fprintf(f, "[enemies]\n");
        int enemy_idx = 0;
        for (int i = 0; i < PZ_MAX_TANKS; i++) {
            pz_tank *tank = &tank_mgr->tanks[i];
            if (!(tank->flags & PZ_TANK_FLAG_ACTIVE))
                continue;
            if (tank->flags & PZ_TANK_FLAG_PLAYER)
                continue;

            const char *status
                = (tank->flags & PZ_TANK_FLAG_DEAD) ? "dead" : "alive";
            fprintf(f, "%d: pos=(%.3f, %.3f) health=%d status=%s\n", enemy_idx,
                tank->pos.x, tank->pos.y, tank->health, status);
            enemy_idx++;
        }
        fprintf(f, "\n");
    }

    // Projectile state
    if (proj_mgr) {
        fprintf(f, "[projectiles]\n");
        fprintf(f, "active: %d\n", proj_mgr->active_count);

        for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
            pz_projectile *proj = &proj_mgr->projectiles[i];
            if (!proj->active)
                continue;

            fprintf(f, "  pos=(%.3f, %.3f) vel=(%.3f, %.3f) bounces=%d\n",
                proj->pos.x, proj->pos.y, proj->velocity.x, proj->velocity.y,
                proj->bounces_remaining);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_CORE, "Debug script: dumped state to '%s'",
        path);
}
