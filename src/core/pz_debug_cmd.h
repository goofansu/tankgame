/*
 * Tank Game - Debug Command Interface
 *
 * Allows external processes (like the coding agent) to send debug script
 * commands to the running game by writing to a command file.
 *
 * Commands are parsed as debug script commands (same syntax as .dbgscript
 * files). Multiple commands can be separated by newlines or semicolons.
 *
 * Example:
 *   echo "screenshot debug-temp/test.png" > /tmp/tankgame_cmd
 *   echo "aim 5.0 3.0; fire; frames 30; screenshot debug-temp/shot.png" >
 * /tmp/tankgame_cmd
 */

#ifndef PZ_DEBUG_CMD_H
#define PZ_DEBUG_CMD_H

#include <stdbool.h>

// Initialize the debug command system
// Creates/clears the command file at the given path
void pz_debug_cmd_init(const char *cmd_file_path);

// Shutdown and cleanup
void pz_debug_cmd_shutdown(void);

// Poll for pending commands and return them as a string
// Returns NULL if no commands pending
// Caller must free the returned string with pz_free()
char *pz_debug_cmd_poll_commands(void);

// Get the default command file path
const char *pz_debug_cmd_default_path(void);

#endif // PZ_DEBUG_CMD_H
