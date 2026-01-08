/*
 * Map Editor System
 *
 * In-game editor for creating and modifying tank game maps.
 * Can be activated via F1 toggle during gameplay or launched
 * directly with --edit-map command line flag.
 */

#ifndef PZ_EDITOR_H
#define PZ_EDITOR_H

#include <stdbool.h>

#include "../core/pz_math.h"
#include "../engine/pz_camera.h"
#include "../engine/pz_font.h"
#include "../engine/render/pz_renderer.h"
#include "../engine/render/pz_texture.h"
#include "../game/pz_map.h"
#include "../game/pz_map_render.h"
#include "../game/pz_tile_registry.h"

// Forward declarations
typedef struct pz_background pz_background;

// Forward declarations
typedef struct pz_editor_ui pz_editor_ui;

// Maximum shortcut bar slots
#define PZ_EDITOR_MAX_SLOTS 6

// Virtual canvas size (map can expand up to this)
#define PZ_EDITOR_CANVAS_SIZE 200

// Map starts at center of virtual canvas
#define PZ_EDITOR_CANVAS_OFFSET 100

// Editor slot content type
typedef enum pz_editor_slot_type {
    PZ_EDITOR_SLOT_EMPTY,
    PZ_EDITOR_SLOT_TILE,
    PZ_EDITOR_SLOT_TAG, // For Phase 3+
} pz_editor_slot_type;

// Editor slot content
typedef struct pz_editor_slot {
    pz_editor_slot_type type;
    int tile_def_index; // Index into map's tile_defs (for TILE type)
    char tag_name[32]; // Tag name (for TAG type)
} pz_editor_slot;

// Window state for draggable dialogs
typedef struct pz_window_state {
    float x, y; // Window position (0,0 = auto-center)
    bool dragging;
    float drag_offset_x, drag_offset_y;
    int z_order;
} pz_window_state;

// Editor state structure
typedef struct pz_editor {
    // Is editor mode active?
    bool active;

    // Map being edited
    pz_map *map;
    pz_map_renderer *map_renderer;
    char map_path[256]; // Path for saving

    // Camera (use pz_camera for consistent screen-to-world conversion)
    pz_camera camera;
    float camera_zoom; // Zoom level for editor camera
    pz_vec2 camera_offset; // Camera offset from map center

    // Cursor state
    float mouse_x, mouse_y; // Screen coordinates
    int hover_tile_x, hover_tile_y; // Tile under cursor
    bool hover_valid; // Is cursor over a valid tile?

    // Selection
    int selected_slot; // 0-5, currently selected shortcut bar slot
    pz_editor_slot slots[PZ_EDITOR_MAX_SLOTS];

    // Input state
    bool mouse_left_down;
    bool mouse_left_just_pressed;
    bool mouse_left_just_released;
    bool mouse_right_just_pressed;

    // Dirty flag for auto-save
    bool dirty;
    bool auto_save_enabled; // Auto-save toggle (default: false)
    double last_save_time;
    double dirty_time; // When the map became dirty

    // Tile registry reference
    const pz_tile_registry *tile_registry;

    // Renderer reference
    pz_renderer *renderer;
    pz_texture_manager *tex_manager;
    pz_font_manager *font_mgr;
    pz_background *background;

    float ui_dpi_scale;

    // Grid overlay rendering
    pz_shader_handle grid_shader;
    pz_pipeline_handle grid_pipeline;
    pz_buffer_handle grid_vb;
    int grid_vertex_count;

    // Hover highlight rendering (dynamic, updated each frame)
    pz_pipeline_handle hover_pipeline; // No depth test, draws on top
    pz_buffer_handle hover_vb;
    int hover_vertex_count;

    // Arrow buffer for facing direction indicators
    pz_buffer_handle arrow_vb;

    // Editor UI
    pz_editor_ui *ui;
    bool ui_wants_mouse;

    // Viewport size (for UI)
    int viewport_width;
    int viewport_height;

    // Dialog state
    bool tags_dialog_open;
    bool map_settings_dialog_open;
    bool tile_picker_open;
    bool tag_editor_open;

    bool tag_rename_open;
    int tag_rename_index;
    int tag_rename_cursor;
    char tag_rename_buffer[32];
    char tag_rename_error[64];
    int tag_editor_index;

    bool map_name_edit_open;
    int map_name_cursor;
    char map_name_buffer[64];
    char map_name_error[64];

    // Tile picker hover state
    int tile_picker_hovered_index; // -1 if no tile hovered
    int tag_list_hovered_index; // -1 if no tag hovered

    // Close confirmation dialog
    bool confirm_close_open;
    bool wants_close; // Set when user confirms close

    // Rotation mode: click on tag to rotate, mouse movement updates angle
    bool rotation_mode;
    int rotation_tag_def_index; // Tag def being rotated (-1 if none)
    float rotation_start_angle; // Original angle for cancel

    int window_z_counter;

    // Window states for dialogs
    pz_window_state tags_window;
    pz_window_state tile_picker_window;
    pz_window_state tag_editor_window;
    pz_window_state confirm_close_window;
    pz_window_state tag_rename_window;
    pz_window_state map_name_window;
    pz_window_state map_settings_window;

    float map_settings_scroll;
    float map_settings_max_scroll;
    bool map_settings_visible;
    float map_settings_window_x;
    float map_settings_window_y;
    float map_settings_window_w;
    float map_settings_window_h;
} pz_editor;

