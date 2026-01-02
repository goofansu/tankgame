/*
 * Tank Game - Debug Command Interface
 *
 * Allows external processes (like the coding agent) to send commands
 * to the running game for debugging purposes.
 *
 * Commands are sent by writing to a command file. The game polls this
 * file each frame and executes any pending commands.
 *
 * Supported commands:
 *   screenshot <path>   - Save a screenshot to the given path
 *   quit                - Exit the game
 */

#ifndef PZ_DEBUG_CMD_H
#define PZ_DEBUG_CMD_H

#include <stdbool.h>

// Forward declarations
typedef struct pz_renderer pz_renderer;

// Initialize the debug command system
// Creates/clears the command file at the given path
void pz_debug_cmd_init(const char *cmd_file_path);

// Shutdown and cleanup
void pz_debug_cmd_shutdown(void);

// Poll for and execute pending commands
// Call this once per frame
// Returns false if a 'quit' command was received
bool pz_debug_cmd_poll(pz_renderer *renderer);

// Get the default command file path
const char *pz_debug_cmd_default_path(void);

#endif // PZ_DEBUG_CMD_H