// ============================================================================
// Lifecycle
// ============================================================================

// Create the editor (does not activate it)
pz_editor *pz_editor_create(pz_renderer *renderer, pz_texture_manager *tex_mgr,
    pz_font_manager *font_mgr, const pz_tile_registry *tile_registry);

// Destroy the editor
void pz_editor_destroy(pz_editor *editor);

// ============================================================================
// Activation
// ============================================================================

// Enter editor mode with existing map (F1 toggle)
// Takes ownership of map renderer, creates copy of map for editing
void pz_editor_enter(pz_editor *editor, pz_map *map,
    pz_map_renderer *map_renderer, const char *map_path);

// Enter editor mode for a new or existing file (--edit-map)
// If path exists, loads it. Otherwise creates new map.
// Returns true on success
bool pz_editor_enter_file(pz_editor *editor, const char *path);

// Exit editor mode
// Returns the edited map (caller takes ownership)
pz_map *pz_editor_exit(pz_editor *editor);

// Check if editor is active
bool pz_editor_is_active(const pz_editor *editor);

// Check if editor wants to close (user confirmed close)
bool pz_editor_wants_close(const pz_editor *editor);

// Clear the wants_close flag (after handling)
void pz_editor_clear_close_request(pz_editor *editor);

// ============================================================================
// Update/Input
// ============================================================================

// Process input event
// Returns true if event was consumed
bool pz_editor_event(pz_editor *editor, const void *event);

// Update editor state (call each frame when active)
void pz_editor_update(pz_editor *editor, float dt);

// Update mouse position (screen coordinates)
void pz_editor_set_mouse(pz_editor *editor, float x, float y);

// Handle mouse button events
void pz_editor_mouse_down(pz_editor *editor, int button);
void pz_editor_mouse_up(pz_editor *editor, int button);

// Handle scroll wheel
void pz_editor_scroll(pz_editor *editor, float delta);

// Handle key events (returns true if consumed)
bool pz_editor_key_down(pz_editor *editor, int keycode, bool repeat);
bool pz_editor_key_up(pz_editor *editor, int keycode);

// ============================================================================
// Rendering
// ============================================================================

// Get camera matrices for editor view
void pz_editor_get_camera(pz_editor *editor, pz_mat4 *view, pz_mat4 *projection,
    int viewport_width, int viewport_height);

// Render the editor (grid overlay, ghost preview)
void pz_editor_render(pz_editor *editor, const pz_mat4 *view_projection);

// Render editor UI (shortcut bar, panels)
void pz_editor_render_ui(
    pz_editor *editor, int screen_width, int screen_height);

// Render the map (ground + walls) using editor camera
void pz_editor_render_map(pz_editor *editor, const pz_mat4 *view_projection,
    pz_texture_handle light_texture, float light_scale_x, float light_scale_z,
    float light_offset_x, float light_offset_z);

// ============================================================================
// Slots
// ============================================================================

// Set a slot to a tile
void pz_editor_set_slot_tile(pz_editor *editor, int slot, int tile_def_index);
void pz_editor_set_slot_tag(pz_editor *editor, int slot, const char *tag_name);

// Clear a slot
void pz_editor_clear_slot(pz_editor *editor, int slot);

// Select a slot
void pz_editor_select_slot(pz_editor *editor, int slot);

// Cycle to next/prev populated slot
void pz_editor_cycle_slot(pz_editor *editor, int direction);

// ============================================================================
// Map Access
// ============================================================================

// Get the current map being edited
pz_map *pz_editor_get_map(pz_editor *editor);

// Get the map renderer
pz_map_renderer *pz_editor_get_map_renderer(pz_editor *editor);

// Force a save (normally auto-saves)
void pz_editor_save(pz_editor *editor);

// Set background renderer for immediate previews
void pz_editor_set_background(pz_editor *editor, pz_background *background);

#endif // PZ_EDITOR_H
