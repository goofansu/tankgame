/*
 * Map Editor Implementation
 */

#include "pz_editor.h"
#include "pz_editor_ui.h"
#include "third_party/sokol/sokol_app.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"
#include "../engine/pz_camera.h"
#include "../game/pz_background.h"

// ============================================================================
// Constants
// ============================================================================

#define EDITOR_PADDING_TILES 2 // Tiles of padding around map
#define EDITOR_REFERENCE_WIDTH 15 // Reference map width for default zoom
#define EDITOR_REFERENCE_HEIGHT 10 // Reference map height for default zoom
#define EDITOR_AUTO_SAVE_DELAY 5.0 // Seconds to wait before auto-saving
#define EDITOR_GRID_LINE_ALPHA 0.25f
#define EDITOR_GRID_EXPANSION_ALPHA 0.15f
#define EDITOR_TAGS_DIALOG_W 350.0f
#define EDITOR_TAGS_DIALOG_H 400.0f
#define EDITOR_TILE_PICKER_W 420.0f
#define EDITOR_TILE_PICKER_H 450.0f
#define EDITOR_TAG_RENAME_W 320.0f
#define EDITOR_TAG_RENAME_H 140.0f
#define EDITOR_CONFIRM_CLOSE_W 300.0f
#define EDITOR_CONFIRM_CLOSE_H 120.0f
#define EDITOR_MAP_SETTINGS_W 460.0f
#define EDITOR_MAP_SETTINGS_H 640.0f
#define EDITOR_NAME_DIALOG_W 320.0f
#define EDITOR_NAME_DIALOG_H 140.0f
#define EDITOR_SETTINGS_LABEL_W 150.0f
#define EDITOR_SETTINGS_ROW_H 26.0f
#define EDITOR_SETTINGS_BUTTON_W 26.0f
#define EDITOR_SETTINGS_VALUE_W 90.0f

static const char *EDITOR_MUSIC_OPTIONS[] = {
    "(none)",
    "march",
};
static const int EDITOR_MUSIC_OPTION_COUNT
    = sizeof(EDITOR_MUSIC_OPTIONS) / sizeof(EDITOR_MUSIC_OPTIONS[0]);

// ============================================================================
// Forward Declarations
// ============================================================================

static void editor_rebuild_grid(pz_editor *editor);
static void editor_apply_edit(
    pz_editor *editor, int tile_x, int tile_y, bool raise);
static void editor_mark_dirty(pz_editor *editor);
static void editor_auto_save(pz_editor *editor);
static void editor_update_hover(pz_editor *editor);
static void editor_init_default_slots(pz_editor *editor);
static float editor_calculate_zoom(pz_editor *editor);
static float editor_get_tile_height(pz_editor *editor, int tile_x, int tile_y);
static bool editor_mouse_over_dialog(pz_editor *editor);
static pz_vec4 editor_preview_color_for_index(int idx);
static int editor_find_or_add_tile_def(
    pz_editor *editor, const pz_tile_config *tile, const char *context);
static int editor_render_slot_widget(
    pz_editor *editor, float x, float y, float size, int slot_index);
static void editor_render_shortcut_bar(
    pz_editor *editor, int logical_width, int logical_height);
static void editor_render_info_text(pz_editor *editor);
static void editor_render_toolbar(pz_editor *editor, int logical_width);
static void editor_render_dialogs(
    pz_editor *editor, int logical_width, int logical_height, float dpi_scale);
static void editor_render_map_settings_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input);
static void editor_render_map_name_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input);
static bool editor_row_visible(
    float draw_y, float height, float view_top, float view_bottom);
static void editor_draw_section_header(pz_editor *editor, float x, float width,
    float *y, const char *label, pz_vec4 color, float scroll, float view_top,
    float view_bottom);
static bool editor_draw_list_item(
    pz_editor *editor, float x, float y, float w, const char *label);
static void editor_render_tags_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input);
static void editor_render_tile_picker(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input);
static void editor_render_tag_editor_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input);
static void editor_render_tag_rename_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input);
static void editor_render_confirm_close(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input);
static void editor_mark_map_settings_changed(pz_editor *editor);
static void editor_refresh_background(pz_editor *editor);
static void editor_open_map_name_dialog(pz_editor *editor);
static void editor_cancel_map_name_dialog(pz_editor *editor);
static void editor_commit_map_name_dialog(pz_editor *editor);
static bool editor_map_name_char_valid(uint32_t codepoint);
static void editor_handle_map_name_char_input(
    pz_editor *editor, uint32_t codepoint);
static bool editor_draw_float_row(pz_editor *editor, float x, float width,
    float *y, const char *label, float *value, float step, float min, float max,
    const char *fmt, float scroll, float view_top, float view_bottom);
static bool editor_draw_int_row(pz_editor *editor, float x, float width,
    float *y, const char *label, int *value, int step, int min, int max,
    float scroll, float view_top, float view_bottom);
static bool editor_draw_toggle_row(pz_editor *editor, float x, float width,
    float *y, const char *label, bool *value, float scroll, float view_top,
    float view_bottom);
static bool editor_draw_color_editor(pz_editor *editor, float x, float width,
    float *y, const char *label, pz_vec3 *color, float scroll, float view_top,
    float view_bottom);
static pz_vec2 editor_world_to_tile(const pz_map *map, pz_vec2 world);
static pz_vec2 editor_tile_to_world(const pz_map *map, pz_vec2 tile);
static void editor_open_tag_editor(pz_editor *editor, int tag_index);
static void editor_close_tag_editor(pz_editor *editor);
static void editor_toggle_dialog(
    pz_editor *editor, bool *open, pz_window_state *state);
static void editor_open_dialog(
    pz_editor *editor, bool *open, pz_window_state *state);
static void editor_close_dialog(
    pz_editor *editor, bool *open, pz_window_state *state);
static bool editor_point_in_rect(
    float x, float y, float rx, float ry, float rw, float rh);
static void editor_window_rect(const pz_window_state *state, float w, float h,
    int screen_w, int screen_h, float *out_x, float *out_y);
static void editor_render_tag_overlays(pz_editor *editor, float dpi_scale);
static void editor_prune_tag_placements(pz_editor *editor);
static bool editor_expand_map_to_include(
    pz_editor *editor, int tile_x, int tile_y, int *offset_x, int *offset_y);
static void editor_place_tag(
    pz_editor *editor, int tile_x, int tile_y, const char *tag_name);
static void editor_remove_tag(
    pz_editor *editor, int tile_x, int tile_y, const char *tag_name);
static void editor_mark_tags_dirty(pz_editor *editor);
static int editor_find_tag_def_index(pz_editor *editor, const char *tag_name);
static pz_vec4 editor_tag_color(pz_tag_type type);
static pz_font *editor_get_ui_font(pz_editor *editor);
static void editor_open_tag_rename(pz_editor *editor, int tag_index);
static void editor_cancel_tag_rename(pz_editor *editor);
static void editor_commit_tag_rename(pz_editor *editor);
static bool editor_tag_name_valid_char(uint32_t codepoint);
static bool editor_tag_name_is_valid(const char *name);
static bool editor_tag_name_is_unique(
    pz_editor *editor, const char *name, int ignore_index);
static void editor_handle_tag_char_input(pz_editor *editor, uint32_t codepoint);
static void editor_generate_tag_name(
    pz_editor *editor, pz_tag_type type, char *out, size_t out_size);
static void editor_init_tag_def(
    pz_editor *editor, pz_tag_def *def, pz_tag_type type);
static bool editor_tag_supports_rotation(const pz_tag_def *def);
static float *editor_get_tag_angle(pz_tag_def *def);
static void editor_enter_rotation_mode(
    pz_editor *editor, int tile_x, int tile_y);
static void editor_exit_rotation_mode(pz_editor *editor, bool cancel);
static void editor_update_rotation(pz_editor *editor);

// ============================================================================
// Lifecycle
// ============================================================================

pz_editor *
pz_editor_create(pz_renderer *renderer, pz_texture_manager *tex_mgr,
    pz_font_manager *font_mgr, const pz_tile_registry *tile_registry)
{
    pz_editor *editor = pz_calloc(1, sizeof(pz_editor));
    if (!editor) {
        return NULL;
    }

    editor->font_mgr = font_mgr;

    editor->renderer = renderer;
    editor->tex_manager = tex_mgr;
    editor->tile_registry = tile_registry;
    editor->background = NULL;
    editor->active = false;
    editor->selected_slot = 0;
    editor->tile_picker_hovered_index = -1;
    editor->tag_list_hovered_index = -1;
    editor->tag_rename_index = -1;
    editor->tag_editor_index = -1;

    // Initialize slots as empty
    for (int i = 0; i < PZ_EDITOR_MAX_SLOTS; i++) {
        editor->slots[i].type = PZ_EDITOR_SLOT_EMPTY;
        editor->slots[i].tile_def_index = -1;
        editor->slots[i].tag_name[0] = '\0';
    }

    // Create grid shader and pipeline (reuse debug_line_3d which is proven to
    // work)
    editor->grid_shader
        = pz_renderer_load_shader(renderer, "shaders/debug_line_3d.vert",
            "shaders/debug_line_3d.frag", "debug_line_3d");

    if (editor->grid_shader != PZ_INVALID_HANDLE) {
        pz_vertex_attr grid_attrs[] = {
            { .name = "a_position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
            { .name = "a_color",
                .type = PZ_ATTR_FLOAT4,
                .offset = 3 * sizeof(float) },
        };

        pz_pipeline_desc grid_desc = {
            .shader = editor->grid_shader,
            .vertex_layout = { .attrs = grid_attrs,
                .attr_count = 2,
                .stride = sizeof(float) * 7 },
            .blend = PZ_BLEND_ALPHA,
            .depth = PZ_DEPTH_READ,
            .cull = PZ_CULL_NONE,
            .primitive = PZ_PRIMITIVE_LINES,
        };
        editor->grid_pipeline
            = pz_renderer_create_pipeline(renderer, &grid_desc);

        // Hover highlight pipeline - no depth test, always draws on top
        pz_pipeline_desc hover_desc = {
            .shader = editor->grid_shader,
            .vertex_layout = { .attrs = grid_attrs,
                .attr_count = 2,
                .stride = sizeof(float) * 7 },
            .blend = PZ_BLEND_ALPHA,
            .depth = PZ_DEPTH_NONE,
            .cull = PZ_CULL_NONE,
            .primitive = PZ_PRIMITIVE_LINES,
        };
        editor->hover_pipeline
            = pz_renderer_create_pipeline(renderer, &hover_desc);
    }

    // Create hover highlight buffer (dynamic, for ghost preview)
    // 4 lines x 2 vertices = 8 vertices for highlight
    // Each vertex: 7 floats (pos3 + color4)
    pz_buffer_desc hover_buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STREAM,
        .size = 8 * 7 * sizeof(float),
    };
    editor->hover_vb = pz_renderer_create_buffer(renderer, &hover_buf_desc);

    // Create arrow buffer for facing direction indicators
    // Max 64 arrows x 6 vertices each = 384 vertices
    pz_buffer_desc arrow_buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STREAM,
        .size = 64 * 6 * 7 * sizeof(float),
    };
    editor->arrow_vb = pz_renderer_create_buffer(renderer, &arrow_buf_desc);

    // Create editor UI
    editor->ui = pz_editor_ui_create(renderer, font_mgr);

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Editor created");

    return editor;
}

void
pz_editor_destroy(pz_editor *editor)
{
    if (!editor) {
        return;
    }

    // Clean up grid resources
    if (editor->grid_vb != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(editor->renderer, editor->grid_vb);
    }
    if (editor->hover_vb != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(editor->renderer, editor->hover_vb);
    }
    if (editor->arrow_vb != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(editor->renderer, editor->arrow_vb);
    }
    if (editor->grid_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(editor->renderer, editor->grid_pipeline);
    }
    if (editor->hover_pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(editor->renderer, editor->hover_pipeline);
    }
    if (editor->grid_shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(editor->renderer, editor->grid_shader);
    }

    // If we still own a map, destroy it
    if (editor->map && editor->active) {
        pz_map_destroy(editor->map);
        pz_map_renderer_destroy(editor->map_renderer);
    }

    // Destroy UI
    if (editor->ui) {
        pz_editor_ui_destroy(editor->ui);
    }

    pz_free(editor);
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Editor destroyed");
}

// ============================================================================
// Activation
// ============================================================================

void
pz_editor_enter(pz_editor *editor, pz_map *map, pz_map_renderer *map_renderer,
    const char *map_path)
{
    if (!editor || !map) {
        return;
    }

    editor->active = true;
    editor->map = map;
    editor->map_renderer = map_renderer;
    editor->dirty = false;

    // Store map path for saving
    if (map_path) {
        strncpy(editor->map_path, map_path, sizeof(editor->map_path) - 1);
        editor->map_path[sizeof(editor->map_path) - 1] = '\0';
    } else {
        editor->map_path[0] = '\0';
    }

    // Set tile registry on map
    if (editor->tile_registry) {
        pz_map_set_tile_registry(editor->map, editor->tile_registry);
    }

    // Calculate zoom to fit map
    editor->camera_zoom = editor_calculate_zoom(editor);
    editor->camera_offset = (pz_vec2) { 0.0f, 0.0f };

    // Initialize default slots
    editor_init_default_slots(editor);
    editor_prune_tag_placements(editor);

    // Build grid overlay
    editor_rebuild_grid(editor);

    editor_refresh_background(editor);

    // DEBUG: Auto-open tile picker for testing (uncomment to test)
    // editor->tile_picker_open = true;

    pz_log(
        PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Editor entered: %s", editor->map_path);
}

bool
pz_editor_enter_file(pz_editor *editor, const char *path)
{
    if (!editor || !path) {
        return false;
    }

    // Try to load existing map
    pz_map *map = pz_map_load(path);

    if (!map) {
        // Create new map with defaults
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Creating new map: %s", path);

        map = pz_map_create(10, 10, 2.0f);
        if (!map) {
            pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to create new map");
            return false;
        }

        // Set default name
        strncpy(map->name, "Untitled", sizeof(map->name) - 1);

        // Set default lighting
        map->lighting.ambient_color = (pz_vec3) { 0.4f, 0.45f, 0.5f };
        map->lighting.sun_direction = (pz_vec3) { 0.4f, -0.8f, 0.3f };
        map->lighting.sun_color = (pz_vec3) { 1.0f, 0.95f, 0.85f };
        map->lighting.has_sun = true;
        map->lighting.ambient_darkness = 0.0f;

        // Initialize all cells to ground at height 0
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                pz_map_set_height(map, x, y, 0);
            }
        }
    }

    // Set tile registry
    if (editor->tile_registry) {
        pz_map_set_tile_registry(map, editor->tile_registry);
    }

    // Create map renderer
    pz_map_renderer *renderer = pz_map_renderer_create(
        editor->renderer, editor->tex_manager, editor->tile_registry);
    if (!renderer) {
        pz_map_destroy(map);
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
            "Failed to create map renderer for editor");
        return false;
    }
    pz_map_renderer_set_map(renderer, map);

    // Enter editor with this map
    pz_editor_enter(editor, map, renderer, path);

    return true;
}

pz_map *
pz_editor_exit(pz_editor *editor)
{
    if (!editor || !editor->active) {
        return NULL;
    }

    // Save any pending changes
    if (editor->dirty) {
        editor_auto_save(editor);
    }

    editor->active = false;

    // Return the map and renderer - caller takes ownership
    pz_map *map = editor->map;
    editor->map = NULL;
    editor->map_renderer = NULL;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Editor exited");

    return map;
}

bool
pz_editor_is_active(const pz_editor *editor)
{
    return editor && editor->active;
}

bool
pz_editor_wants_close(const pz_editor *editor)
{
    return editor && editor->wants_close;
}

void
pz_editor_clear_close_request(pz_editor *editor)
{
    if (editor) {
        editor->wants_close = false;
    }
}

// ============================================================================
// Update/Input
// ============================================================================

bool
pz_editor_event(pz_editor *editor, const void *event)
{
    if (!editor || !editor->active || !event) {
        return false;
    }

    const sapp_event *evt = (const sapp_event *)event;

    switch (evt->type) {
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        pz_editor_set_mouse(editor, evt->mouse_x, evt->mouse_y);
        return true;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        pz_editor_mouse_down(editor, evt->mouse_button);
        return true;
    case SAPP_EVENTTYPE_MOUSE_UP:
        pz_editor_mouse_up(editor, evt->mouse_button);
        return true;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        pz_editor_scroll(editor, evt->scroll_y);
        return true;
    case SAPP_EVENTTYPE_CHAR:
        if (editor->map_name_edit_open) {
            editor_handle_map_name_char_input(editor, evt->char_code);
            return true;
        }
        editor_handle_tag_char_input(editor, evt->char_code);
        return editor->tag_rename_open;
    case SAPP_EVENTTYPE_KEY_DOWN:
        return pz_editor_key_down(editor, evt->key_code, evt->key_repeat);
    case SAPP_EVENTTYPE_KEY_UP:
        return pz_editor_key_up(editor, evt->key_code);
    default:
        break;
    }

    return false;
}

void
pz_editor_update(pz_editor *editor, float dt)
{
    if (!editor || !editor->active) {
        return;
    }

    (void)dt;

    // Update hover state
    editor_update_hover(editor);

    // Paint mode: apply paint when dragging to new tiles
    if (editor->paint_mode && editor->hover_valid) {
        // Check if we moved to a different tile
        if (editor->hover_tile_x != editor->paint_last_tile_x
            || editor->hover_tile_y != editor->paint_last_tile_y) {

            int tile_x = editor->hover_tile_x;
            int tile_y = editor->hover_tile_y;

            // Expand map if needed
            if (!pz_map_in_bounds(editor->map, tile_x, tile_y)) {
                int offset_x = 0, offset_y = 0;
                if (editor_expand_map_to_include(
                        editor, tile_x, tile_y, &offset_x, &offset_y)) {
                    tile_x += offset_x;
                    tile_y += offset_y;
                    editor->paint_last_tile_x += offset_x;
                    editor->paint_last_tile_y += offset_y;
                    editor->camera_zoom = editor_calculate_zoom(editor);
                }
            }

            // Apply paint if tile is now in bounds
            if (pz_map_in_bounds(editor->map, tile_x, tile_y)) {
                // Check for entity - don't paint over entities
                bool has_entity
                    = pz_map_find_tag_placement(editor->map, tile_x, tile_y, -1)
                    >= 0;
                if (!has_entity) {
                    pz_map_cell new_cell = {
                        .height = editor->paint_target_height,
                        .tile_index = editor->paint_target_tile_index,
                    };
                    pz_map_set_cell(editor->map, tile_x, tile_y, new_cell);
                    editor_mark_dirty(editor);
                }
            }

            // Update last painted tile
            editor->paint_last_tile_x = editor->hover_tile_x;
            editor->paint_last_tile_y = editor->hover_tile_y;
        }
    }

    // Update rotation if in rotation mode
    if (editor->rotation_mode) {
        editor_update_rotation(editor);
    }

    // Auto-save if dirty and auto-save is enabled
    if (editor->dirty && editor->auto_save_enabled) {
        double now = pz_time_now();
        if (now - editor->dirty_time >= EDITOR_AUTO_SAVE_DELAY) {
            editor_auto_save(editor);
        }
    }
}

void
pz_editor_set_mouse(pz_editor *editor, float x, float y)
{
    if (!editor) {
        return;
    }
    editor->mouse_x = x;
    editor->mouse_y = y;
}

void
pz_editor_mouse_down(pz_editor *editor, int button)
{
    if (!editor || !editor->active) {
        return;
    }

    if (button == 0) { // Left button
        editor->mouse_left_down = true;
        editor->mouse_left_just_pressed = true;

        if (editor->ui_wants_mouse || editor_mouse_over_dialog(editor)) {
            return;
        }

        // If in rotation mode, click commits the rotation
        if (editor->rotation_mode) {
            editor_exit_rotation_mode(editor, false);
            return;
        }

        // Apply edit on click
        if (editor->hover_valid) {
            pz_editor_slot *slot = &editor->slots[editor->selected_slot];
            if (slot->type == PZ_EDITOR_SLOT_TAG) {
                // Check if there's an existing tag at this tile
                int existing = pz_map_find_tag_placement(editor->map,
                    editor->hover_tile_x, editor->hover_tile_y, -1);

                if (existing >= 0) {
                    // Click on existing tag enters rotation mode (if rotatable)
                    int tag_idx
                        = editor->map->tag_placements[existing].tag_index;
                    if (tag_idx >= 0 && tag_idx < editor->map->tag_def_count) {
                        pz_tag_def *def = &editor->map->tag_defs[tag_idx];
                        if (editor_tag_supports_rotation(def)) {
                            editor_enter_rotation_mode(editor,
                                editor->hover_tile_x, editor->hover_tile_y);
                            return;
                        }
                    }
                    // Non-rotatable tag clicked - do nothing (don't replace)
                    return;
                }

                // No existing tag - place new one
                editor_place_tag(editor, editor->hover_tile_x,
                    editor->hover_tile_y, slot->tag_name);
            } else {
                // Check if there's an existing entity at this tile
                int existing = pz_map_find_tag_placement(editor->map,
                    editor->hover_tile_x, editor->hover_tile_y, -1);

                if (existing >= 0) {
                    // Click on existing entity enters rotation mode (if
                    // rotatable)
                    int tag_idx
                        = editor->map->tag_placements[existing].tag_index;
                    if (tag_idx >= 0 && tag_idx < editor->map->tag_def_count) {
                        pz_tag_def *def = &editor->map->tag_defs[tag_idx];
                        if (editor_tag_supports_rotation(def)) {
                            editor_enter_rotation_mode(editor,
                                editor->hover_tile_x, editor->hover_tile_y);
                            return;
                        }
                    }
                    // Non-rotatable entity clicked - do nothing
                    return;
                }

                editor_apply_edit(
                    editor, editor->hover_tile_x, editor->hover_tile_y, true);

                // Start paint mode - record the resulting tile state
                if (pz_map_in_bounds(
                        editor->map, editor->hover_tile_x, editor->hover_tile_y)
                    && slot->type == PZ_EDITOR_SLOT_TILE) {
                    pz_map_cell cell = pz_map_get_cell(editor->map,
                        editor->hover_tile_x, editor->hover_tile_y);
                    editor->paint_mode = true;
                    editor->paint_last_tile_x = editor->hover_tile_x;
                    editor->paint_last_tile_y = editor->hover_tile_y;
                    editor->paint_target_height = cell.height;
                    editor->paint_target_tile_index = cell.tile_index;
                    editor->paint_is_raise = true;
                }
            }
        }
    } else if (button == 1) { // Right button
        editor->mouse_right_down = true;
        editor->mouse_right_just_pressed = true;

        if (editor->ui_wants_mouse || editor_mouse_over_dialog(editor)) {
            return;
        }

        // Right-click cancels rotation mode
        if (editor->rotation_mode) {
            editor_exit_rotation_mode(editor, true);
            return;
        }

        // Apply edit on click
        if (editor->hover_valid) {
            pz_editor_slot *slot = &editor->slots[editor->selected_slot];
            // Right-click removes entity at this tile (regardless of slot type)
            int existing = pz_map_find_tag_placement(
                editor->map, editor->hover_tile_x, editor->hover_tile_y, -1);
            if (existing >= 0) {
                pz_map_remove_tag_placement(editor->map, existing);
                editor_mark_tags_dirty(editor);
            } else if (slot->type != PZ_EDITOR_SLOT_TAG) {
                // No entity present, apply height edit for tile slots
                editor_apply_edit(
                    editor, editor->hover_tile_x, editor->hover_tile_y, false);

                // Start paint mode for lowering
                if (pz_map_in_bounds(
                        editor->map, editor->hover_tile_x, editor->hover_tile_y)
                    && slot->type == PZ_EDITOR_SLOT_TILE) {
                    pz_map_cell cell = pz_map_get_cell(editor->map,
                        editor->hover_tile_x, editor->hover_tile_y);
                    editor->paint_mode = true;
                    editor->paint_last_tile_x = editor->hover_tile_x;
                    editor->paint_last_tile_y = editor->hover_tile_y;
                    editor->paint_target_height = cell.height;
                    editor->paint_target_tile_index = cell.tile_index;
                    editor->paint_is_raise = false;
                }
            }
        }
    }
}

void
pz_editor_mouse_up(pz_editor *editor, int button)
{
    if (!editor) {
        return;
    }

    if (button == 0) {
        editor->mouse_left_down = false;
        editor->mouse_left_just_released = true;
        // End paint mode on left button release
        if (editor->paint_mode && editor->paint_is_raise) {
            editor->paint_mode = false;
        }
    } else if (button == 1) {
        editor->mouse_right_down = false;
        // End paint mode on right button release
        if (editor->paint_mode && !editor->paint_is_raise) {
            editor->paint_mode = false;
        }
    }
}

void
pz_editor_scroll(pz_editor *editor, float delta)
{
    if (!editor || !editor->active) {
        return;
    }

    // Handle map settings dialog scrolling
    if (editor->map_settings_dialog_open && editor->map_settings_visible) {
        float dpi_scale = pz_renderer_get_dpi_scale(editor->renderer);
        float mouse_x = editor->mouse_x / dpi_scale;
        float mouse_y = editor->mouse_y / dpi_scale;

        if (editor_point_in_rect(mouse_x, mouse_y,
                editor->map_settings_window_x, editor->map_settings_window_y,
                editor->map_settings_window_w, editor->map_settings_window_h)) {
            float scroll_speed = 30.0f;
            editor->map_settings_scroll -= delta * scroll_speed;

            // Clamp scroll
            if (editor->map_settings_scroll < 0.0f) {
                editor->map_settings_scroll = 0.0f;
            }
            if (editor->map_settings_scroll > editor->map_settings_max_scroll) {
                editor->map_settings_scroll = editor->map_settings_max_scroll;
            }
            return;
        }
    }

    if (editor->ui_wants_mouse || editor_mouse_over_dialog(editor)) {
        return;
    }

    // Cycle through slots with scroll wheel
    if (delta > 0) {
        pz_editor_cycle_slot(editor, 1);
    } else if (delta < 0) {
        pz_editor_cycle_slot(editor, -1);
    }
}

bool
pz_editor_key_down(pz_editor *editor, int keycode, bool repeat)
{
    if (!editor || !editor->active || repeat) {
        return false;
    }

    if (editor->map_name_edit_open) {
        if (keycode == SAPP_KEYCODE_ESCAPE) {
            editor_cancel_map_name_dialog(editor);
            return true;
        }
        if (keycode == SAPP_KEYCODE_ENTER || keycode == SAPP_KEYCODE_KP_ENTER) {
            editor_commit_map_name_dialog(editor);
            return true;
        }
        if (keycode == SAPP_KEYCODE_BACKSPACE) {
            size_t len = strlen(editor->map_name_buffer);
            int cursor = editor->map_name_cursor;
            if (cursor > 0 && len > 0) {
                if ((size_t)cursor > len) {
                    cursor = (int)len;
                }
                memmove(editor->map_name_buffer + cursor - 1,
                    editor->map_name_buffer + cursor, len - (size_t)cursor + 1);
                editor->map_name_cursor = cursor - 1;
                editor->map_name_error[0] = '\0';
            }
            return true;
        }
        if (keycode == SAPP_KEYCODE_DELETE) {
            size_t len = strlen(editor->map_name_buffer);
            int cursor = editor->map_name_cursor;
            if ((size_t)cursor < len) {
                memmove(editor->map_name_buffer + cursor,
                    editor->map_name_buffer + cursor + 1, len - (size_t)cursor);
                editor->map_name_error[0] = '\0';
            }
            return true;
        }
        if (keycode == SAPP_KEYCODE_LEFT) {
            if (editor->map_name_cursor > 0) {
                editor->map_name_cursor--;
            }
            return true;
        }
        if (keycode == SAPP_KEYCODE_RIGHT) {
            size_t len = strlen(editor->map_name_buffer);
            if ((size_t)editor->map_name_cursor < len) {
                editor->map_name_cursor++;
            }
            return true;
        }
        if (keycode == SAPP_KEYCODE_HOME) {
            editor->map_name_cursor = 0;
            return true;
        }
        if (keycode == SAPP_KEYCODE_END) {
            editor->map_name_cursor = (int)strlen(editor->map_name_buffer);
            return true;
        }
        return true;
    }

    if (editor->tag_rename_open) {
        if (keycode == SAPP_KEYCODE_ESCAPE) {
            editor_cancel_tag_rename(editor);
            return true;
        }
        if (keycode == SAPP_KEYCODE_ENTER || keycode == SAPP_KEYCODE_KP_ENTER) {
            editor_commit_tag_rename(editor);
            return true;
        }
        if (keycode == SAPP_KEYCODE_BACKSPACE) {
            size_t len = strlen(editor->tag_rename_buffer);
            int cursor = editor->tag_rename_cursor;
            if (cursor > 0 && len > 0) {
                if ((size_t)cursor > len) {
                    cursor = (int)len;
                }
                memmove(editor->tag_rename_buffer + cursor - 1,
                    editor->tag_rename_buffer + cursor,
                    len - (size_t)cursor + 1);
                editor->tag_rename_cursor = cursor - 1;
                editor->tag_rename_error[0] = '\0';
            }
            return true;
        }
        if (keycode == SAPP_KEYCODE_DELETE) {
            size_t len = strlen(editor->tag_rename_buffer);
            int cursor = editor->tag_rename_cursor;
            if ((size_t)cursor < len) {
                memmove(editor->tag_rename_buffer + cursor,
                    editor->tag_rename_buffer + cursor + 1,
                    len - (size_t)cursor);
                editor->tag_rename_error[0] = '\0';
            }
            return true;
        }
        if (keycode == SAPP_KEYCODE_LEFT) {
            if (editor->tag_rename_cursor > 0) {
                editor->tag_rename_cursor--;
            }
            return true;
        }
        if (keycode == SAPP_KEYCODE_RIGHT) {
            size_t len = strlen(editor->tag_rename_buffer);
            if ((size_t)editor->tag_rename_cursor < len) {
                editor->tag_rename_cursor++;
            }
            return true;
        }
        if (keycode == SAPP_KEYCODE_HOME) {
            editor->tag_rename_cursor = 0;
            return true;
        }
        if (keycode == SAPP_KEYCODE_END) {
            editor->tag_rename_cursor = (int)strlen(editor->tag_rename_buffer);
            return true;
        }
        return true;
    }

    // Escape key - close dialogs or show close confirmation
    if (keycode == 256) { // SAPP_KEYCODE_ESCAPE
        // Cancel rotation mode first
        if (editor->rotation_mode) {
            editor_exit_rotation_mode(editor, true);
            return true;
        }
        // Close dialogs in order of priority
        if (editor->confirm_close_open) {
            editor_close_dialog(editor, &editor->confirm_close_open,
                &editor->confirm_close_window);
            return true;
        }
        if (editor->tile_picker_open) {
            editor_close_dialog(
                editor, &editor->tile_picker_open, &editor->tile_picker_window);
            return true;
        }
        if (editor->tag_editor_open) {
            editor_close_tag_editor(editor);
            return true;
        }
        if (editor->tags_dialog_open) {
            editor_close_dialog(
                editor, &editor->tags_dialog_open, &editor->tags_window);
            return true;
        }
        if (editor->map_settings_dialog_open) {
            editor->map_settings_dialog_open = false;
            return true;
        }
        // No dialogs open - show close confirmation
        editor->confirm_close_window.x = 0;
        editor->confirm_close_window.y = 0;
        editor_open_dialog(
            editor, &editor->confirm_close_open, &editor->confirm_close_window);
        return true;
    }

    // Number keys 1-6 for slot selection or tile assignment
    if (keycode >= 49 && keycode <= 54) { // '1' - '6' (SAPP_KEYCODE_1 = 49)
        int slot = keycode - 49;

        // If tile picker is open and a tile is hovered, assign it to the slot
        if (editor->tile_picker_open
            && editor->tile_picker_hovered_index >= 0) {
            const pz_tile_config *tile = pz_tile_registry_get_by_index(
                editor->tile_registry, editor->tile_picker_hovered_index);
            int tile_def_idx
                = editor_find_or_add_tile_def(editor, tile, "via hotkey");
            if (tile_def_idx >= 0) {
                pz_editor_set_slot_tile(editor, slot, tile_def_idx);
            }
            // Keep picker open to allow more assignments
            return true;
        }

        if (editor->tags_dialog_open && editor->tag_list_hovered_index >= 0
            && editor->map
            && editor->tag_list_hovered_index < editor->map->tag_def_count) {
            const pz_tag_def *def
                = &editor->map->tag_defs[editor->tag_list_hovered_index];
            pz_editor_set_slot_tag(editor, slot, def->name);
            return true;
        }

        // Otherwise just select the slot
        pz_editor_select_slot(editor, slot);
        return true;
    }

    // Tab to toggle between slot 0 and 1
    if (keycode == 258) { // SAPP_KEYCODE_TAB
        int new_slot = (editor->selected_slot == 0) ? 1 : 0;
        pz_editor_select_slot(editor, new_slot);
        return true;
    }

    // Ctrl/Cmd+Z for undo (Phase 5)
    // Ctrl/Cmd+S for save
    if (keycode == 83) { // SAPP_KEYCODE_S
        // Check for ctrl/cmd modifier would be done in main.c
        pz_editor_save(editor);
        return true;
    }

    // T key to toggle tile picker
    if (keycode == 84) { // SAPP_KEYCODE_T
        editor_toggle_dialog(
            editor, &editor->tile_picker_open, &editor->tile_picker_window);
        return true;
    }

    // G key to toggle tags dialog
    if (keycode == 71) { // SAPP_KEYCODE_G
        editor_toggle_dialog(
            editor, &editor->tags_dialog_open, &editor->tags_window);
        return true;
    }

    return false;
}

bool
pz_editor_key_up(pz_editor *editor, int keycode)
{
    (void)editor;
    (void)keycode;
    return false;
}

// ============================================================================
// Rendering
// ============================================================================

void
pz_editor_get_camera(pz_editor *editor, pz_mat4 *view, pz_mat4 *projection,
    int viewport_width, int viewport_height)
{
    if (!editor || !editor->map) {
        return;
    }

    // Add padding to map dimensions
    float padded_width = editor->map->world_width
        + EDITOR_PADDING_TILES * 2 * editor->map->tile_size;
    float padded_height = editor->map->world_height
        + EDITOR_PADDING_TILES * 2 * editor->map->tile_size;

    // Use same camera angle as gameplay (20 degrees from vertical)
    float pitch_degrees = 20.0f;
    float pitch_rad = pitch_degrees * PZ_PI / 180.0f;

    // Calculate height needed to see the map (same logic as pz_camera_fit_map)
    float aspect = (float)viewport_width / (float)viewport_height;
    float fov = 45.0f;
    float fov_rad = fov * PZ_PI / 180.0f;

    // Height needed to fit map width horizontally
    float hfov_rad = 2.0f * atanf(tanf(fov_rad / 2.0f) * aspect);
    float height_for_width = (padded_width / 2.0f) / tanf(hfov_rad / 2.0f);

    // For depth: approximate by treating it as foreshortened
    float cos_pitch = cosf(pitch_rad);
    float apparent_depth = padded_height * cos_pitch;
    float height_for_depth = (apparent_depth / 2.0f) / tanf(fov_rad / 2.0f);

    // Take the larger, add small margin
    float height = fmaxf(height_for_width, height_for_depth) * 1.1f;

    // Camera position - above and behind the center
    float horizontal_dist = height * tanf(pitch_rad);

    // Configure the pz_camera struct
    editor->camera.position = (pz_vec3) {
        editor->camera_offset.x,
        height,
        editor->camera_offset.y + horizontal_dist,
    };
    editor->camera.target = (pz_vec3) {
        editor->camera_offset.x,
        0.0f,
        editor->camera_offset.y,
    };
    editor->camera.up = (pz_vec3) { 0.0f, 1.0f, 0.0f };
    editor->camera.fov = fov;
    editor->camera.aspect = aspect;
    editor->camera.near_plane = 0.1f;
    editor->camera.far_plane = 500.0f;
    editor->camera.viewport_width = viewport_width;
    editor->camera.viewport_height = viewport_height;

    // Update matrices
    pz_camera_update(&editor->camera);

    // Return the matrices
    *view = editor->camera.view;
    *projection = editor->camera.projection;
}

void
pz_editor_render(pz_editor *editor, const pz_mat4 *view_projection)
{
    if (!editor || !editor->active || !editor->map) {
        return;
    }

    // Render grid overlay
    if (editor->grid_pipeline != PZ_INVALID_HANDLE
        && editor->grid_vb != PZ_INVALID_HANDLE
        && editor->grid_vertex_count > 0) {

        // Set uniforms (debug_line_3d uses u_mvp)
        pz_renderer_set_uniform_mat4(
            editor->renderer, editor->grid_shader, "u_mvp", view_projection);

        // Draw grid
        pz_draw_cmd cmd = {
            .pipeline = editor->grid_pipeline,
            .vertex_buffer = editor->grid_vb,
            .index_buffer = 0,
            .vertex_count = editor->grid_vertex_count,
            .index_count = 0,
            .vertex_offset = 0,
            .index_offset = 0,
        };
        pz_renderer_draw(editor->renderer, &cmd);
    }

    // Render hover highlight / ghost preview
    if (editor->hover_valid && editor->hover_pipeline != PZ_INVALID_HANDLE
        && editor->hover_vb != PZ_INVALID_HANDLE) {

        // Calculate tile world position using map's coordinate system
        // The map is centered at world origin, so we need to use tile_to_world
        float tile_size = editor->map->tile_size;
        float half_w = editor->map->world_width / 2.0f;
        float half_h = editor->map->world_height / 2.0f;

        // Convert tile coords to world coords (top-left corner of tile)
        float x0 = editor->hover_tile_x * tile_size - half_w;
        float z0 = editor->hover_tile_y * tile_size - half_h;
        float x1 = x0 + tile_size;
        float z1 = z0 + tile_size;

        // Draw highlight at the floor level of this tile
        // GROUND_Y_OFFSET = -0.01f, WALL_HEIGHT_UNIT = 1.5f
        float y = -0.01f + 0.02f; // Ground level + small offset for z-fighting
        int map_x = editor->hover_tile_x;
        int map_y = editor->hover_tile_y;
        if (map_x >= 0 && map_x < editor->map->width && map_y >= 0
            && map_y < editor->map->height) {
            int cell_idx = map_y * editor->map->width + map_x;
            int8_t h = editor->map->cells[cell_idx].height;
            // Draw at the TOP surface of this tile's floor (base of any wall)
            y = -0.01f + h * 1.5f + 0.02f;

            // If tile is underwater, draw highlight at water surface instead
            if (editor->map->has_water && h < editor->map->water_level) {
                float water_y = -0.01f + editor->map->water_level * 1.5f - 0.5f;
                y = water_y + 0.02f;
            }
        }

        // Choose color based on selected slot type - use bright colors
        float r = 0.0f, g = 1.0f, b = 0.0f, a = 1.0f; // Bright green default
        if (editor->slots[editor->selected_slot].type == PZ_EDITOR_SLOT_TILE) {
            r = 0.0f;
            g = 1.0f;
            b = 1.0f; // Cyan for tiles
        } else if (editor->slots[editor->selected_slot].type
            == PZ_EDITOR_SLOT_TAG) {
            int tag_index = editor_find_tag_def_index(
                editor, editor->slots[editor->selected_slot].tag_name);
            if (tag_index >= 0) {
                pz_vec4 tag_color
                    = editor_tag_color(editor->map->tag_defs[tag_index].type);
                r = tag_color.x;
                g = tag_color.y;
                b = tag_color.z;
            } else {
                r = 1.0f;
                g = 0.5f;
                b = 0.0f; // Fallback orange for tags
            }
        }

        // Build hover highlight vertices (4 lines = 8 vertices)
        float verts[8 * 7] = {
            // Line 1: x0,z0 -> x1,z0
            x0,
            y,
            z0,
            r,
            g,
            b,
            a,
            x1,
            y,
            z0,
            r,
            g,
            b,
            a,
            // Line 2: x1,z0 -> x1,z1
            x1,
            y,
            z0,
            r,
            g,
            b,
            a,
            x1,
            y,
            z1,
            r,
            g,
            b,
            a,
            // Line 3: x1,z1 -> x0,z1
            x1,
            y,
            z1,
            r,
            g,
            b,
            a,
            x0,
            y,
            z1,
            r,
            g,
            b,
            a,
            // Line 4: x0,z1 -> x0,z0
            x0,
            y,
            z1,
            r,
            g,
            b,
            a,
            x0,
            y,
            z0,
            r,
            g,
            b,
            a,
        };

        // Update buffer and draw
        pz_renderer_update_buffer(
            editor->renderer, editor->hover_vb, 0, verts, sizeof(verts));

        pz_draw_cmd hover_cmd = {
            .pipeline = editor->hover_pipeline,
            .vertex_buffer = editor->hover_vb,
            .vertex_count = 8,
        };
        pz_renderer_draw(editor->renderer, &hover_cmd);
    }

    // Render facing direction arrows for all rotatable tags
    // Batch all arrows into a single buffer update
    if (editor->hover_pipeline != PZ_INVALID_HANDLE
        && editor->arrow_vb != PZ_INVALID_HANDLE) {

// Max 64 arrows, each with 6 vertices of 7 floats
#define MAX_ARROWS 64
        float arrow_data[MAX_ARROWS * 6 * 7];
        int arrow_count = 0;

        float tile_size = editor->map->tile_size;
        float head_angle = 0.4f;

        for (int i = 0;
             i < editor->map->tag_placement_count && arrow_count < MAX_ARROWS;
             i++) {
            int tag_idx = editor->map->tag_placements[i].tag_index;
            if (tag_idx < 0 || tag_idx >= editor->map->tag_def_count) {
                continue;
            }

            pz_tag_def *def = &editor->map->tag_defs[tag_idx];
            float *angle_ptr = editor_get_tag_angle(def);
            if (!angle_ptr) {
                continue; // Not rotatable
            }

            float angle = *angle_ptr;
            int tx = editor->map->tag_placements[i].tile_x;
            int ty = editor->map->tag_placements[i].tile_y;
            pz_vec2 tile_world = pz_map_tile_to_world(editor->map, tx, ty);
            float y = editor_get_tile_height(editor, tx, ty) + 0.1f;

            // Arrow parameters depend on whether we're rotating this specific
            // tag
            bool is_rotating = editor->rotation_mode
                && tag_idx == editor->rotation_tag_def_index;
            float arrow_len = is_rotating ? tile_size * 0.7f : tile_size * 0.6f;
            float head_len = is_rotating ? tile_size * 0.25f : tile_size * 0.2f;

            // Color: bright yellow if rotating, white with transparency
            // otherwise
            float ar, ag, ab, aa;
            if (is_rotating) {
                ar = 1.0f;
                ag = 1.0f;
                ab = 0.0f;
                aa = 1.0f;
            } else {
                ar = 1.0f;
                ag = 1.0f;
                ab = 1.0f;
                aa = 0.8f;
            }

            float dx = sinf(angle);
            float dz = cosf(angle);
            float cx = tile_world.x;
            float cz = tile_world.y;
            float tip_x = cx + dx * arrow_len;
            float tip_z = cz + dz * arrow_len;

            float head_dx1 = sinf(angle + PZ_PI - head_angle);
            float head_dz1 = cosf(angle + PZ_PI - head_angle);
            float head_dx2 = sinf(angle + PZ_PI + head_angle);
            float head_dz2 = cosf(angle + PZ_PI + head_angle);

            // Add 6 vertices for this arrow (3 lines)
            float *v = &arrow_data[arrow_count * 6 * 7];

            // Line 1: shaft
            v[0] = cx;
            v[1] = y;
            v[2] = cz;
            v[3] = ar;
            v[4] = ag;
            v[5] = ab;
            v[6] = aa;
            v[7] = tip_x;
            v[8] = y;
            v[9] = tip_z;
            v[10] = ar;
            v[11] = ag;
            v[12] = ab;
            v[13] = aa;
            // Line 2: arrowhead left
            v[14] = tip_x;
            v[15] = y;
            v[16] = tip_z;
            v[17] = ar;
            v[18] = ag;
            v[19] = ab;
            v[20] = aa;
            v[21] = tip_x + head_dx1 * head_len;
            v[22] = y;
            v[23] = tip_z + head_dz1 * head_len;
            v[24] = ar;
            v[25] = ag;
            v[26] = ab;
            v[27] = aa;
            // Line 3: arrowhead right
            v[28] = tip_x;
            v[29] = y;
            v[30] = tip_z;
            v[31] = ar;
            v[32] = ag;
            v[33] = ab;
            v[34] = aa;
            v[35] = tip_x + head_dx2 * head_len;
            v[36] = y;
            v[37] = tip_z + head_dz2 * head_len;
            v[38] = ar;
            v[39] = ag;
            v[40] = ab;
            v[41] = aa;

            arrow_count++;
        }

        if (arrow_count > 0) {
            pz_renderer_update_buffer(editor->renderer, editor->arrow_vb, 0,
                arrow_data, (size_t)(arrow_count * 6 * 7 * sizeof(float)));

            pz_draw_cmd arrow_cmd = {
                .pipeline = editor->hover_pipeline,
                .vertex_buffer = editor->arrow_vb,
                .vertex_count = arrow_count * 6,
            };
            pz_renderer_draw(editor->renderer, &arrow_cmd);
        }
#undef MAX_ARROWS
    }
}

void
pz_editor_render_ui(pz_editor *editor, int screen_width, int screen_height)
{
    if (!editor || !editor->active || !editor->ui) {
        return;
    }

    // Convert framebuffer pixels to logical pixels for UI
    // The font system expects logical coordinates
    float dpi_scale = pz_renderer_get_dpi_scale(editor->renderer);
    int logical_width = (int)(screen_width / dpi_scale);
    int logical_height = (int)(screen_height / dpi_scale);

    // Prepare mouse state for UI (mouse coords are in framebuffer pixels)
    pz_ui_mouse mouse = {
        .x = editor->mouse_x / dpi_scale,
        .y = editor->mouse_y / dpi_scale,
        .left_down = editor->mouse_left_down,
        .left_clicked = editor->mouse_left_just_pressed,
        .left_released = editor->mouse_left_just_released,
    };

    pz_editor_ui_begin(editor->ui, logical_width, logical_height, mouse);

    editor_render_shortcut_bar(editor, logical_width, logical_height);
    editor_render_info_text(editor);
    editor_render_toolbar(editor, logical_width);
    editor_render_tag_overlays(editor, dpi_scale);
    editor_render_dialogs(editor, logical_width, logical_height, dpi_scale);

    pz_editor_ui_end(editor->ui);
    editor->ui_wants_mouse = pz_editor_ui_wants_mouse(editor->ui);

    // Reset just-pressed/released state (consumed by UI)
    editor->mouse_left_just_pressed = false;
    editor->mouse_left_just_released = false;
}

void
pz_editor_render_map(pz_editor *editor, const pz_mat4 *view_projection,
    pz_texture_handle light_texture, float light_scale_x, float light_scale_z,
    float light_offset_x, float light_offset_z)
{
    if (!editor || !editor->active || !editor->map_renderer) {
        return;
    }

    pz_map_render_params params = {
        .light_texture = light_texture,
        .light_scale_x = light_scale_x,
        .light_scale_z = light_scale_z,
        .light_offset_x = light_offset_x,
        .light_offset_z = light_offset_z,
        .has_sun = editor->map->lighting.has_sun,
        .sun_direction = editor->map->lighting.sun_direction,
        .sun_color = editor->map->lighting.sun_color,
        .water_alpha = 0.5f, // Translucent water in editor to see pits below
    };

    pz_map_renderer_draw(editor->map_renderer, view_projection, &params);
}

// ============================================================================
// Slots
// ============================================================================

void
pz_editor_set_slot_tile(pz_editor *editor, int slot, int tile_def_index)
{
    if (!editor || slot < 0 || slot >= PZ_EDITOR_MAX_SLOTS) {
        return;
    }

    editor->slots[slot].type = PZ_EDITOR_SLOT_TILE;
    editor->slots[slot].tile_def_index = tile_def_index;
    editor->slots[slot].tag_name[0] = '\0';
}

void
pz_editor_set_slot_tag(pz_editor *editor, int slot, const char *tag_name)
{
    if (!editor || slot < 0 || slot >= PZ_EDITOR_MAX_SLOTS || !tag_name) {
        return;
    }

    editor->slots[slot].type = PZ_EDITOR_SLOT_TAG;
    editor->slots[slot].tile_def_index = -1;
    strncpy(editor->slots[slot].tag_name, tag_name,
        sizeof(editor->slots[slot].tag_name) - 1);
    editor->slots[slot].tag_name[sizeof(editor->slots[slot].tag_name) - 1]
        = '\0';
}

void
pz_editor_clear_slot(pz_editor *editor, int slot)
{
    if (!editor || slot < 0 || slot >= PZ_EDITOR_MAX_SLOTS) {
        return;
    }

    editor->slots[slot].type = PZ_EDITOR_SLOT_EMPTY;
    editor->slots[slot].tag_name[0] = '\0';
    editor->slots[slot].tile_def_index = -1;
}

void
pz_editor_select_slot(pz_editor *editor, int slot)
{
    if (!editor || slot < 0 || slot >= PZ_EDITOR_MAX_SLOTS) {
        return;
    }

    editor->selected_slot = slot;
}

void
pz_editor_cycle_slot(pz_editor *editor, int direction)
{
    if (!editor) {
        return;
    }

    // Find next populated slot in direction
    int start = editor->selected_slot;
    int slot = start;

    for (int i = 0; i < PZ_EDITOR_MAX_SLOTS; i++) {
        slot = (slot + direction + PZ_EDITOR_MAX_SLOTS) % PZ_EDITOR_MAX_SLOTS;
        if (editor->slots[slot].type != PZ_EDITOR_SLOT_EMPTY) {
            editor->selected_slot = slot;
            return;
        }
    }

    // No populated slots found, stay at current
}

// ============================================================================
// Map Access
// ============================================================================

pz_map *
pz_editor_get_map(pz_editor *editor)
{
    return editor ? editor->map : NULL;
}

pz_map_renderer *
pz_editor_get_map_renderer(pz_editor *editor)
{
    return editor ? editor->map_renderer : NULL;
}

void
pz_editor_save(pz_editor *editor)
{
    if (!editor || !editor->map || editor->map_path[0] == '\0') {
        return;
    }

    if (pz_map_save(editor->map, editor->map_path)) {
        editor->dirty = false;
        editor->last_save_time = pz_time_now();
        pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Map saved: %s", editor->map_path);
    } else {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME, "Failed to save: %s",
            editor->map_path);
    }
}

void
pz_editor_set_background(pz_editor *editor, pz_background *background)
{
    if (!editor) {
        return;
    }

    editor->background = background;
    editor_refresh_background(editor);
}

// ============================================================================
// UI Helpers
// ============================================================================

static pz_vec4
editor_preview_color_for_index(int idx)
{
    return (pz_vec4) {
        0.3f + 0.15f * (idx % 3),
        0.25f + 0.15f * ((idx + 1) % 4),
        0.4f + 0.15f * ((idx + 2) % 3),
        1.0f,
    };
}

static int
editor_find_or_add_tile_def(
    pz_editor *editor, const pz_tile_config *tile, const char *context)
{
    if (!editor || !editor->map || !tile || !tile->valid) {
        return -1;
    }

    int tile_def_idx = -1;
    for (int j = 0; j < editor->map->tile_def_count; j++) {
        if (strcmp(editor->map->tile_defs[j].name, tile->name) == 0) {
            tile_def_idx = j;
            break;
        }
    }

    if (tile_def_idx < 0
        && editor->map->tile_def_count < PZ_MAP_MAX_TILE_DEFS) {
        tile_def_idx = editor->map->tile_def_count;
        pz_tile_def *new_def = &editor->map->tile_defs[tile_def_idx];

        memset(new_def, 0, sizeof(*new_def));
        strncpy(new_def->name, tile->name, sizeof(new_def->name) - 1);
        new_def->name[sizeof(new_def->name) - 1] = '\0';
        new_def->symbol = (tile->name[0] != '\0') ? tile->name[0] : '?';

        editor->map->tile_def_count++;

        if (context) {
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                "Added tile def %d: '%s' (%s)", tile_def_idx, new_def->name,
                context);
        } else {
            pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Added tile def %d: '%s'",
                tile_def_idx, new_def->name);
        }

        if (editor->map_renderer) {
            pz_map_renderer_set_map(editor->map_renderer, editor->map);
        }
    }

    return tile_def_idx;
}

static int
editor_render_slot_widget(
    pz_editor *editor, float x, float y, float size, int slot_index)
{
    bool selected = (slot_index == editor->selected_slot);
    bool filled = (editor->slots[slot_index].type != PZ_EDITOR_SLOT_EMPTY);

    char label[4];
    snprintf(label, sizeof(label), "%d", slot_index + 1);

    if (filled && editor->slots[slot_index].type == PZ_EDITOR_SLOT_TILE) {
        int idx = editor->slots[slot_index].tile_def_index;
        const char *tile_name = NULL;
        pz_texture_handle wall_tex = PZ_INVALID_HANDLE;
        pz_texture_handle ground_tex = PZ_INVALID_HANDLE;

        if (idx >= 0 && idx < editor->map->tile_def_count) {
            tile_name = editor->map->tile_defs[idx].name;

            if (editor->tile_registry && tile_name && tile_name[0]) {
                const pz_tile_config *config
                    = pz_tile_registry_get(editor->tile_registry, tile_name);
                if (config) {
                    wall_tex = config->wall_texture;
                    ground_tex = config->ground_texture;
                }
            }
        }

        if (wall_tex != PZ_INVALID_HANDLE && ground_tex != PZ_INVALID_HANDLE) {
            return pz_ui_slot_textured(
                editor->ui, x, y, size, selected, label, wall_tex, ground_tex);
        }

        pz_vec4 preview_color = editor_preview_color_for_index(idx);
        return pz_ui_slot(editor->ui, x, y, size, selected, filled, label,
            tile_name, preview_color);
    }

    if (filled && editor->slots[slot_index].type == PZ_EDITOR_SLOT_TAG) {
        const char *content_label = editor->slots[slot_index].tag_name;
        int tag_index = editor_find_tag_def_index(editor, content_label);
        if (tag_index >= 0) {
            pz_vec4 preview_color
                = editor_tag_color(editor->map->tag_defs[tag_index].type);
            return pz_ui_slot(editor->ui, x, y, size, selected, filled, label,
                content_label, preview_color);
        }
        pz_vec4 preview_color = { 0.35f, 0.35f, 0.35f, 1.0f };
        return pz_ui_slot(editor->ui, x, y, size, selected, filled, label,
            content_label, preview_color);
    }

    pz_vec4 preview_color = { 0.4f, 0.4f, 0.4f, 1.0f };
    return pz_ui_slot(
        editor->ui, x, y, size, selected, filled, label, NULL, preview_color);
}

static void
editor_render_shortcut_bar(
    pz_editor *editor, int logical_width, int logical_height)
{
    float slot_size = 48.0f;
    float slot_spacing = 4.0f;
    float bar_width = PZ_EDITOR_MAX_SLOTS * slot_size
        + (PZ_EDITOR_MAX_SLOTS - 1) * slot_spacing;
    float bar_x = (logical_width - bar_width) / 2.0f;
    float bar_y = logical_height - slot_size - 16.0f;

    pz_ui_rect(editor->ui, bar_x - 8, bar_y - 8, bar_width + 16, slot_size + 16,
        (pz_vec4) { 0.1f, 0.1f, 0.12f, 0.8f });

    for (int i = 0; i < PZ_EDITOR_MAX_SLOTS; i++) {
        float x = bar_x + i * (slot_size + slot_spacing);
        int result = editor_render_slot_widget(editor, x, bar_y, slot_size, i);
        if (result & PZ_UI_CLICKED) {
            pz_editor_select_slot(editor, i);
        }
    }
}

static void
editor_render_info_text(pz_editor *editor)
{
    char info[128];
    if (editor->hover_valid) {
        snprintf(info, sizeof(info), "Tile: %d,%d", editor->hover_tile_x,
            editor->hover_tile_y);
    } else {
        snprintf(info, sizeof(info), "Tile: --");
    }
    pz_ui_label(editor->ui, 16, 16, info, (pz_vec4) { 1.0f, 1.0f, 1.0f, 0.8f });

    pz_ui_label(editor->ui, 16, 36,
        "LMB: Place/Raise | RMB: Lower | 1-6: Select Slot | Tab: Toggle Slots",
        (pz_vec4) { 0.7f, 0.7f, 0.7f, 0.6f });
}

static void
editor_render_toolbar(pz_editor *editor, int logical_width)
{
    float toolbar_x = logical_width - 70;
    float toolbar_y = 16;
    float btn_w = 54;
    float btn_h = 24;
    float btn_spacing = 4;

    // Save button - shows asterisk when dirty
    const char *save_label = editor->dirty ? "SAVE*" : "SAVE";
    if (pz_ui_button(
            editor->ui, toolbar_x, toolbar_y, btn_w, btn_h, save_label)) {
        pz_editor_save(editor);
    }
    toolbar_y += btn_h + btn_spacing;

    // Auto-save toggle - custom colors based on state
    {
        const pz_ui_colors *colors = pz_ui_get_colors(editor->ui);
        float dpi = pz_renderer_get_dpi_scale(editor->renderer);
        float mx = editor->mouse_x / dpi;
        float my = editor->mouse_y / dpi;
        bool hovered = mx >= toolbar_x && mx < toolbar_x + btn_w
            && my >= toolbar_y && my < toolbar_y + btn_h;

        pz_vec4 bg_color = editor->auto_save_enabled ? colors->button_border
                                                     : colors->button_bg;
        if (hovered && !editor->auto_save_enabled) {
            bg_color = colors->button_hover;
        }

        pz_ui_rect(editor->ui, toolbar_x, toolbar_y, btn_w, btn_h, bg_color);
        pz_ui_rect_outline(editor->ui, toolbar_x, toolbar_y, btn_w, btn_h,
            colors->button_border, 1.0f);
        pz_ui_label_centered(editor->ui, toolbar_x, toolbar_y, btn_w, btn_h,
            "AUTO", colors->text);

        if (hovered && editor->mouse_left_just_pressed) {
            editor->auto_save_enabled = !editor->auto_save_enabled;
            pz_ui_consume_mouse(editor->ui);
        }
    }
    toolbar_y += btn_h + btn_spacing;

    if (pz_ui_button(editor->ui, toolbar_x, toolbar_y, btn_w, btn_h, "TAGS")) {
        editor_toggle_dialog(
            editor, &editor->tags_dialog_open, &editor->tags_window);
    }
    toolbar_y += btn_h + btn_spacing;

    if (pz_ui_button(editor->ui, toolbar_x, toolbar_y, btn_w, btn_h, "MAP")) {
        // Reset scroll when opening the dialog
        if (!editor->map_settings_dialog_open) {
            editor->map_settings_scroll = 0.0f;
            editor->map_settings_visible = false;
        }
        editor_toggle_dialog(editor, &editor->map_settings_dialog_open,
            &editor->map_settings_window);
    }
    toolbar_y += btn_h + btn_spacing;

    if (pz_ui_button(editor->ui, toolbar_x, toolbar_y, btn_w, btn_h, "TILES")) {
        editor_toggle_dialog(
            editor, &editor->tile_picker_open, &editor->tile_picker_window);
    }
}

static void
editor_render_tag_overlays(pz_editor *editor, float dpi_scale)
{
    if (!editor || !editor->map || editor->map->tag_placement_count <= 0) {
        return;
    }

    float box_w = 28.0f;
    float box_h = 16.0f;

    for (int i = 0; i < editor->map->tag_placement_count; i++) {
        const pz_tag_placement *placement = &editor->map->tag_placements[i];
        if (placement->tag_index < 0
            || placement->tag_index >= editor->map->tag_def_count) {
            continue;
        }

        const pz_tag_def *def = &editor->map->tag_defs[placement->tag_index];
        pz_vec2 world = pz_map_tile_to_world(
            editor->map, placement->tile_x, placement->tile_y);
        float height = editor_get_tile_height(
            editor, placement->tile_x, placement->tile_y);
        pz_vec3 screen = pz_camera_world_to_screen(
            &editor->camera, (pz_vec3) { world.x, height + 0.3f, world.y });

        if (screen.z < 0.0f || screen.z > 1.0f) {
            continue;
        }

        float sx = screen.x / dpi_scale;
        float sy = screen.y / dpi_scale;
        pz_vec4 color = editor_tag_color(def->type);
        color.w = 0.7f;

        pz_ui_rect(editor->ui, sx - box_w * 0.5f, sy - box_h * 0.5f, box_w,
            box_h, color);
        pz_ui_rect_outline(editor->ui, sx - box_w * 0.5f, sy - box_h * 0.5f,
            box_w, box_h, (pz_vec4) { 0, 0, 0, 0.6f }, 1.0f);
        pz_ui_label_centered(editor->ui, sx - box_w * 0.5f, sy - box_h * 0.5f,
            box_w, box_h, def->name, (pz_vec4) { 1.0f, 1.0f, 1.0f, 0.95f });
    }
}

static void
editor_render_dialogs(
    pz_editor *editor, int logical_width, int logical_height, float dpi_scale)
{
    typedef struct editor_window_entry {
        pz_window_state *state;
        float w;
        float h;
        void (*render)(pz_editor *editor, int logical_width, int logical_height,
            bool allow_input);
    } editor_window_entry;

    editor_window_entry entries[7];
    int entry_count = 0;

    editor->map_settings_visible = false;

    if (editor->tags_dialog_open) {
        entries[entry_count++] = (editor_window_entry) {
            .state = &editor->tags_window,
            .w = EDITOR_TAGS_DIALOG_W,
            .h = EDITOR_TAGS_DIALOG_H,
            .render = editor_render_tags_dialog,
        };
    }

    if (editor->tile_picker_open) {
        entries[entry_count++] = (editor_window_entry) {
            .state = &editor->tile_picker_window,
            .w = EDITOR_TILE_PICKER_W,
            .h = EDITOR_TILE_PICKER_H,
            .render = editor_render_tile_picker,
        };
    }

    if (editor->map_settings_dialog_open) {
        entries[entry_count++] = (editor_window_entry) {
            .state = &editor->map_settings_window,
            .w = EDITOR_MAP_SETTINGS_W,
            .h = EDITOR_MAP_SETTINGS_H,
            .render = editor_render_map_settings_dialog,
        };
    }

    if (editor->tag_editor_open) {
        entries[entry_count++] = (editor_window_entry) {
            .state = &editor->tag_editor_window,
            .w = EDITOR_TAGS_DIALOG_W,
            .h = EDITOR_TAGS_DIALOG_H,
            .render = editor_render_tag_editor_dialog,
        };
    }

    if (editor->tag_rename_open) {
        entries[entry_count++] = (editor_window_entry) {
            .state = &editor->tag_rename_window,
            .w = EDITOR_TAG_RENAME_W,
            .h = EDITOR_TAG_RENAME_H,
            .render = editor_render_tag_rename_dialog,
        };
    }

    if (editor->map_name_edit_open) {
        entries[entry_count++] = (editor_window_entry) {
            .state = &editor->map_name_window,
            .w = EDITOR_NAME_DIALOG_W,
            .h = EDITOR_NAME_DIALOG_H,
            .render = editor_render_map_name_dialog,
        };
    }

    if (editor->confirm_close_open) {
        entries[entry_count++] = (editor_window_entry) {
            .state = &editor->confirm_close_window,
            .w = EDITOR_CONFIRM_CLOSE_W,
            .h = EDITOR_CONFIRM_CLOSE_H,
            .render = editor_render_confirm_close,
        };
    }

    if (entry_count == 0) {
        return;
    }

    for (int i = 0; i < entry_count; i++) {
        if (entries[i].state->z_order == 0) {
            entries[i].state->z_order = ++editor->window_z_counter;
        }
    }

    float mouse_x = editor->mouse_x / dpi_scale;
    float mouse_y = editor->mouse_y / dpi_scale;

    pz_window_state *active_state = NULL;
    int active_z = -1;

    for (int i = 0; i < entry_count; i++) {
        if (entries[i].state->dragging
            && entries[i].state->z_order > active_z) {
            active_state = entries[i].state;
            active_z = entries[i].state->z_order;
        }
    }

    if (!active_state) {
        for (int i = 0; i < entry_count; i++) {
            float win_x = 0.0f;
            float win_y = 0.0f;
            editor_window_rect(entries[i].state, entries[i].w, entries[i].h,
                logical_width, logical_height, &win_x, &win_y);

            if (editor_point_in_rect(
                    mouse_x, mouse_y, win_x, win_y, entries[i].w, entries[i].h)
                && entries[i].state->z_order > active_z) {
                active_state = entries[i].state;
                active_z = entries[i].state->z_order;
            }
        }
    }

    if (editor->mouse_left_just_pressed && active_state) {
        active_state->z_order = ++editor->window_z_counter;
        active_z = active_state->z_order;
    }

    for (int i = 0; i < entry_count - 1; i++) {
        for (int j = i + 1; j < entry_count; j++) {
            if (entries[j].state->z_order < entries[i].state->z_order) {
                editor_window_entry temp = entries[i];
                entries[i] = entries[j];
                entries[j] = temp;
            }
        }
    }

    for (int i = 0; i < entry_count; i++) {
        bool allow_input = entries[i].state == active_state;
        pz_editor_ui_set_input_enabled(editor->ui, allow_input);
        entries[i].render(editor, logical_width, logical_height, allow_input);
        pz_editor_ui_set_input_enabled(editor->ui, true);
    }
}

static void
editor_toggle_dialog(pz_editor *editor, bool *open, pz_window_state *state)
{
    if (!editor || !open || !state) {
        return;
    }

    if (*open) {
        *open = false;
        state->dragging = false;
        state->z_order = 0;
    } else {
        *open = true;
        state->z_order = ++editor->window_z_counter;
    }
}

static void
editor_open_dialog(pz_editor *editor, bool *open, pz_window_state *state)
{
    if (!editor || !open || !state) {
        return;
    }

    if (!*open) {
        *open = true;
        state->z_order = ++editor->window_z_counter;
    }
}

static void
editor_close_dialog(pz_editor *editor, bool *open, pz_window_state *state)
{
    if (!editor || !open || !state) {
        return;
    }

    if (*open) {
        *open = false;
        state->dragging = false;
        state->z_order = 0;
    }
}

static bool
editor_point_in_rect(float x, float y, float rx, float ry, float rw, float rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void
editor_window_rect(const pz_window_state *state, float w, float h, int screen_w,
    int screen_h, float *out_x, float *out_y)
{
    float x = 0.0f;
    float y = 0.0f;

    if (!state) {
        if (out_x)
            *out_x = 0.0f;
        if (out_y)
            *out_y = 0.0f;
        return;
    }

    x = state->x;
    y = state->y;

    if (x == 0.0f && y == 0.0f) {
        x = (screen_w - w) / 2.0f;
        y = (screen_h - h) / 2.0f;
    }

    if (x < 0.0f)
        x = 0.0f;
    if (y < 0.0f)
        y = 0.0f;
    if (x + w > screen_w)
        x = screen_w - w;
    if (y + h > screen_h)
        y = screen_h - h;

    if (out_x)
        *out_x = x;
    if (out_y)
        *out_y = y;
}

// ============================================================================
// Internal Helpers
// ============================================================================

static float
editor_calculate_zoom(pz_editor *editor)
{
    if (!editor || !editor->map) {
        return 1.0f;
    }

    // Calculate zoom based on map size vs reference size
    float ref_width = EDITOR_REFERENCE_WIDTH * editor->map->tile_size;
    float ref_height = EDITOR_REFERENCE_HEIGHT * editor->map->tile_size;

    // Add padding
    float padded_width = editor->map->world_width
        + EDITOR_PADDING_TILES * 2 * editor->map->tile_size;
    float padded_height = editor->map->world_height
        + EDITOR_PADDING_TILES * 2 * editor->map->tile_size;

    // Zoom out if map is larger than reference
    float zoom_x = padded_width / ref_width;
    float zoom_y = padded_height / ref_height;
    float zoom = fmaxf(zoom_x, zoom_y);

    // Don't zoom in for small maps
    if (zoom < 1.0f) {
        zoom = 1.0f;
    }

    return zoom;
}

static void
editor_init_default_slots(pz_editor *editor)
{
    if (!editor || !editor->map) {
        return;
    }

    // Slot 0: ground (first tile def, usually '.')
    if (editor->map->tile_def_count > 0) {
        pz_editor_set_slot_tile(editor, 0, 0);
    }

    // Slot 1: wall (second tile def, usually '#')
    if (editor->map->tile_def_count > 1) {
        pz_editor_set_slot_tile(editor, 1, 1);
    }

    // Clear remaining slots
    for (int i = 2; i < PZ_EDITOR_MAX_SLOTS; i++) {
        pz_editor_clear_slot(editor, i);
    }
}

static int
editor_find_tag_def_index(pz_editor *editor, const char *tag_name)
{
    if (!editor || !editor->map || !tag_name || !tag_name[0]) {
        return -1;
    }

    return pz_map_find_tag_def(editor->map, tag_name);
}

static pz_vec4
editor_tag_color(pz_tag_type type)
{
    switch (type) {
    case PZ_TAG_SPAWN:
        return (pz_vec4) { 0.25f, 0.55f, 0.95f, 1.0f };
    case PZ_TAG_ENEMY:
        return (pz_vec4) { 0.95f, 0.3f, 0.3f, 1.0f };
    case PZ_TAG_POWERUP:
        return (pz_vec4) { 0.95f, 0.8f, 0.2f, 1.0f };
    case PZ_TAG_BARRIER:
        return (pz_vec4) { 0.6f, 0.5f, 0.35f, 1.0f };
    default:
        return (pz_vec4) { 0.7f, 0.7f, 0.7f, 1.0f };
    }
}

static pz_font *
editor_get_ui_font(pz_editor *editor)
{
    if (!editor || !editor->font_mgr) {
        return NULL;
    }

    pz_font *font = pz_font_get(editor->font_mgr, "RussoOne-Regular");
    if (!font) {
        font = pz_font_get(editor->font_mgr, "CaveatBrush-Regular");
    }
    return font;
}

static void
editor_open_tag_editor(pz_editor *editor, int tag_index)
{
    if (!editor || !editor->map) {
        return;
    }
    if (tag_index < 0 || tag_index >= editor->map->tag_def_count) {
        return;
    }

    editor->tag_editor_index = tag_index;
    editor_open_dialog(
        editor, &editor->tag_editor_open, &editor->tag_editor_window);
}

static void
editor_close_tag_editor(pz_editor *editor)
{
    if (!editor) {
        return;
    }

    editor_close_dialog(
        editor, &editor->tag_editor_open, &editor->tag_editor_window);
    editor->tag_editor_index = -1;
}

static bool
editor_tag_name_valid_char(uint32_t codepoint)
{
    if (codepoint > 0x7F) {
        return false;
    }

    unsigned char ch = (unsigned char)codepoint;
    return (ch == '_') || isalnum(ch);
}

static bool
editor_tag_name_is_valid(const char *name)
{
    if (!name || !name[0]) {
        return false;
    }

    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (!editor_tag_name_valid_char(*p)) {
            return false;
        }
    }

    return true;
}

static bool
editor_tag_name_is_unique(pz_editor *editor, const char *name, int ignore_index)
{
    if (!editor || !editor->map || !name) {
        return false;
    }

    for (int i = 0; i < editor->map->tag_def_count; i++) {
        if (i == ignore_index) {
            continue;
        }
        if (strcmp(editor->map->tag_defs[i].name, name) == 0) {
            return false;
        }
    }

    return true;
}

static void
editor_open_tag_rename(pz_editor *editor, int tag_index)
{
    if (!editor || !editor->map || tag_index < 0
        || tag_index >= editor->map->tag_def_count) {
        return;
    }

    editor->tag_rename_open = true;
    editor->tag_rename_index = tag_index;
    editor->tag_rename_window.x = 0.0f;
    editor->tag_rename_window.y = 0.0f;
    editor->tag_rename_window.z_order = ++editor->window_z_counter;
    strncpy(editor->tag_rename_buffer, editor->map->tag_defs[tag_index].name,
        sizeof(editor->tag_rename_buffer) - 1);
    editor->tag_rename_buffer[sizeof(editor->tag_rename_buffer) - 1] = '\0';
    editor->tag_rename_cursor = (int)strlen(editor->tag_rename_buffer);
    editor->tag_rename_error[0] = '\0';
}

static void
editor_cancel_tag_rename(pz_editor *editor)
{
    if (!editor) {
        return;
    }

    editor->tag_rename_open = false;
    editor->tag_rename_index = -1;
    editor->tag_rename_window.dragging = false;
    editor->tag_rename_window.z_order = 0;
    editor->tag_rename_error[0] = '\0';
}

static void
editor_commit_tag_rename(pz_editor *editor)
{
    if (!editor || !editor->map || !editor->tag_rename_open) {
        return;
    }

    int idx = editor->tag_rename_index;
    if (idx < 0 || idx >= editor->map->tag_def_count) {
        editor_cancel_tag_rename(editor);
        return;
    }

    const char *old_name = editor->map->tag_defs[idx].name;
    const char *new_name = editor->tag_rename_buffer;

    if (strcmp(old_name, new_name) == 0) {
        editor_cancel_tag_rename(editor);
        return;
    }

    if (!editor_tag_name_is_valid(new_name)) {
        snprintf(editor->tag_rename_error, sizeof(editor->tag_rename_error),
            "Use A-Z, 0-9, and _ only");
        return;
    }

    if (!editor_tag_name_is_unique(editor, new_name, idx)) {
        snprintf(editor->tag_rename_error, sizeof(editor->tag_rename_error),
            "Tag name already exists");
        return;
    }

    char previous_name[32];
    strncpy(previous_name, old_name, sizeof(previous_name) - 1);
    previous_name[sizeof(previous_name) - 1] = '\0';

    strncpy(editor->map->tag_defs[idx].name, new_name,
        sizeof(editor->map->tag_defs[idx].name) - 1);
    editor->map->tag_defs[idx].name[sizeof(editor->map->tag_defs[idx].name) - 1]
        = '\0';

    for (int s = 0; s < PZ_EDITOR_MAX_SLOTS; s++) {
        if (editor->slots[s].type == PZ_EDITOR_SLOT_TAG
            && strcmp(editor->slots[s].tag_name, previous_name) == 0) {
            strncpy(editor->slots[s].tag_name, new_name,
                sizeof(editor->slots[s].tag_name) - 1);
            editor->slots[s].tag_name[sizeof(editor->slots[s].tag_name) - 1]
                = '\0';
        }
    }

    for (int i = 0; i < editor->map->tag_def_count; i++) {
        pz_tag_def *def = &editor->map->tag_defs[i];
        if (def->type == PZ_TAG_POWERUP
            && strcmp(def->data.powerup.barrier_tag, previous_name) == 0) {
            strncpy(def->data.powerup.barrier_tag, new_name,
                sizeof(def->data.powerup.barrier_tag) - 1);
            def->data.powerup
                .barrier_tag[sizeof(def->data.powerup.barrier_tag) - 1]
                = '\0';
        }
    }

    editor_mark_tags_dirty(editor);
    editor_cancel_tag_rename(editor);
}

static void
editor_handle_tag_char_input(pz_editor *editor, uint32_t codepoint)
{
    if (!editor || !editor->tag_rename_open) {
        return;
    }

    if (!editor_tag_name_valid_char(codepoint)) {
        return;
    }

    size_t len = strlen(editor->tag_rename_buffer);
    if (len >= sizeof(editor->tag_rename_buffer) - 1) {
        return;
    }

    int cursor = editor->tag_rename_cursor;
    if (cursor < 0) {
        cursor = 0;
    }
    if ((size_t)cursor > len) {
        cursor = (int)len;
    }

    memmove(editor->tag_rename_buffer + cursor + 1,
        editor->tag_rename_buffer + cursor, len - (size_t)cursor + 1);
    editor->tag_rename_buffer[cursor] = (char)codepoint;
    editor->tag_rename_cursor = cursor + 1;
    editor->tag_rename_error[0] = '\0';
}

static void
editor_mark_tags_dirty(pz_editor *editor)
{
    if (!editor || !editor->map) {
        return;
    }

    pz_map_rebuild_spawns_from_tags(editor->map);
    editor_mark_dirty(editor);
}

// Check if a tag def supports rotation (spawn or enemy types)
static bool
editor_tag_supports_rotation(const pz_tag_def *def)
{
    if (!def) {
        return false;
    }
    return def->type == PZ_TAG_SPAWN || def->type == PZ_TAG_ENEMY;
}

// Get pointer to angle for a tag def (or NULL if not rotatable)
static float *
editor_get_tag_angle(pz_tag_def *def)
{
    if (!def) {
        return NULL;
    }
    if (def->type == PZ_TAG_SPAWN) {
        return &def->data.spawn.angle;
    }
    if (def->type == PZ_TAG_ENEMY) {
        return &def->data.enemy.angle;
    }
    return NULL;
}

// Enter rotation mode for a tag at the given tile
static void
editor_enter_rotation_mode(pz_editor *editor, int tile_x, int tile_y)
{
    if (!editor || !editor->map) {
        return;
    }

    // Find the tag placement at this tile
    int placement_idx
        = pz_map_find_tag_placement(editor->map, tile_x, tile_y, -1);
    if (placement_idx < 0) {
        return;
    }

    int tag_def_idx = editor->map->tag_placements[placement_idx].tag_index;
    if (tag_def_idx < 0 || tag_def_idx >= editor->map->tag_def_count) {
        return;
    }

    pz_tag_def *def = &editor->map->tag_defs[tag_def_idx];
    if (!editor_tag_supports_rotation(def)) {
        return;
    }

    float *angle = editor_get_tag_angle(def);
    if (!angle) {
        return;
    }

    editor->rotation_mode = true;
    editor->rotation_tag_def_index = tag_def_idx;
    editor->rotation_start_angle = *angle;
}

// Exit rotation mode (commit the angle)
static void
editor_exit_rotation_mode(pz_editor *editor, bool cancel)
{
    if (!editor || !editor->rotation_mode) {
        return;
    }

    if (cancel && editor->rotation_tag_def_index >= 0
        && editor->rotation_tag_def_index < editor->map->tag_def_count) {
        // Restore original angle
        pz_tag_def *def
            = &editor->map->tag_defs[editor->rotation_tag_def_index];
        float *angle = editor_get_tag_angle(def);
        if (angle) {
            *angle = editor->rotation_start_angle;
        }
    } else if (!cancel) {
        // Mark dirty since we changed the angle
        editor_mark_tags_dirty(editor);
    }

    editor->rotation_mode = false;
    editor->rotation_tag_def_index = -1;
}

// Update rotation based on mouse position relative to tile center
static void
editor_update_rotation(pz_editor *editor)
{
    if (!editor || !editor->rotation_mode || !editor->map) {
        return;
    }

    if (editor->rotation_tag_def_index < 0
        || editor->rotation_tag_def_index >= editor->map->tag_def_count) {
        return;
    }

    // Find all placements of this tag def to determine center
    // (use the first placement we find for rotation center)
    int tile_x = -1, tile_y = -1;
    for (int i = 0; i < editor->map->tag_placement_count; i++) {
        if (editor->map->tag_placements[i].tag_index
            == editor->rotation_tag_def_index) {
            tile_x = editor->map->tag_placements[i].tile_x;
            tile_y = editor->map->tag_placements[i].tile_y;
            break;
        }
    }

    if (tile_x < 0 || tile_y < 0) {
        return;
    }

    // Get world position of tile center
    pz_vec2 tile_world = pz_map_tile_to_world(editor->map, tile_x, tile_y);

    // Get mouse world position via ray-ground intersection
    pz_vec3 ray_dir = pz_camera_screen_to_ray(
        &editor->camera, (int)editor->mouse_x, (int)editor->mouse_y);
    pz_vec3 ray_origin = editor->camera.position;

    // Intersect with ground plane (y = tile height)
    float tile_height = editor_get_tile_height(editor, tile_x, tile_y);
    if (fabsf(ray_dir.y) < 0.0001f) {
        return;
    }
    float t = (tile_height - ray_origin.y) / ray_dir.y;
    if (t < 0.0f) {
        return;
    }
    float mouse_world_x = ray_origin.x + ray_dir.x * t;
    float mouse_world_z = ray_origin.z + ray_dir.z * t;

    // Calculate angle from tile center to mouse
    float dx = mouse_world_x - tile_world.x;
    float dz = mouse_world_z - tile_world.y; // Note: tile_world.y is z-coord

    // atan2 gives angle where 0 = +X axis, increasing counterclockwise
    // For tank facing: 0 = facing +Z (down in screen space), positive = CCW
    // So we use atan2(dx, dz) for angle where 0 = +Z
    float angle = atan2f(dx, dz);

    // Update the tag def's angle
    pz_tag_def *def = &editor->map->tag_defs[editor->rotation_tag_def_index];
    float *def_angle = editor_get_tag_angle(def);
    if (def_angle) {
        *def_angle = angle;
    }
}

static void
editor_place_tag(
    pz_editor *editor, int tile_x, int tile_y, const char *tag_name)
{
    if (!editor || !editor->map || !tag_name || !tag_name[0]) {
        return;
    }

    // Check if we need to expand the map
    if (!pz_map_in_bounds(editor->map, tile_x, tile_y)) {
        // Check if within expansion zone
        int padded_min_x = -EDITOR_PADDING_TILES;
        int padded_max_x = editor->map->width + EDITOR_PADDING_TILES - 1;
        int padded_min_y = -EDITOR_PADDING_TILES;
        int padded_max_y = editor->map->height + EDITOR_PADDING_TILES - 1;

        bool in_expansion_zone = tile_x >= padded_min_x
            && tile_x <= padded_max_x && tile_y >= padded_min_y
            && tile_y <= padded_max_y;

        if (!in_expansion_zone) {
            return;
        }

        // Expand the map
        int offset_x = 0, offset_y = 0;
        if (!editor_expand_map_to_include(
                editor, tile_x, tile_y, &offset_x, &offset_y)) {
            return;
        }

        // Adjust tile coordinates after expansion
        tile_x += offset_x;
        tile_y += offset_y;

        // Recalculate camera zoom for new map size
        editor->camera_zoom = editor_calculate_zoom(editor);

        // Rebuild grid and map renderer (expansion changed map dimensions)
        editor_rebuild_grid(editor);
        if (editor->map_renderer) {
            pz_map_renderer_set_map(editor->map_renderer, editor->map);
        }
    }

    int tag_index = pz_map_find_tag_def(editor->map, tag_name);
    if (tag_index < 0) {
        return;
    }

    // Editor v1 limitation: one tag per cell; remove any existing placement.
    int existing = pz_map_find_tag_placement(editor->map, tile_x, tile_y, -1);
    if (existing >= 0) {
        pz_map_remove_tag_placement(editor->map, existing);
    }

    const pz_tag_def *def = &editor->map->tag_defs[tag_index];
    if (def->type == PZ_TAG_SPAWN) {
        // Spawn tags are single-placement; move existing one.
        for (int i = 0; i < editor->map->tag_placement_count;) {
            if (editor->map->tag_placements[i].tag_index == tag_index) {
                pz_map_remove_tag_placement(editor->map, i);
                continue;
            }
            i++;
        }
    }

    if (pz_map_add_tag_placement(editor->map, tag_index, tile_x, tile_y) < 0) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "Too many tag placements (max=%d)",
            PZ_MAP_MAX_TAG_PLACEMENTS);
        return;
    }

    editor_mark_tags_dirty(editor);
}

static void
editor_remove_tag(
    pz_editor *editor, int tile_x, int tile_y, const char *tag_name)
{
    if (!editor || !editor->map || !tag_name || !tag_name[0]) {
        return;
    }

    if (!pz_map_in_bounds(editor->map, tile_x, tile_y)) {
        return;
    }

    int tag_index = pz_map_find_tag_def(editor->map, tag_name);
    if (tag_index < 0) {
        return;
    }

    int placement
        = pz_map_find_tag_placement(editor->map, tile_x, tile_y, tag_index);
    if (placement >= 0) {
        pz_map_remove_tag_placement(editor->map, placement);
        editor_mark_tags_dirty(editor);
    }
}

static void
editor_prune_tag_placements(pz_editor *editor)
{
    if (!editor || !editor->map) {
        return;
    }

    bool seen[PZ_MAP_MAX_SIZE * PZ_MAP_MAX_SIZE] = { 0 };
    bool removed = false;
    int width = editor->map->width;
    int height = editor->map->height;
    int max_cells = width * height;

    for (int i = 0; i < editor->map->tag_placement_count;) {
        const pz_tag_placement *placement = &editor->map->tag_placements[i];
        int idx = placement->tile_y * width + placement->tile_x;
        if (placement->tile_x < 0 || placement->tile_y < 0
            || placement->tile_x >= width || placement->tile_y >= height
            || idx < 0 || idx >= max_cells || seen[idx]) {
            // Editor limitation: keep only the first tag per cell.
            pz_map_remove_tag_placement(editor->map, i);
            removed = true;
            continue;
        }
        seen[idx] = true;
        i++;
    }

    if (removed) {
        editor_mark_tags_dirty(editor);
    }
}

static void
editor_generate_tag_name(
    pz_editor *editor, pz_tag_type type, char *out, size_t out_size)
{
    if (!editor || !editor->map || !out || out_size == 0) {
        return;
    }

    char prefix = 'T';
    switch (type) {
    case PZ_TAG_SPAWN:
        prefix = 'P';
        break;
    case PZ_TAG_ENEMY:
        prefix = 'E';
        break;
    case PZ_TAG_POWERUP:
        prefix = 'W';
        break;
    case PZ_TAG_BARRIER:
        prefix = 'B';
        break;
    default:
        prefix = 'T';
        break;
    }

    for (int n = 1; n < 100; n++) {
        bool used = false;
        for (int i = 0; i < editor->map->tag_def_count; i++) {
            const char *name = editor->map->tag_defs[i].name;
            if (name[0] != prefix) {
                continue;
            }
            char *end = NULL;
            long val = strtol(name + 1, &end, 10);
            if (end && *end == '\0' && val == n) {
                used = true;
                break;
            }
        }
        if (!used) {
            snprintf(out, out_size, "%c%d", prefix, n);
            return;
        }
    }

    snprintf(out, out_size, "%cX", prefix);
}

static void
editor_init_tag_def(pz_editor *editor, pz_tag_def *def, pz_tag_type type)
{
    if (!editor || !def) {
        return;
    }

    memset(def, 0, sizeof(*def));
    def->type = type;

    editor_generate_tag_name(editor, type, def->name, sizeof(def->name));

    switch (type) {
    case PZ_TAG_SPAWN:
        def->data.spawn.angle = 0.0f;
        def->data.spawn.team = 0;
        def->data.spawn.team_spawn = false;
        break;
    case PZ_TAG_ENEMY:
        def->data.enemy.angle = 0.0f;
        def->data.enemy.type = 3;
        break;
    case PZ_TAG_POWERUP:
        strncpy(def->data.powerup.type_name, "machine_gun",
            sizeof(def->data.powerup.type_name) - 1);
        def->data.powerup.type_name[sizeof(def->data.powerup.type_name) - 1]
            = '\0';
        def->data.powerup.respawn_time = 15.0f;
        def->data.powerup.barrier_tag[0] = '\0';
        def->data.powerup.barrier_count = 2;
        def->data.powerup.barrier_lifetime = 0.0f;
        break;
    case PZ_TAG_BARRIER: {
        const char *tile_name = NULL;
        if (editor->map->tile_def_count > 1) {
            tile_name = editor->map->tile_defs[1].name;
        } else if (editor->map->tile_def_count > 0) {
            tile_name = editor->map->tile_defs[0].name;
        }
        if (tile_name) {
            strncpy(def->data.barrier.tile_name, tile_name,
                sizeof(def->data.barrier.tile_name) - 1);
            def->data.barrier.tile_name[sizeof(def->data.barrier.tile_name) - 1]
                = '\0';
        }
        def->data.barrier.health = 20.0f;
    } break;
    default:
        break;
    }
}

// Get tile height at given tile coordinates (returns 0 for out-of-bounds)
static float
editor_get_tile_height(pz_editor *editor, int tile_x, int tile_y)
{
    if (tile_x < 0 || tile_x >= editor->map->width || tile_y < 0
        || tile_y >= editor->map->height) {
        return 0.0f;
    }
    int cell_idx = tile_y * editor->map->width + tile_x;
    int8_t h = editor->map->cells[cell_idx].height;
    // Match map renderer: GROUND_Y_OFFSET + h * WALL_HEIGHT_UNIT
    return -0.01f + h * 1.5f;
}

static void
editor_update_hover(pz_editor *editor)
{
    if (!editor || !editor->map) {
        editor->hover_valid = false;
        return;
    }

    // Get viewport dimensions
    int vp_width, vp_height;
    pz_renderer_get_viewport(editor->renderer, &vp_width, &vp_height);

    // Update the camera (this configures editor->camera with proper matrices)
    pz_mat4 view, projection;
    pz_editor_get_camera(editor, &view, &projection, vp_width, vp_height);

    // Get ray from camera through mouse position
    pz_vec3 ray_dir = pz_camera_screen_to_ray(
        &editor->camera, (int)editor->mouse_x, (int)editor->mouse_y);
    pz_vec3 ray_origin = editor->camera.position;

    // Ray march through the heightmap to find intersection
    // Step size should be small enough to not miss tiles
    float step_size = editor->map->tile_size * 0.25f;
    float max_dist = 500.0f;

    float half_w = editor->map->world_width / 2.0f;
    float half_h = editor->map->world_height / 2.0f;
    float tile_size = editor->map->tile_size;

    int best_tile_x = -1000;
    int best_tile_y = -1000;
    bool found = false;

    for (float t = 0.0f; t < max_dist; t += step_size) {
        float px = ray_origin.x + t * ray_dir.x;
        float py = ray_origin.y + t * ray_dir.y;
        float pz = ray_origin.z + t * ray_dir.z;

        // Convert to tile coordinates
        int tile_x = (int)floorf((px + half_w) / tile_size);
        int tile_y = (int)floorf((pz + half_h) / tile_size);

        // Get height at this tile
        float tile_height = editor_get_tile_height(editor, tile_x, tile_y);

        // Check if ray is at or below the tile surface
        if (py <= tile_height + 0.1f) {
            best_tile_x = tile_x;
            best_tile_y = tile_y;
            found = true;
            break;
        }
    }

    // If no heightmap hit, fall back to ground plane intersection
    if (!found) {
        pz_vec3 world_point = pz_camera_screen_to_world(
            &editor->camera, (int)editor->mouse_x, (int)editor->mouse_y);
        best_tile_x = (int)floorf((world_point.x + half_w) / tile_size);
        best_tile_y = (int)floorf((world_point.z + half_h) / tile_size);
        found = true;
    }

    // Check if within map bounds (including padding for expansion)
    int padded_min_x = -EDITOR_PADDING_TILES;
    int padded_max_x = editor->map->width + EDITOR_PADDING_TILES - 1;
    int padded_min_y = -EDITOR_PADDING_TILES;
    int padded_max_y = editor->map->height + EDITOR_PADDING_TILES - 1;

    if (found && best_tile_x >= padded_min_x && best_tile_x <= padded_max_x
        && best_tile_y >= padded_min_y && best_tile_y <= padded_max_y) {
        editor->hover_tile_x = best_tile_x;
        editor->hover_tile_y = best_tile_y;
        editor->hover_valid = true;
    } else {
        editor->hover_valid = false;
    }
}

// Expand the map to include the given tile coordinates
// Returns true if expansion happened (and tile_x/tile_y should be recalculated)
// The offset_x/offset_y output parameters indicate how much existing coords
// shifted
static bool
editor_expand_map_to_include(
    pz_editor *editor, int tile_x, int tile_y, int *offset_x, int *offset_y)
{
    if (!editor || !editor->map) {
        return false;
    }

    pz_map *map = editor->map;

    // Calculate expansion needed in each direction
    int expand_left = (tile_x < 0) ? -tile_x : 0;
    int expand_right = (tile_x >= map->width) ? (tile_x - map->width + 1) : 0;
    int expand_top = (tile_y < 0) ? -tile_y : 0;
    int expand_bottom
        = (tile_y >= map->height) ? (tile_y - map->height + 1) : 0;

    if (expand_left == 0 && expand_right == 0 && expand_top == 0
        && expand_bottom == 0) {
        // No expansion needed
        *offset_x = 0;
        *offset_y = 0;
        return false;
    }

    // Check if resulting size is valid
    int new_width = map->width + expand_left + expand_right;
    int new_height = map->height + expand_top + expand_bottom;

    if (new_width > PZ_MAP_MAX_SIZE || new_height > PZ_MAP_MAX_SIZE) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Cannot expand map: would exceed max size (%dx%d > %d)", new_width,
            new_height, PZ_MAP_MAX_SIZE);
        return false;
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
        "Expanding map from %dx%d to %dx%d (L:%d T:%d R:%d B:%d)", map->width,
        map->height, new_width, new_height, expand_left, expand_top,
        expand_right, expand_bottom);

    // Allocate new cells array
    int new_cell_count = new_width * new_height;
    pz_map_cell *new_cells = pz_calloc(new_cell_count, sizeof(pz_map_cell));
    if (!new_cells) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_GAME,
            "Failed to allocate cells for map expansion");
        return false;
    }

    // Initialize new cells with ground at height 0, tile index 0
    for (int i = 0; i < new_cell_count; i++) {
        new_cells[i].height = 0;
        new_cells[i].tile_index = 0;
    }

    // Copy existing cells to their new positions
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int old_idx = y * map->width + x;
            int new_x = x + expand_left;
            int new_y = y + expand_top;
            int new_idx = new_y * new_width + new_x;
            new_cells[new_idx] = map->cells[old_idx];
        }
    }

    // Free old cells and replace
    pz_free(map->cells);
    map->cells = new_cells;
    map->width = new_width;
    map->height = new_height;
    map->world_width = new_width * map->tile_size;
    map->world_height = new_height * map->tile_size;

    // Update tag placements (shift coordinates)
    if (expand_left > 0 || expand_top > 0) {
        for (int i = 0; i < map->tag_placement_count; i++) {
            map->tag_placements[i].tile_x += expand_left;
            map->tag_placements[i].tile_y += expand_top;
        }
    }

    // Update toxic cloud center if it exists
    if (map->has_toxic_cloud) {
        // Convert from old world coords to tile coords, shift, convert back
        pz_vec2 old_center = map->toxic_config.center;
        float old_half_w
            = (map->width - expand_left - expand_right) * map->tile_size / 2.0f;
        float old_half_h = (map->height - expand_top - expand_bottom)
            * map->tile_size / 2.0f;

        // Convert to tile coords in old system
        float tile_cx = (old_center.x + old_half_w) / map->tile_size;
        float tile_cy = (old_center.y + old_half_h) / map->tile_size;

        // Shift by expansion
        tile_cx += (float)expand_left;
        tile_cy += (float)expand_top;

        // Convert back to new world coords
        float new_half_w = map->world_width / 2.0f;
        float new_half_h = map->world_height / 2.0f;
        map->toxic_config.center.x
            = tile_cx * map->tile_size - new_half_w + map->tile_size / 2.0f;
        map->toxic_config.center.y
            = tile_cy * map->tile_size - new_half_h + map->tile_size / 2.0f;
    }

    // Rebuild spawns from tags (this recalculates world positions)
    pz_map_rebuild_spawns_from_tags(map);

    // Return offset for caller to adjust coordinates
    *offset_x = expand_left;
    *offset_y = expand_top;

    return true;
}

static void
editor_apply_edit(pz_editor *editor, int tile_x, int tile_y, bool raise)
{
    if (!editor || !editor->map) {
        return;
    }

    pz_editor_slot *slot = &editor->slots[editor->selected_slot];
    if (slot->type == PZ_EDITOR_SLOT_EMPTY) {
        return;
    }

    // For now, only handle tile placement
    if (slot->type != PZ_EDITOR_SLOT_TILE) {
        return;
    }

    // Check if tile is within map bounds
    bool in_bounds = pz_map_in_bounds(editor->map, tile_x, tile_y);

    if (!in_bounds) {
        // Check if within expansion zone (padding area)
        int padded_min_x = -EDITOR_PADDING_TILES;
        int padded_max_x = editor->map->width + EDITOR_PADDING_TILES - 1;
        int padded_min_y = -EDITOR_PADDING_TILES;
        int padded_max_y = editor->map->height + EDITOR_PADDING_TILES - 1;

        bool in_expansion_zone = tile_x >= padded_min_x
            && tile_x <= padded_max_x && tile_y >= padded_min_y
            && tile_y <= padded_max_y;

        if (!in_expansion_zone) {
            // Outside even the expansion zone
            return;
        }

        // Expand the map to include this tile
        int offset_x = 0, offset_y = 0;
        if (!editor_expand_map_to_include(
                editor, tile_x, tile_y, &offset_x, &offset_y)) {
            // Expansion failed (e.g., would exceed max size)
            return;
        }

        // Adjust tile coordinates after expansion
        tile_x += offset_x;
        tile_y += offset_y;

        // Recalculate camera zoom for new map size
        editor->camera_zoom = editor_calculate_zoom(editor);
    }

    // Get current cell state
    pz_map_cell cell = pz_map_get_cell(editor->map, tile_x, tile_y);
    int selected_tile_index = slot->tile_def_index;

    // Check if there's an entity on this tile - block height changes
    bool has_entity
        = pz_map_find_tag_placement(editor->map, tile_x, tile_y, -1) >= 0;

    if (raise) {
        // Left click: place/raise
        if (cell.tile_index == (uint8_t)selected_tile_index) {
            // Same tile type - increase height (blocked if entity present)
            if (!has_entity && cell.height < 10) {
                pz_map_set_height(editor->map, tile_x, tile_y, cell.height + 1);
                editor_mark_dirty(editor);
            }
        } else {
            // Different tile type - replace tile, keep height
            pz_map_cell new_cell = {
                .height = cell.height,
                .tile_index = (uint8_t)selected_tile_index,
            };
            pz_map_set_cell(editor->map, tile_x, tile_y, new_cell);
            editor_mark_dirty(editor);
        }
    } else {
        // Right click: lower/remove (blocked if entity present)
        if (!has_entity && cell.height > -3) {
            // Decrease height (allows pits down to -3)
            pz_map_set_height(editor->map, tile_x, tile_y, cell.height - 1);
            editor_mark_dirty(editor);
        }
        // At minimum pit depth (-3) or entity present - do nothing
    }
}

static void
editor_mark_dirty(pz_editor *editor)
{
    if (!editor) {
        return;
    }

    if (!editor->dirty) {
        editor->dirty = true;
        editor->dirty_time = pz_time_now();
    }

    // Rebuild map renderer mesh
    if (editor->map_renderer) {
        pz_map_renderer_set_map(editor->map_renderer, editor->map);
    }

    // Rebuild grid overlay
    editor_rebuild_grid(editor);
}

static void
editor_auto_save(pz_editor *editor)
{
    if (!editor || !editor->dirty || editor->map_path[0] == '\0') {
        return;
    }

    pz_editor_save(editor);
}

// ============================================================================
// Tags Dialog
// ============================================================================

// Enemy type names for display
static const char *ENEMY_TYPE_NAMES[]
    = { "sentry", "skirmisher", "hunter", "sniper" };
static const char *POWERUP_TYPE_NAMES[]
    = { "machine_gun", "ricochet", "barrier_placer" };

static bool
editor_mouse_over_dialog(pz_editor *editor)
{
    // Check if mouse is over any open dialog
    // This is now handled by the UI system's mouse consumption
    return editor->tags_dialog_open || editor->tile_picker_open
        || editor->tag_editor_open || editor->map_settings_dialog_open
        || editor->map_name_edit_open || editor->confirm_close_open
        || editor->tag_rename_open;
}

static void
editor_render_tag_editor_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input)
{
    (void)logical_width;
    (void)logical_height;

    float dialog_w = EDITOR_TAGS_DIALOG_W;
    float dialog_h = EDITOR_TAGS_DIALOG_H;

    pz_ui_window_state *state
        = (pz_ui_window_state *)&editor->tag_editor_window;
    pz_ui_window_result win = pz_ui_window(editor->ui, "Tag Editor", dialog_w,
        dialog_h, &editor->tag_editor_open, state, allow_input);

    if (!win.visible) {
        if (!editor->tag_editor_open) {
            editor->tag_editor_index = -1;
        }
        return;
    }

    if (!editor->map || editor->tag_editor_index < 0
        || editor->tag_editor_index >= editor->map->tag_def_count) {
        pz_ui_label(editor->ui, win.content_x, win.content_y,
            "Select a tag from the list to edit",
            (pz_vec4) { 0.8f, 0.5f, 0.5f, 1.0f });
        return;
    }

    float content_x = win.content_x;
    float content_y = win.content_y;
    float content_w = win.content_w;

    float row_label_x = content_x + 8;
    float control_x = content_x + 90;
    float button_w = 22.0f;
    float button_h = 18.0f;
    float value_w = 60.0f;
    float row_h = 20.0f;

    int tag_index = editor->tag_editor_index;
    pz_tag_def *def = &editor->map->tag_defs[tag_index];
    int placement_count = pz_map_count_tag_placements(editor->map, tag_index);

    int lines = 1;
    if (def->type == PZ_TAG_SPAWN) {
        lines += 3;
    } else if (def->type == PZ_TAG_ENEMY) {
        lines += 2;
    } else if (def->type == PZ_TAG_POWERUP) {
        lines += (strcmp(def->data.powerup.type_name, "barrier_placer") == 0)
            ? 5
            : 2;
    } else if (def->type == PZ_TAG_BARRIER) {
        lines += 2;
    }

    float entry_h = 24.0f + lines * row_h + 6.0f;
    pz_ui_rect(editor->ui, content_x, content_y, content_w, entry_h,
        (pz_vec4) { 0.12f, 0.12f, 0.16f, 0.85f });
    pz_ui_rect_outline(editor->ui, content_x, content_y, content_w, entry_h,
        (pz_vec4) { 0.2f, 0.2f, 0.25f, 1.0f }, 1.0f);

    char header[128];
    const char *type_label = (def->type == PZ_TAG_SPAWN) ? "spawn"
        : (def->type == PZ_TAG_ENEMY)                    ? "enemy"
        : (def->type == PZ_TAG_POWERUP)                  ? "powerup"
                                                         : "barrier";
    snprintf(header, sizeof(header), "%s (%s) - %d placements", def->name,
        type_label, placement_count);
    pz_ui_label(editor->ui, row_label_x, content_y + 4, header,
        (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });

    if (pz_ui_button(editor->ui, content_x + content_w - 44, content_y + 2, 38,
            18, "Del")) {
        char removed_name[32];
        strncpy(removed_name, def->name, sizeof(removed_name) - 1);
        removed_name[sizeof(removed_name) - 1] = '\0';
        if (pz_map_remove_tag_def(editor->map, tag_index)) {
            for (int s = 0; s < PZ_EDITOR_MAX_SLOTS; s++) {
                if (editor->slots[s].type == PZ_EDITOR_SLOT_TAG
                    && strcmp(editor->slots[s].tag_name, removed_name) == 0) {
                    pz_editor_clear_slot(editor, s);
                }
            }
            editor_mark_tags_dirty(editor);
        }
        editor_close_tag_editor(editor);
        return;
    }

    float row_y = content_y + 26.0f;
    bool changed = false;

    pz_ui_label(editor->ui, row_label_x, row_y, "Name",
        (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
    if (pz_ui_button(
            editor->ui, control_x, row_y - 2, 140, button_h, def->name)) {
        editor_open_tag_rename(editor, tag_index);
    }
    row_y += row_h;

    if (def->type == PZ_TAG_SPAWN) {
        // Angle
        pz_ui_label(editor->ui, row_label_x, row_y, "Angle",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        if (pz_ui_button(
                editor->ui, control_x, row_y - 2, button_w, button_h, "-")) {
            def->data.spawn.angle -= 0.25f;
            changed = true;
        }
        char value[32];
        snprintf(value, sizeof(value), "%.2f", def->data.spawn.angle);
        pz_ui_label(editor->ui, control_x + button_w + 4, row_y, value,
            (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });
        if (pz_ui_button(editor->ui, control_x + button_w + 4 + value_w + 4,
                row_y - 2, button_w, button_h, "+")) {
            def->data.spawn.angle += 0.25f;
            changed = true;
        }
        if (def->data.spawn.angle > PZ_PI) {
            def->data.spawn.angle -= (PZ_PI * 2.0f);
        } else if (def->data.spawn.angle < -PZ_PI) {
            def->data.spawn.angle += (PZ_PI * 2.0f);
        }
        row_y += row_h;

        // Team
        pz_ui_label(editor->ui, row_label_x, row_y, "Team",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        if (pz_ui_button(
                editor->ui, control_x, row_y - 2, button_w, button_h, "-")) {
            if (def->data.spawn.team > 0) {
                def->data.spawn.team--;
                changed = true;
            }
        }
        snprintf(value, sizeof(value), "%d", def->data.spawn.team);
        pz_ui_label(editor->ui, control_x + button_w + 4, row_y, value,
            (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });
        if (pz_ui_button(editor->ui, control_x + button_w + 4 + value_w + 4,
                row_y - 2, button_w, button_h, "+")) {
            def->data.spawn.team++;
            changed = true;
        }
        row_y += row_h;

        // Team Spawn
        pz_ui_label(editor->ui, row_label_x, row_y, "Team Spawn",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        const char *toggle_label = def->data.spawn.team_spawn ? "On" : "Off";
        if (pz_ui_button(
                editor->ui, control_x, row_y - 2, 60, button_h, toggle_label)) {
            def->data.spawn.team_spawn = !def->data.spawn.team_spawn;
            changed = true;
        }
    } else if (def->type == PZ_TAG_ENEMY) {
        // Angle
        pz_ui_label(editor->ui, row_label_x, row_y, "Angle",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        if (pz_ui_button(
                editor->ui, control_x, row_y - 2, button_w, button_h, "-")) {
            def->data.enemy.angle -= 0.25f;
            changed = true;
        }
        char value[32];
        snprintf(value, sizeof(value), "%.2f", def->data.enemy.angle);
        pz_ui_label(editor->ui, control_x + button_w + 4, row_y, value,
            (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });
        if (pz_ui_button(editor->ui, control_x + button_w + 4 + value_w + 4,
                row_y - 2, button_w, button_h, "+")) {
            def->data.enemy.angle += 0.25f;
            changed = true;
        }
        if (def->data.enemy.angle > PZ_PI) {
            def->data.enemy.angle -= (PZ_PI * 2.0f);
        } else if (def->data.enemy.angle < -PZ_PI) {
            def->data.enemy.angle += (PZ_PI * 2.0f);
        }
        row_y += row_h;

        // Type
        pz_ui_label(editor->ui, row_label_x, row_y, "Type",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        int type_index = def->data.enemy.type - 1;
        if (type_index < 0 || type_index > 3) {
            type_index = 0;
        }
        if (pz_ui_button(editor->ui, control_x, row_y - 2, 140, button_h,
                ENEMY_TYPE_NAMES[type_index])) {
            def->data.enemy.type = (type_index + 1) % 4 + 1;
            changed = true;
        }
    } else if (def->type == PZ_TAG_POWERUP) {
        // Type
        pz_ui_label(editor->ui, row_label_x, row_y, "Type",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        int type_index = 0;
        for (int t = 0; t < 3; t++) {
            if (strcmp(def->data.powerup.type_name, POWERUP_TYPE_NAMES[t])
                == 0) {
                type_index = t;
                break;
            }
        }
        if (pz_ui_button(editor->ui, control_x, row_y - 2, 140, button_h,
                POWERUP_TYPE_NAMES[type_index])) {
            type_index = (type_index + 1) % 3;
            strncpy(def->data.powerup.type_name, POWERUP_TYPE_NAMES[type_index],
                sizeof(def->data.powerup.type_name) - 1);
            def->data.powerup.type_name[sizeof(def->data.powerup.type_name) - 1]
                = '\0';
            changed = true;
        }
        row_y += row_h;

        // Respawn
        pz_ui_label(editor->ui, row_label_x, row_y, "Respawn",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        if (pz_ui_button(
                editor->ui, control_x, row_y - 2, button_w, button_h, "-")) {
            def->data.powerup.respawn_time
                = fmaxf(0.0f, def->data.powerup.respawn_time - 5.0f);
            changed = true;
        }
        char value[32];
        snprintf(value, sizeof(value), "%.1f", def->data.powerup.respawn_time);
        pz_ui_label(editor->ui, control_x + button_w + 4, row_y, value,
            (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });
        if (pz_ui_button(editor->ui, control_x + button_w + 4 + value_w + 4,
                row_y - 2, button_w, button_h, "+")) {
            def->data.powerup.respawn_time += 5.0f;
            changed = true;
        }
        row_y += row_h;

        if (strcmp(def->data.powerup.type_name, "barrier_placer") == 0) {
            // Barrier tag reference
            pz_ui_label(editor->ui, row_label_x, row_y, "Barrier",
                (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
            const char *current = def->data.powerup.barrier_tag[0]
                ? def->data.powerup.barrier_tag
                : "(none)";
            if (pz_ui_button(
                    editor->ui, control_x, row_y - 2, 140, button_h, current)) {
                int barrier_count = 0;
                int current_idx = -1;
                for (int t = 0; t < editor->map->tag_def_count; t++) {
                    if (editor->map->tag_defs[t].type != PZ_TAG_BARRIER) {
                        continue;
                    }
                    if (strcmp(editor->map->tag_defs[t].name,
                            def->data.powerup.barrier_tag)
                        == 0) {
                        current_idx = barrier_count;
                    }
                    barrier_count++;
                }
                if (barrier_count == 0) {
                    def->data.powerup.barrier_tag[0] = '\0';
                } else {
                    int next_idx = (current_idx + 1) % barrier_count;
                    int idx = 0;
                    for (int t = 0; t < editor->map->tag_def_count; t++) {
                        if (editor->map->tag_defs[t].type != PZ_TAG_BARRIER) {
                            continue;
                        }
                        if (idx == next_idx) {
                            strncpy(def->data.powerup.barrier_tag,
                                editor->map->tag_defs[t].name,
                                sizeof(def->data.powerup.barrier_tag) - 1);
                            def->data.powerup.barrier_tag
                                [sizeof(def->data.powerup.barrier_tag) - 1]
                                = '\0';
                            break;
                        }
                        idx++;
                    }
                }
                changed = true;
            }
            row_y += row_h;

            // Barrier count
            pz_ui_label(editor->ui, row_label_x, row_y, "Count",
                (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
            if (pz_ui_button(editor->ui, control_x, row_y - 2, button_w,
                    button_h, "-")) {
                if (def->data.powerup.barrier_count > 1) {
                    def->data.powerup.barrier_count--;
                    changed = true;
                }
            }
            snprintf(
                value, sizeof(value), "%d", def->data.powerup.barrier_count);
            pz_ui_label(editor->ui, control_x + button_w + 4, row_y, value,
                (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });
            if (pz_ui_button(editor->ui, control_x + button_w + 4 + value_w + 4,
                    row_y - 2, button_w, button_h, "+")) {
                if (def->data.powerup.barrier_count < 8) {
                    def->data.powerup.barrier_count++;
                    changed = true;
                }
            }
            row_y += row_h;

            // Barrier lifetime
            pz_ui_label(editor->ui, row_label_x, row_y, "Lifetime",
                (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
            if (pz_ui_button(editor->ui, control_x, row_y - 2, button_w,
                    button_h, "-")) {
                def->data.powerup.barrier_lifetime
                    = fmaxf(0.0f, def->data.powerup.barrier_lifetime - 5.0f);
                changed = true;
            }
            snprintf(value, sizeof(value), "%.1f",
                def->data.powerup.barrier_lifetime);
            pz_ui_label(editor->ui, control_x + button_w + 4, row_y, value,
                (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });
            if (pz_ui_button(editor->ui, control_x + button_w + 4 + value_w + 4,
                    row_y - 2, button_w, button_h, "+")) {
                def->data.powerup.barrier_lifetime += 5.0f;
                changed = true;
            }
        }
    } else if (def->type == PZ_TAG_BARRIER) {
        // Tile
        pz_ui_label(editor->ui, row_label_x, row_y, "Tile",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        const char *tile_label = def->data.barrier.tile_name[0]
            ? def->data.barrier.tile_name
            : "(unset)";
        if (pz_ui_button(
                editor->ui, control_x, row_y - 2, 140, button_h, tile_label)
            && editor->map->tile_def_count > 0) {
            int tile_index = 0;
            for (int t = 0; t < editor->map->tile_def_count; t++) {
                if (strcmp(editor->map->tile_defs[t].name,
                        def->data.barrier.tile_name)
                    == 0) {
                    tile_index = (t + 1) % editor->map->tile_def_count;
                    break;
                }
            }
            strncpy(def->data.barrier.tile_name,
                editor->map->tile_defs[tile_index].name,
                sizeof(def->data.barrier.tile_name) - 1);
            def->data.barrier.tile_name[sizeof(def->data.barrier.tile_name) - 1]
                = '\0';
            changed = true;
        }
        row_y += row_h;

        // Health
        pz_ui_label(editor->ui, row_label_x, row_y, "Health",
            (pz_vec4) { 0.8f, 0.8f, 0.85f, 1.0f });
        if (pz_ui_button(
                editor->ui, control_x, row_y - 2, button_w, button_h, "-")) {
            if (def->data.barrier.health > 5.0f) {
                def->data.barrier.health -= 5.0f;
                changed = true;
            }
        }
        char value[32];
        snprintf(value, sizeof(value), "%.1f", def->data.barrier.health);
        pz_ui_label(editor->ui, control_x + button_w + 4, row_y, value,
            (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });
        if (pz_ui_button(editor->ui, control_x + button_w + 4 + value_w + 4,
                row_y - 2, button_w, button_h, "+")) {
            def->data.barrier.health += 5.0f;
            changed = true;
        }
    }

    if (changed) {
        editor_mark_tags_dirty(editor);
    }
}

// ============================================================================
// Tile Picker
// ============================================================================

static void
editor_render_tile_picker(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input)
{
    (void)logical_width;
    (void)logical_height;

    // Reset hovered tile index
    editor->tile_picker_hovered_index = -1;

    float dialog_w = EDITOR_TILE_PICKER_W;
    float dialog_h = EDITOR_TILE_PICKER_H;

    // Use shared window function
    pz_ui_window_state *state
        = (pz_ui_window_state *)&editor->tile_picker_window;
    pz_ui_window_result win = pz_ui_window(editor->ui, "Tile Picker", dialog_w,
        dialog_h, &editor->tile_picker_open, state, allow_input);

    if (!win.visible) {
        return;
    }

    // Get all tiles from the registry
    int tile_count = pz_tile_registry_count(editor->tile_registry);
    if (tile_count == 0) {
        pz_ui_label(editor->ui, win.content_x, win.content_y, "No tiles loaded",
            (pz_vec4) { 0.8f, 0.5f, 0.5f, 1.0f });
        return;
    }

    // Convert mouse coords to logical pixels for hit testing
    float dpi_scale = pz_renderer_get_dpi_scale(editor->renderer);
    float mouse_x = editor->mouse_x / dpi_scale;
    float mouse_y = editor->mouse_y / dpi_scale;

    // Grid layout for tiles
    float tile_size = 56.0f;
    float spacing = 8.0f;
    float label_height = 16.0f;
    float item_height = tile_size + label_height + spacing;
    int cols = (int)((win.content_w + spacing) / (tile_size + spacing));
    if (cols < 1)
        cols = 1;

    float start_x = win.content_x;
    float start_y = win.content_y;

    for (int i = 0; i < tile_count; i++) {
        const pz_tile_config *tile
            = pz_tile_registry_get_by_index(editor->tile_registry, i);
        if (!tile || !tile->valid)
            continue;

        int col = i % cols;
        int row = i / cols;
        float item_x = start_x + col * (tile_size + spacing);
        float item_y = start_y + row * item_height;

        // Skip if outside content area
        if (item_y + item_height > win.content_y + win.content_h) {
            continue;
        }

        // Check hover
        bool hovered = allow_input && mouse_x >= item_x
            && mouse_x < item_x + tile_size && mouse_y >= item_y
            && mouse_y < item_y + tile_size;

        if (hovered) {
            editor->tile_picker_hovered_index = i;
        }

        // Draw tile preview background
        pz_vec4 bg_color = hovered ? (pz_vec4) { 0.4f, 0.5f, 0.6f, 1.0f }
                                   : (pz_vec4) { 0.25f, 0.25f, 0.3f, 1.0f };
        pz_ui_rect(editor->ui, item_x, item_y, tile_size, tile_size, bg_color);

        // Draw textured preview if textures available
        if (tile->wall_texture != PZ_INVALID_HANDLE
            && tile->ground_texture != PZ_INVALID_HANDLE) {
            // Use the slot_textured function for the preview
            pz_ui_slot_textured(editor->ui, item_x, item_y, tile_size, false,
                NULL, tile->wall_texture, tile->ground_texture);
        }

        // Draw border
        pz_vec4 border_color = hovered ? (pz_vec4) { 1.0f, 1.0f, 1.0f, 1.0f }
                                       : (pz_vec4) { 0.4f, 0.4f, 0.45f, 1.0f };
        pz_ui_rect_outline(editor->ui, item_x, item_y, tile_size, tile_size,
            border_color, hovered ? 2.0f : 1.0f);

        // Draw tile name below (truncate to tile width)
        pz_ui_label_fit(editor->ui, item_x, item_y + tile_size + 2, tile_size,
            tile->name,
            hovered ? (pz_vec4) { 1.0f, 1.0f, 1.0f, 1.0f }
                    : (pz_vec4) { 0.7f, 0.7f, 0.7f, 1.0f });

        // Handle click
        if (hovered && editor->mouse_left_just_pressed) {
            int tile_def_idx = editor_find_or_add_tile_def(editor, tile, NULL);
            if (tile_def_idx >= 0) {
                pz_editor_set_slot_tile(
                    editor, editor->selected_slot, tile_def_idx);
            }
            editor_close_dialog(
                editor, &editor->tile_picker_open, &editor->tile_picker_window);
            pz_ui_consume_mouse(editor->ui);
        }
    }

    // Hint text at bottom
    float hint_y = win.content_y + win.content_h - 20;
    pz_ui_label(editor->ui, win.content_x, hint_y,
        "Click to select | Hover + 1-6: assign to slot",
        (pz_vec4) { 0.6f, 0.6f, 0.6f, 1.0f });
}

// ============================================================================
// Tag Picker
// ============================================================================

static void
editor_render_tags_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input)
{
    (void)logical_width;
    (void)logical_height;

    editor->tag_list_hovered_index = -1;

    float dialog_w = EDITOR_TAGS_DIALOG_W;
    float dialog_h = EDITOR_TAGS_DIALOG_H;

    pz_ui_window_state *state = (pz_ui_window_state *)&editor->tags_window;
    pz_ui_window_result win = pz_ui_window(editor->ui, "Tags", dialog_w,
        dialog_h, &editor->tags_dialog_open, state, allow_input);

    if (!win.visible) {
        return;
    }

    float dpi_scale = pz_renderer_get_dpi_scale(editor->renderer);
    float mouse_x = editor->mouse_x / dpi_scale;
    float mouse_y = editor->mouse_y / dpi_scale;

    float content_x = win.content_x;
    float content_y = win.content_y;
    float content_w = win.content_w;

    bool can_add
        = editor->map && editor->map->tag_def_count < PZ_MAP_MAX_TAG_DEFS;

    if (editor->map) {
        if (pz_ui_button(editor->ui, content_x, content_y, 60, 22, "+Spawn")
            && can_add) {
            pz_tag_def def;
            editor_init_tag_def(editor, &def, PZ_TAG_SPAWN);
            if (pz_map_add_tag_def(editor->map, &def) >= 0) {
                editor_mark_tags_dirty(editor);
            }
        }

        if (pz_ui_button(
                editor->ui, content_x + 65, content_y, 60, 22, "+Enemy")
            && can_add) {
            pz_tag_def def;
            editor_init_tag_def(editor, &def, PZ_TAG_ENEMY);
            if (pz_map_add_tag_def(editor->map, &def) >= 0) {
                editor_mark_tags_dirty(editor);
            }
        }

        if (pz_ui_button(
                editor->ui, content_x + 130, content_y, 70, 22, "+Powerup")
            && can_add) {
            pz_tag_def def;
            editor_init_tag_def(editor, &def, PZ_TAG_POWERUP);
            if (pz_map_add_tag_def(editor->map, &def) >= 0) {
                editor_mark_tags_dirty(editor);
            }
        }

        if (pz_ui_button(
                editor->ui, content_x + 205, content_y, 70, 22, "+Barrier")
            && can_add) {
            pz_tag_def def;
            editor_init_tag_def(editor, &def, PZ_TAG_BARRIER);
            if (pz_map_add_tag_def(editor->map, &def) >= 0) {
                editor_mark_tags_dirty(editor);
            }
        }
    } else {
        pz_ui_label(editor->ui, content_x, content_y, "No map loaded",
            (pz_vec4) { 0.8f, 0.5f, 0.5f, 1.0f });
    }

    content_y += 32.0f;

    float item_h = 24.0f;
    float spacing = 4.0f;
    float edit_button_w = 48.0f;
    float x = content_x;
    float y = content_y;
    float w = content_w;

    // Empty option
    bool empty_hovered = allow_input && mouse_x >= x && mouse_x < x + w
        && mouse_y >= y && mouse_y < y + item_h;
    pz_vec4 empty_bg = empty_hovered ? (pz_vec4) { 0.35f, 0.35f, 0.4f, 1.0f }
                                     : (pz_vec4) { 0.25f, 0.25f, 0.3f, 1.0f };
    pz_ui_rect(editor->ui, x, y, w, item_h, empty_bg);
    pz_ui_label_centered(editor->ui, x, y, w, item_h, "(empty)",
        (pz_vec4) { 0.85f, 0.85f, 0.9f, 1.0f });
    if (empty_hovered && editor->mouse_left_just_pressed) {
        pz_editor_clear_slot(editor, editor->selected_slot);
        pz_ui_consume_mouse(editor->ui);
        return;
    }

    y += item_h + spacing;

    if (!editor->map || editor->map->tag_def_count == 0) {
        pz_ui_label(editor->ui, x, y, "No tags defined",
            (pz_vec4) { 0.8f, 0.5f, 0.5f, 1.0f });
        return;
    }

    for (int i = 0; i < editor->map->tag_def_count; i++) {
        const pz_tag_def *def = &editor->map->tag_defs[i];
        if (y + item_h > win.content_y + win.content_h) {
            break;
        }

        bool hovered = allow_input && mouse_x >= x && mouse_x < x + w
            && mouse_y >= y && mouse_y < y + item_h;
        if (hovered) {
            editor->tag_list_hovered_index = i;
        }

        pz_vec4 bg = hovered ? (pz_vec4) { 0.35f, 0.35f, 0.4f, 1.0f }
                             : (pz_vec4) { 0.22f, 0.22f, 0.26f, 1.0f };
        pz_ui_rect(editor->ui, x, y, w, item_h, bg);

        pz_vec4 color = editor_tag_color(def->type);
        pz_ui_rect(editor->ui, x + 4, y + 4, 14, item_h - 8, color);

        char label[128];
        if (def->type == PZ_TAG_ENEMY) {
            int enemy_index = def->data.enemy.type - 1;
            if (enemy_index < 0 || enemy_index > 3) {
                enemy_index = 0;
            }
            snprintf(label, sizeof(label), "%s (enemy: %s)", def->name,
                ENEMY_TYPE_NAMES[enemy_index]);
        } else if (def->type == PZ_TAG_POWERUP) {
            const char *powerup_type = def->data.powerup.type_name[0]
                ? def->data.powerup.type_name
                : "machine_gun";
            snprintf(label, sizeof(label), "%s (powerup: %s)", def->name,
                powerup_type);
        } else if (def->type == PZ_TAG_BARRIER) {
            snprintf(label, sizeof(label), "%s (barrier: %s)", def->name,
                def->data.barrier.tile_name[0] ? def->data.barrier.tile_name
                                               : "default");
        } else {
            snprintf(label, sizeof(label), "%s (spawn)", def->name);
        }

        pz_ui_label(editor->ui, x + 24, y + 3, label,
            (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f });

        float edit_x = x + w - edit_button_w - 6.0f;
        if (pz_ui_button(editor->ui, edit_x, y + 3, edit_button_w,
                item_h - 6.0f, "Edit")) {
            editor_open_tag_editor(editor, i);
            pz_ui_consume_mouse(editor->ui);
            return;
        }

        if (hovered && editor->mouse_left_just_pressed) {
            pz_editor_set_slot_tag(editor, editor->selected_slot, def->name);
            pz_ui_consume_mouse(editor->ui);
            return;
        }

        y += item_h + spacing;
    }

    float hint_y = win.content_y + win.content_h - 18.0f;
    pz_ui_label(editor->ui, win.content_x, hint_y,
        "Click: assign slot | Hover + 1-6: bind",
        (pz_vec4) { 0.6f, 0.6f, 0.6f, 1.0f });
}

// ============================================================================
// Tag Rename Dialog
// ============================================================================

static void
editor_render_tag_rename_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input)
{
    (void)logical_width;
    (void)logical_height;

    float dialog_w = EDITOR_TAG_RENAME_W;
    float dialog_h = EDITOR_TAG_RENAME_H;

    pz_ui_window_state *state
        = (pz_ui_window_state *)&editor->tag_rename_window;
    pz_ui_window_result win = pz_ui_window(editor->ui, "Rename Tag", dialog_w,
        dialog_h, &editor->tag_rename_open, state, allow_input);

    if (!win.visible) {
        if (!editor->tag_rename_open) {
            editor_cancel_tag_rename(editor);
        }
        return;
    }

    float x = win.content_x;
    float y = win.content_y;

    pz_ui_label(
        editor->ui, x, y, "Name", (pz_vec4) { 0.85f, 0.85f, 0.9f, 1.0f });

    float field_x = x;
    float field_y = y + 20.0f;
    float field_w = win.content_w;
    float field_h = 24.0f;

    pz_vec4 field_bg = allow_input ? (pz_vec4) { 0.2f, 0.2f, 0.25f, 1.0f }
                                   : (pz_vec4) { 0.16f, 0.16f, 0.2f, 1.0f };
    pz_ui_rect(editor->ui, field_x, field_y, field_w, field_h, field_bg);
    pz_ui_rect_outline(editor->ui, field_x, field_y, field_w, field_h,
        (pz_vec4) { 0.4f, 0.4f, 0.45f, 1.0f }, 1.0f);

    float text_x = field_x + 6.0f;
    float text_y = field_y + 4.0f;
    pz_ui_label(editor->ui, text_x, text_y, editor->tag_rename_buffer,
        (pz_vec4) { 0.95f, 0.95f, 1.0f, 1.0f });

    pz_font *font = editor_get_ui_font(editor);
    if (font) {
        size_t len = strlen(editor->tag_rename_buffer);
        int cursor = editor->tag_rename_cursor;
        if (cursor < 0) {
            cursor = 0;
        }
        if ((size_t)cursor > len) {
            cursor = (int)len;
        }

        char temp[32];
        if ((size_t)cursor >= sizeof(temp)) {
            cursor = (int)sizeof(temp) - 1;
        }
        memcpy(temp, editor->tag_rename_buffer, (size_t)cursor);
        temp[cursor] = '\0';

        pz_text_style style = PZ_TEXT_STYLE_DEFAULT(font, 16.0f);
        style.align_v = PZ_FONT_ALIGN_TOP;
        pz_text_bounds bounds = pz_font_measure(&style, temp);

        float caret_x = text_x + bounds.width;
        pz_ui_rect(editor->ui, caret_x, field_y + 4.0f, 2.0f, field_h - 8.0f,
            allow_input ? (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f }
                        : (pz_vec4) { 0.5f, 0.5f, 0.55f, 1.0f });
    }

    float dpi_scale = pz_renderer_get_dpi_scale(editor->renderer);
    float mouse_x = editor->mouse_x / dpi_scale;
    float mouse_y = editor->mouse_y / dpi_scale;
    if (allow_input && editor->mouse_left_just_pressed
        && editor_point_in_rect(
            mouse_x, mouse_y, field_x, field_y, field_w, field_h)) {
        editor->tag_rename_cursor = (int)strlen(editor->tag_rename_buffer);
        pz_ui_consume_mouse(editor->ui);
    }

    if (editor->tag_rename_error[0]) {
        pz_ui_label(editor->ui, x, field_y + field_h + 6.0f,
            editor->tag_rename_error, (pz_vec4) { 0.95f, 0.55f, 0.55f, 1.0f });
    }

    float btn_w = 80.0f;
    float btn_h = 24.0f;
    float btn_y = win.content_y + win.content_h - btn_h;
    float btn_x = win.content_x + win.content_w - btn_w * 2 - 10.0f;

    if (pz_ui_button(editor->ui, btn_x, btn_y, btn_w, btn_h, "Cancel")) {
        editor_cancel_tag_rename(editor);
    }
    if (pz_ui_button(
            editor->ui, btn_x + btn_w + 10.0f, btn_y, btn_w, btn_h, "OK")) {
        editor_commit_tag_rename(editor);
    }
}

static void
editor_render_map_name_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input)
{
    (void)logical_width;
    (void)logical_height;

    pz_ui_window_state *state = (pz_ui_window_state *)&editor->map_name_window;
    pz_ui_window_result win = pz_ui_window(editor->ui, "Rename Map",
        EDITOR_NAME_DIALOG_W, EDITOR_NAME_DIALOG_H, &editor->map_name_edit_open,
        state, allow_input);

    if (!win.visible) {
        if (!editor->map_name_edit_open) {
            editor_cancel_map_name_dialog(editor);
        }
        return;
    }

    float x = win.content_x;
    float y = win.content_y;

    pz_ui_label(
        editor->ui, x, y, "Name", (pz_vec4) { 0.85f, 0.85f, 0.9f, 1.0f });

    float field_x = x;
    float field_y = y + 20.0f;
    float field_w = win.content_w;
    float field_h = 24.0f;

    pz_vec4 field_bg = allow_input ? (pz_vec4) { 0.2f, 0.2f, 0.25f, 1.0f }
                                   : (pz_vec4) { 0.16f, 0.16f, 0.2f, 1.0f };
    pz_ui_rect(editor->ui, field_x, field_y, field_w, field_h, field_bg);
    pz_ui_rect_outline(editor->ui, field_x, field_y, field_w, field_h,
        (pz_vec4) { 0.4f, 0.4f, 0.45f, 1.0f }, 1.0f);

    float text_x = field_x + 6.0f;
    float text_y = field_y + 4.0f;
    pz_ui_label(editor->ui, text_x, text_y, editor->map_name_buffer,
        (pz_vec4) { 0.95f, 0.95f, 1.0f, 1.0f });

    pz_font *font = editor_get_ui_font(editor);
    if (font) {
        size_t len = strlen(editor->map_name_buffer);
        int cursor = editor->map_name_cursor;
        if (cursor < 0) {
            cursor = 0;
        }
        if ((size_t)cursor > len) {
            cursor = (int)len;
        }

        char temp[64];
        if ((size_t)cursor >= sizeof(temp)) {
            cursor = (int)sizeof(temp) - 1;
        }
        memcpy(temp, editor->map_name_buffer, (size_t)cursor);
        temp[cursor] = '\0';

        pz_text_style style = PZ_TEXT_STYLE_DEFAULT(font, 16.0f);
        style.align_v = PZ_FONT_ALIGN_TOP;
        pz_text_bounds bounds = pz_font_measure(&style, temp);

        float caret_x = text_x + bounds.width;
        pz_ui_rect(editor->ui, caret_x, field_y + 4.0f, 2.0f, field_h - 8.0f,
            allow_input ? (pz_vec4) { 0.9f, 0.9f, 0.95f, 1.0f }
                        : (pz_vec4) { 0.5f, 0.5f, 0.55f, 1.0f });
    }

    float dpi_scale = pz_renderer_get_dpi_scale(editor->renderer);
    float mouse_x = editor->mouse_x / dpi_scale;
    float mouse_y = editor->mouse_y / dpi_scale;
    if (allow_input && editor->mouse_left_just_pressed
        && editor_point_in_rect(
            mouse_x, mouse_y, field_x, field_y, field_w, field_h)) {
        editor->map_name_cursor = (int)strlen(editor->map_name_buffer);
        pz_ui_consume_mouse(editor->ui);
    }

    if (editor->map_name_error[0]) {
        pz_ui_label(editor->ui, x, field_y + field_h + 6.0f,
            editor->map_name_error, (pz_vec4) { 0.95f, 0.55f, 0.55f, 1.0f });
    }

    float btn_w = 80.0f;
    float btn_h = 24.0f;
    float btn_y = win.content_y + win.content_h - btn_h;
    float btn_x = win.content_x + win.content_w - btn_w * 2 - 10.0f;

    if (pz_ui_button(editor->ui, btn_x, btn_y, btn_w, btn_h, "Cancel")) {
        editor_cancel_map_name_dialog(editor);
    }
    if (pz_ui_button(
            editor->ui, btn_x + btn_w + 10.0f, btn_y, btn_w, btn_h, "OK")) {
        editor_commit_map_name_dialog(editor);
    }
}

// ============================================================================
// Close Confirmation Dialog
// ============================================================================

static void
editor_render_confirm_close(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input)
{
    (void)logical_width;
    (void)logical_height;

    float dialog_w = EDITOR_CONFIRM_CLOSE_W;
    float dialog_h = EDITOR_CONFIRM_CLOSE_H;

    // Use shared window function
    pz_ui_window_state *state
        = (pz_ui_window_state *)&editor->confirm_close_window;
    pz_ui_window_result win = pz_ui_window(editor->ui, "Close Editor?",
        dialog_w, dialog_h, &editor->confirm_close_open, state, allow_input);

    if (!win.visible) {
        return;
    }

    // Message
    pz_ui_label(editor->ui, win.content_x, win.content_y,
        editor->dirty ? "You have unsaved changes." : "Close the editor?",
        (pz_vec4) { 1.0f, 1.0f, 1.0f, 0.9f });

    // Buttons at bottom
    float btn_w = 100;
    float btn_h = 28;
    float btn_spacing = 20;
    float btn_y = win.content_y + win.content_h - btn_h;
    float total_btn_w = btn_w * 2 + btn_spacing;
    float btn_start_x = win.content_x + (win.content_w - total_btn_w) / 2;

    // Save & Close button (if dirty)
    if (editor->dirty) {
        if (pz_ui_button(
                editor->ui, btn_start_x, btn_y, btn_w, btn_h, "Save & Close")) {
            pz_editor_save(editor);
            editor_close_dialog(editor, &editor->confirm_close_open,
                &editor->confirm_close_window);
            editor->wants_close = true;
        }
        if (pz_ui_button(editor->ui, btn_start_x + btn_w + btn_spacing, btn_y,
                btn_w, btn_h, "Discard")) {
            editor_close_dialog(editor, &editor->confirm_close_open,
                &editor->confirm_close_window);
            editor->wants_close = true;
        }
    } else {
        // Not dirty - just Yes/No
        if (pz_ui_button(editor->ui, btn_start_x, btn_y, btn_w, btn_h, "Yes")) {
            editor_close_dialog(editor, &editor->confirm_close_open,
                &editor->confirm_close_window);
            editor->wants_close = true;
        }
        if (pz_ui_button(editor->ui, btn_start_x + btn_w + btn_spacing, btn_y,
                btn_w, btn_h, "No")) {
            editor_close_dialog(editor, &editor->confirm_close_open,
                &editor->confirm_close_window);
        }
    }
}

// ============================================================================
// Map Settings Dialog & Helpers
// ============================================================================

static void
editor_refresh_background(pz_editor *editor)
{
    if (!editor || !editor->background || !editor->map) {
        return;
    }

    pz_background_set_from_map(editor->background, editor->map);
}

static void
editor_mark_map_settings_changed(pz_editor *editor)
{
    if (!editor) {
        return;
    }

    editor_mark_dirty(editor);
    editor_refresh_background(editor);
}

static bool
editor_map_name_char_valid(uint32_t codepoint)
{
    return codepoint >= 32 && codepoint <= 126;
}

static void
editor_open_map_name_dialog(pz_editor *editor)
{
    if (!editor || !editor->map) {
        return;
    }

    editor->map_name_edit_open = true;
    editor->map_name_window.x = 0.0f;
    editor->map_name_window.y = 0.0f;
    editor->map_name_window.z_order = ++editor->window_z_counter;
    strncpy(editor->map_name_buffer, editor->map->name,
        sizeof(editor->map_name_buffer) - 1);
    editor->map_name_buffer[sizeof(editor->map_name_buffer) - 1] = '\0';
    editor->map_name_cursor = (int)strlen(editor->map_name_buffer);
    editor->map_name_error[0] = '\0';
}

static void
editor_cancel_map_name_dialog(pz_editor *editor)
{
    if (!editor) {
        return;
    }

    editor->map_name_edit_open = false;
    editor->map_name_window.dragging = false;
    editor->map_name_window.z_order = 0;
    editor->map_name_error[0] = '\0';
}

static void
editor_commit_map_name_dialog(pz_editor *editor)
{
    if (!editor || !editor->map || !editor->map_name_edit_open) {
        return;
    }

    char temp[sizeof(editor->map_name_buffer)];
    strncpy(temp, editor->map_name_buffer, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *start = temp;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';

    if (!*start) {
        snprintf(editor->map_name_error, sizeof(editor->map_name_error),
            "Name required");
        return;
    }

    strncpy(editor->map->name, start, sizeof(editor->map->name) - 1);
    editor->map->name[sizeof(editor->map->name) - 1] = '\0';
    strncpy(editor->map_name_buffer, editor->map->name,
        sizeof(editor->map_name_buffer) - 1);
    editor->map_name_buffer[sizeof(editor->map_name_buffer) - 1] = '\0';
    editor->map_name_cursor = (int)strlen(editor->map_name_buffer);

    editor_mark_map_settings_changed(editor);
    editor_cancel_map_name_dialog(editor);
}

static void
editor_handle_map_name_char_input(pz_editor *editor, uint32_t codepoint)
{
    if (!editor || !editor->map_name_edit_open
        || !editor_map_name_char_valid(codepoint)) {
        return;
    }

    size_t len = strlen(editor->map_name_buffer);
    if (len >= sizeof(editor->map_name_buffer) - 1) {
        return;
    }

    int cursor = editor->map_name_cursor;
    if (cursor < 0) {
        cursor = 0;
    }
    if ((size_t)cursor > len) {
        cursor = (int)len;
    }

    memmove(editor->map_name_buffer + cursor + 1,
        editor->map_name_buffer + cursor, len - (size_t)cursor + 1);
    editor->map_name_buffer[cursor] = (char)codepoint;
    editor->map_name_cursor = cursor + 1;
    editor->map_name_error[0] = '\0';
}

static bool
editor_row_visible(
    float draw_y, float height, float view_top, float view_bottom)
{
    return (draw_y + height) > view_top && draw_y < view_bottom;
}

static void
editor_draw_section_header(pz_editor *editor, float x, float width, float *y,
    const char *label, pz_vec4 color, float scroll, float view_top,
    float view_bottom)
{
    if (!editor || !y || !label) {
        return;
    }

    float header_h = 28.0f;
    float draw_y = *y - scroll;
    if (editor_row_visible(draw_y, header_h, view_top, view_bottom)) {
        pz_ui_label(editor->ui, x, draw_y, label, color);
        pz_vec4 line = { color.x, color.y, color.z, 0.35f };
        pz_ui_rect(editor->ui, x, draw_y + header_h - 6.0f, width, 1.0f, line);
    }
    *y += header_h;
}

static bool
editor_draw_float_row(pz_editor *editor, float x, float width, float *y,
    const char *label, float *value, float step, float min, float max,
    const char *fmt, float scroll, float view_top, float view_bottom)
{
    (void)width;
    if (!editor || !y || !label || !value || !fmt) {
        return false;
    }

    const pz_vec4 text_color = { 0.85f, 0.85f, 0.9f, 1.0f };
    float control_x = x + EDITOR_SETTINGS_LABEL_W;
    float row_h = EDITOR_SETTINGS_ROW_H;
    bool changed = false;
    float draw_y = *y - scroll;

    if (editor_row_visible(draw_y, row_h, view_top, view_bottom)) {
        pz_ui_label(editor->ui, x, draw_y + 2.0f, label, text_color);

        if (pz_ui_button(editor->ui, control_x, draw_y,
                EDITOR_SETTINGS_BUTTON_W, row_h - 4.0f, "-")) {
            *value = pz_clampf(*value - step, min, max);
            changed = true;
        }

        char buffer[32];
        snprintf(buffer, sizeof(buffer), fmt, *value);
        pz_ui_label_centered(editor->ui,
            control_x + EDITOR_SETTINGS_BUTTON_W + 4.0f, draw_y,
            EDITOR_SETTINGS_VALUE_W, row_h - 4.0f, buffer,
            (pz_vec4) { 1.0f, 1.0f, 1.0f, 0.9f });

        float plus_x = control_x + EDITOR_SETTINGS_BUTTON_W + 4.0f
            + EDITOR_SETTINGS_VALUE_W + 4.0f;
        if (pz_ui_button(editor->ui, plus_x, draw_y, EDITOR_SETTINGS_BUTTON_W,
                row_h - 4.0f, "+")) {
            *value = pz_clampf(*value + step, min, max);
            changed = true;
        }
    }

    *y += row_h;
    return changed;
}

static bool
editor_draw_int_row(pz_editor *editor, float x, float width, float *y,
    const char *label, int *value, int step, int min, int max, float scroll,
    float view_top, float view_bottom)
{
    (void)width;
    if (!editor || !label || !y || !value) {
        return false;
    }

    float row_h = EDITOR_SETTINGS_ROW_H;
    float control_x = x + EDITOR_SETTINGS_LABEL_W;
    const pz_vec4 text_color = { 0.85f, 0.85f, 0.9f, 1.0f };
    bool changed = false;
    float draw_y = *y - scroll;

    if (editor_row_visible(draw_y, row_h, view_top, view_bottom)) {
        pz_ui_label(editor->ui, x, draw_y + 2.0f, label, text_color);

        if (pz_ui_button(editor->ui, control_x, draw_y,
                EDITOR_SETTINGS_BUTTON_W, row_h - 4.0f, "-")) {
            int next = *value - step;
            if (next < min)
                next = min;
            *value = next;
            changed = true;
        }

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d", *value);
        pz_ui_label_centered(editor->ui,
            control_x + EDITOR_SETTINGS_BUTTON_W + 4.0f, draw_y,
            EDITOR_SETTINGS_VALUE_W, row_h - 4.0f, buffer,
            (pz_vec4) { 1.0f, 1.0f, 1.0f, 0.9f });

        float plus_x = control_x + EDITOR_SETTINGS_BUTTON_W + 4.0f
            + EDITOR_SETTINGS_VALUE_W + 4.0f;
        if (pz_ui_button(editor->ui, plus_x, draw_y, EDITOR_SETTINGS_BUTTON_W,
                row_h - 4.0f, "+")) {
            int next = *value + step;
            if (next > max)
                next = max;
            *value = next;
            changed = true;
        }
    }

    *y += row_h;
    return changed;
}

static bool
editor_draw_toggle_row(pz_editor *editor, float x, float width, float *y,
    const char *label, bool *value, float scroll, float view_top,
    float view_bottom)
{
    (void)width;
    if (!editor || !label || !y || !value) {
        return false;
    }

    const pz_vec4 text_color = { 0.85f, 0.85f, 0.9f, 1.0f };
    float row_h = EDITOR_SETTINGS_ROW_H;
    bool changed = false;
    float draw_y = *y - scroll;

    if (editor_row_visible(draw_y, row_h, view_top, view_bottom)) {
        pz_ui_label(editor->ui, x, draw_y + 2.0f, label, text_color);

        const char *state_label = *value ? "On" : "Off";
        if (pz_ui_button(editor->ui, x + EDITOR_SETTINGS_LABEL_W, draw_y, 80.0f,
                row_h - 4.0f, state_label)) {
            *value = !*value;
            changed = true;
        }
    }

    *y += row_h;
    return changed;
}

static bool
editor_draw_color_editor(pz_editor *editor, float x, float width, float *y,
    const char *label, pz_vec3 *color, float scroll, float view_top,
    float view_bottom)
{
    if (!editor || !label || !y || !color) {
        return false;
    }

    const pz_vec4 text_color = { 0.85f, 0.85f, 0.9f, 1.0f };
    bool changed = false;

    float swatch_size = EDITOR_SETTINGS_ROW_H - 6.0f;
    float swatch_x = x + width - swatch_size;
    float draw_y = *y - scroll;

    if (editor_row_visible(
            draw_y, EDITOR_SETTINGS_ROW_H, view_top, view_bottom)) {
        pz_ui_label(editor->ui, x, draw_y + 2.0f, label, text_color);
        pz_vec4 swatch_color = { color->x, color->y, color->z, 1.0f };
        pz_ui_rect(editor->ui, swatch_x, draw_y, swatch_size, swatch_size,
            swatch_color);
        pz_ui_rect_outline(editor->ui, swatch_x, draw_y, swatch_size,
            swatch_size, (pz_vec4) { 0.0f, 0.0f, 0.0f, 0.7f }, 1.0f);
    }
    *y += EDITOR_SETTINGS_ROW_H;

    changed |= editor_draw_float_row(editor, x, width, y, "Red", &color->x,
        0.05f, 0.0f, 1.0f, "%.2f", scroll, view_top, view_bottom);
    changed |= editor_draw_float_row(editor, x, width, y, "Green", &color->y,
        0.05f, 0.0f, 1.0f, "%.2f", scroll, view_top, view_bottom);
    changed |= editor_draw_float_row(editor, x, width, y, "Blue", &color->z,
        0.05f, 0.0f, 1.0f, "%.2f", scroll, view_top, view_bottom);

    color->x = pz_clampf(color->x, 0.0f, 1.0f);
    color->y = pz_clampf(color->y, 0.0f, 1.0f);
    color->z = pz_clampf(color->z, 0.0f, 1.0f);

    return changed;
}

static pz_vec2
editor_world_to_tile(const pz_map *map, pz_vec2 world)
{
    if (!map || map->tile_size <= 0.0f) {
        return pz_vec2_zero();
    }

    float half_w = map->world_width * 0.5f;
    float half_h = map->world_height * 0.5f;
    return (pz_vec2) {
        .x = (world.x + half_w - map->tile_size * 0.5f) / map->tile_size,
        .y = (world.y + half_h - map->tile_size * 0.5f) / map->tile_size,
    };
}

static pz_vec2
editor_tile_to_world(const pz_map *map, pz_vec2 tile)
{
    if (!map || map->tile_size <= 0.0f) {
        return pz_vec2_zero();
    }

    float half_w = map->world_width * 0.5f;
    float half_h = map->world_height * 0.5f;
    return (pz_vec2) {
        .x = tile.x * map->tile_size + map->tile_size * 0.5f - half_w,
        .y = tile.y * map->tile_size + map->tile_size * 0.5f - half_h,
    };
}

static void
editor_render_map_settings_dialog(
    pz_editor *editor, int logical_width, int logical_height, bool allow_input)
{
    (void)logical_width;
    (void)logical_height;

    pz_ui_window_state *state
        = (pz_ui_window_state *)&editor->map_settings_window;
    pz_ui_window_result win = pz_ui_window(editor->ui, "Map Settings",
        EDITOR_MAP_SETTINGS_W, EDITOR_MAP_SETTINGS_H,
        &editor->map_settings_dialog_open, state, allow_input);

    if (!win.visible) {
        return;
    }

    if (!editor->map) {
        pz_ui_label(editor->ui, win.content_x, win.content_y, "No map loaded",
            (pz_vec4) { 0.9f, 0.5f, 0.5f, 1.0f });
        return;
    }

    pz_ui_clip_begin(
        editor->ui, win.content_x, win.content_y, win.content_w, win.content_h);

    // Scroll setup
    float scroll = editor->map_settings_scroll;
    float view_top = win.content_y;
    float view_bottom = win.content_y + win.content_h;
    float view_height = win.content_h;

    float x = win.content_x;
    float y
        = win.content_y; // Absolute y position (scroll applied when drawing)
    float width = win.content_w;
    const pz_vec4 header_color = { 0.95f, 0.95f, 1.0f, 1.0f };
    const pz_vec4 text_color = { 0.85f, 0.85f, 0.9f, 1.0f };

    // ========================================================================
    // General
    // ========================================================================
    editor_draw_section_header(editor, x, width, &y, "General", header_color,
        scroll, view_top, view_bottom);

    // Name row
    float row_draw_y = y - scroll;
    if (editor_row_visible(
            row_draw_y, EDITOR_SETTINGS_ROW_H, view_top, view_bottom)) {
        pz_ui_label(editor->ui, x, row_draw_y, "Name:", text_color);
        pz_ui_label(
            editor->ui, x + 60.0f, row_draw_y, editor->map->name, text_color);
        float rename_btn_w = 80.0f;
        if (pz_ui_button(editor->ui, x + width - rename_btn_w,
                row_draw_y - 2.0f, rename_btn_w, EDITOR_SETTINGS_ROW_H - 4.0f,
                "Rename")) {
            editor_open_map_name_dialog(editor);
        }
    }
    y += EDITOR_SETTINGS_ROW_H;

    // Music row
    row_draw_y = y - scroll;
    if (editor_row_visible(
            row_draw_y, EDITOR_SETTINGS_ROW_H, view_top, view_bottom)) {
        pz_ui_label(editor->ui, x, row_draw_y, "Music:", text_color);
        int music_index = 0;
        bool music_known = false;
        if (editor->map->has_music && editor->map->music_name[0]) {
            for (int i = 1; i < EDITOR_MUSIC_OPTION_COUNT; i++) {
                if (strcmp(editor->map->music_name, EDITOR_MUSIC_OPTIONS[i])
                    == 0) {
                    music_index = i;
                    music_known = true;
                    break;
                }
            }
        }
        const char *music_label = (editor->map->has_music && !music_known)
            ? editor->map->music_name
            : EDITOR_MUSIC_OPTIONS[music_index];
        if (pz_ui_button(editor->ui, x + EDITOR_SETTINGS_LABEL_W,
                row_draw_y - 2.0f, 140.0f, EDITOR_SETTINGS_ROW_H - 4.0f,
                music_label)) {
            int next_index = music_known ? music_index : 0;
            next_index = (next_index + 1) % EDITOR_MUSIC_OPTION_COUNT;
            if (next_index == 0) {
                editor->map->has_music = false;
                editor->map->music_name[0] = '\0';
            } else {
                editor->map->has_music = true;
                strncpy(editor->map->music_name,
                    EDITOR_MUSIC_OPTIONS[next_index],
                    sizeof(editor->map->music_name) - 1);
                editor->map->music_name[sizeof(editor->map->music_name) - 1]
                    = '\0';
            }
            editor_mark_map_settings_changed(editor);
        }
    }
    y += EDITOR_SETTINGS_ROW_H;

    float tile_size = editor->map->tile_size;
    if (editor_draw_float_row(editor, x, width, &y, "Tile Size", &tile_size,
            0.25f, 0.5f, 6.0f, "%.2f", scroll, view_top, view_bottom)) {
        editor->map->tile_size = tile_size;
        editor->map->world_width = editor->map->width * editor->map->tile_size;
        editor->map->world_height
            = editor->map->height * editor->map->tile_size;
        editor->camera_zoom = editor_calculate_zoom(editor);
        editor_mark_map_settings_changed(editor);
    }

    y += 6.0f;

    // ========================================================================
    // Background
    // ========================================================================
    editor_draw_section_header(editor, x, width, &y, "Background", header_color,
        scroll, view_top, view_bottom);

    // Mode row
    row_draw_y = y - scroll;
    if (editor_row_visible(
            row_draw_y, EDITOR_SETTINGS_ROW_H, view_top, view_bottom)) {
        pz_ui_label(editor->ui, x, row_draw_y, "Mode", text_color);
        int bg_mode = 0;
        if (editor->map->background.type == PZ_BACKGROUND_GRADIENT) {
            bg_mode
                = (editor->map->background.gradient_dir == PZ_GRADIENT_RADIAL)
                ? 2
                : 1;
        }
        const char *bg_labels[] = { "Solid", "Vertical", "Radial" };
        if (pz_ui_button(editor->ui, x + EDITOR_SETTINGS_LABEL_W,
                row_draw_y - 2.0f, 100.0f, EDITOR_SETTINGS_ROW_H - 4.0f,
                bg_labels[bg_mode])) {
            bg_mode = (bg_mode + 1) % 3;
            if (bg_mode == 0) {
                editor->map->background.type = PZ_BACKGROUND_COLOR;
            } else {
                editor->map->background.type = PZ_BACKGROUND_GRADIENT;
                editor->map->background.gradient_dir = (bg_mode == 2)
                    ? PZ_GRADIENT_RADIAL
                    : PZ_GRADIENT_VERTICAL;
            }
            editor_mark_map_settings_changed(editor);
        }
    }
    y += EDITOR_SETTINGS_ROW_H;

    if (editor->map->background.type == PZ_BACKGROUND_COLOR) {
        if (editor_draw_color_editor(editor, x, width, &y, "Color",
                &editor->map->background.color, scroll, view_top,
                view_bottom)) {
            editor_mark_map_settings_changed(editor);
        }
    } else {
        if (editor_draw_color_editor(editor, x, width, &y, "Top Color",
                &editor->map->background.color, scroll, view_top,
                view_bottom)) {
            editor_mark_map_settings_changed(editor);
        }
        if (editor_draw_color_editor(editor, x, width, &y, "Bottom Color",
                &editor->map->background.color_end, scroll, view_top,
                view_bottom)) {
            editor_mark_map_settings_changed(editor);
        }
    }

    y += 6.0f;

    // ========================================================================
    // Lighting
    // ========================================================================
    editor_draw_section_header(editor, x, width, &y, "Lighting", header_color,
        scroll, view_top, view_bottom);
    bool sun_enabled = editor->map->lighting.has_sun;
    if (editor_draw_toggle_row(editor, x, width, &y, "Sun Enabled",
            &sun_enabled, scroll, view_top, view_bottom)) {
        editor->map->lighting.has_sun = sun_enabled;
        editor_mark_map_settings_changed(editor);
    }

    pz_vec3 sun_dir = editor->map->lighting.sun_direction;
    bool sun_dir_changed = false;
    sun_dir_changed |= editor_draw_float_row(editor, x, width, &y, "Sun Dir X",
        &sun_dir.x, 0.05f, -1.0f, 1.0f, "%.2f", scroll, view_top, view_bottom);
    sun_dir_changed |= editor_draw_float_row(editor, x, width, &y, "Sun Dir Y",
        &sun_dir.y, 0.05f, -1.0f, 1.0f, "%.2f", scroll, view_top, view_bottom);
    sun_dir_changed |= editor_draw_float_row(editor, x, width, &y, "Sun Dir Z",
        &sun_dir.z, 0.05f, -1.0f, 1.0f, "%.2f", scroll, view_top, view_bottom);
    if (sun_dir_changed) {
        float len = pz_vec3_len(sun_dir);
        if (len < 0.001f) {
            sun_dir = (pz_vec3) { 0.4f, -0.8f, 0.3f };
        } else {
            sun_dir = pz_vec3_scale(sun_dir, 1.0f / len);
        }
        editor->map->lighting.sun_direction = sun_dir;
        editor_mark_map_settings_changed(editor);
    }

    if (editor_draw_color_editor(editor, x, width, &y, "Sun Color",
            &editor->map->lighting.sun_color, scroll, view_top, view_bottom)) {
        editor_mark_map_settings_changed(editor);
    }
    if (editor_draw_color_editor(editor, x, width, &y, "Ambient Color",
            &editor->map->lighting.ambient_color, scroll, view_top,
            view_bottom)) {
        editor_mark_map_settings_changed(editor);
    }
    if (editor_draw_float_row(editor, x, width, &y, "Ambient Darkness",
            &editor->map->lighting.ambient_darkness, 0.05f, 0.0f, 1.0f, "%.2f",
            scroll, view_top, view_bottom)) {
        editor_mark_map_settings_changed(editor);
    }

    y += 6.0f;

    // ========================================================================
    // Water
    // ========================================================================
    editor_draw_section_header(editor, x, width, &y, "Water", header_color,
        scroll, view_top, view_bottom);
    bool water_enabled = editor->map->has_water;
    if (editor_draw_toggle_row(editor, x, width, &y, "Water Enabled",
            &water_enabled, scroll, view_top, view_bottom)) {
        editor->map->has_water = water_enabled;
        if (water_enabled && editor->map->water_level < -50) {
            editor->map->water_level = -1;
        }
        editor_mark_map_settings_changed(editor);
    }
    if (editor->map->has_water) {
        int water_level = editor->map->water_level;
        if (editor_draw_int_row(editor, x, width, &y, "Water Level",
                &water_level, 1, -10, 10, scroll, view_top, view_bottom)) {
            editor->map->water_level = water_level;
            editor_mark_map_settings_changed(editor);
        }
        if (editor_draw_color_editor(editor, x, width, &y, "Water Color",
                &editor->map->water_color, scroll, view_top, view_bottom)) {
            editor_mark_map_settings_changed(editor);
        }
        if (editor_draw_float_row(editor, x, width, &y, "Wave Strength",
                &editor->map->wave_strength, 0.1f, 0.1f, 5.0f, "%.2f", scroll,
                view_top, view_bottom)) {
            editor_mark_map_settings_changed(editor);
        }
        if (editor_draw_float_row(editor, x, width, &y, "Wind Dir (rad)",
                &editor->map->wind_direction, 0.1f, 0.0f, PZ_PI * 2.0f, "%.2f",
                scroll, view_top, view_bottom)) {
            float full_turn = PZ_PI * 2.0f;
            while (editor->map->wind_direction < 0.0f) {
                editor->map->wind_direction += full_turn;
            }
            while (editor->map->wind_direction >= full_turn) {
                editor->map->wind_direction -= full_turn;
            }
            editor_mark_map_settings_changed(editor);
        }
        if (editor_draw_float_row(editor, x, width, &y, "Wind Strength",
                &editor->map->wind_strength, 0.1f, 0.0f, 5.0f, "%.2f", scroll,
                view_top, view_bottom)) {
            editor_mark_map_settings_changed(editor);
        }
    }

    y += 6.0f;

    // ========================================================================
    // Fog
    // ========================================================================
    editor_draw_section_header(editor, x, width, &y, "Fog", header_color,
        scroll, view_top, view_bottom);
    bool fog_enabled = editor->map->has_fog;
    if (editor_draw_toggle_row(editor, x, width, &y, "Fog Enabled",
            &fog_enabled, scroll, view_top, view_bottom)) {
        editor->map->has_fog = fog_enabled;
        if (fog_enabled && editor->map->fog_level < -50) {
            editor->map->fog_level = 0;
        }
        editor_mark_map_settings_changed(editor);
    }
    if (editor->map->has_fog) {
        int fog_level = editor->map->fog_level;
        if (editor_draw_int_row(editor, x, width, &y, "Fog Level", &fog_level,
                1, -10, 10, scroll, view_top, view_bottom)) {
            editor->map->fog_level = fog_level;
            editor_mark_map_settings_changed(editor);
        }
        if (editor_draw_color_editor(editor, x, width, &y, "Fog Color",
                &editor->map->fog_color, scroll, view_top, view_bottom)) {
            editor_mark_map_settings_changed(editor);
        }
    }

    y += 6.0f;

    // ========================================================================
    // Toxic Cloud
    // ========================================================================
    editor_draw_section_header(editor, x, width, &y, "Toxic Cloud",
        header_color, scroll, view_top, view_bottom);
    bool toxic_enabled
        = editor->map->has_toxic_cloud && editor->map->toxic_config.enabled;
    if (editor_draw_toggle_row(editor, x, width, &y, "Toxic Enabled",
            &toxic_enabled, scroll, view_top, view_bottom)) {
        editor->map->has_toxic_cloud = toxic_enabled;
        editor->map->toxic_config.enabled = toxic_enabled;
        editor_mark_map_settings_changed(editor);
    }
    if (editor->map->has_toxic_cloud && editor->map->toxic_config.enabled) {
        pz_toxic_cloud_config *cfg = &editor->map->toxic_config;
        bool settings_changed = false;
        settings_changed
            |= editor_draw_float_row(editor, x, width, &y, "Delay", &cfg->delay,
                1.0f, 0.0f, 600.0f, "%.1f", scroll, view_top, view_bottom);
        settings_changed |= editor_draw_float_row(editor, x, width, &y,
            "Duration", &cfg->duration, 5.0f, 5.0f, 600.0f, "%.1f", scroll,
            view_top, view_bottom);
        settings_changed |= editor_draw_float_row(editor, x, width, &y,
            "Safe Zone", &cfg->safe_zone_ratio, 0.05f, 0.05f, 0.90f, "%.2f",
            scroll, view_top, view_bottom);

        int damage = cfg->damage;
        if (editor_draw_int_row(editor, x, width, &y, "Damage", &damage, 1, 0,
                20, scroll, view_top, view_bottom)) {
            cfg->damage = damage;
            settings_changed = true;
        }

        settings_changed |= editor_draw_float_row(editor, x, width, &y,
            "Damage Interval", &cfg->damage_interval, 0.5f, 0.5f, 30.0f, "%.1f",
            scroll, view_top, view_bottom);
        settings_changed |= editor_draw_float_row(editor, x, width, &y,
            "Slowdown", &cfg->slowdown, 0.05f, 0.1f, 1.0f, "%.2f", scroll,
            view_top, view_bottom);

        if (editor_draw_color_editor(editor, x, width, &y, "Cloud Color",
                &cfg->color, scroll, view_top, view_bottom)) {
            settings_changed = true;
        }

        bool center_changed = false;
        pz_vec2 center_tile = editor_world_to_tile(editor->map, cfg->center);
        if (editor_draw_float_row(editor, x, width, &y, "Center X",
                &center_tile.x, 0.5f, 0.0f, (float)(editor->map->width - 1),
                "%.2f", scroll, view_top, view_bottom)) {
            center_tile.x = pz_clampf(
                center_tile.x, 0.0f, (float)(editor->map->width - 1));
            center_changed = true;
        }
        if (editor_draw_float_row(editor, x, width, &y, "Center Y",
                &center_tile.y, 0.5f, 0.0f, (float)(editor->map->height - 1),
                "%.2f", scroll, view_top, view_bottom)) {
            center_tile.y = pz_clampf(
                center_tile.y, 0.0f, (float)(editor->map->height - 1));
            center_changed = true;
        }
        if (center_changed) {
            cfg->center = editor_tile_to_world(editor->map, center_tile);
            settings_changed = true;
        }

        if (settings_changed) {
            editor_mark_map_settings_changed(editor);
        }
    }

    pz_ui_clip_end(editor->ui);

    // Update max scroll based on content height
    float content_height = y - win.content_y;
    float max_scroll = content_height - view_height;
    if (max_scroll < 0.0f) {
        max_scroll = 0.0f;
    }
    editor->map_settings_max_scroll = max_scroll;

    // Clamp scroll
    if (editor->map_settings_scroll > max_scroll) {
        editor->map_settings_scroll = max_scroll;
    }
    if (editor->map_settings_scroll < 0.0f) {
        editor->map_settings_scroll = 0.0f;
    }

    // Store window rect for scroll handling
    editor->map_settings_window_x = win.content_x;
    editor->map_settings_window_y = win.content_y;
    editor->map_settings_window_w = win.content_w;
    editor->map_settings_window_h = win.content_h;
    editor->map_settings_visible = true;
}

static void
editor_rebuild_grid(pz_editor *editor)
{
    if (!editor || !editor->map) {
        return;
    }

    // Calculate grid bounds including padding
    int min_x = -EDITOR_PADDING_TILES;
    int max_x = editor->map->width + EDITOR_PADDING_TILES;
    int min_y = -EDITOR_PADDING_TILES;
    int max_y = editor->map->height + EDITOR_PADDING_TILES;

    // Calculate number of lines needed
    int h_lines = max_y - min_y + 1;
    int v_lines = max_x - min_x + 1;
    int total_lines = h_lines + v_lines;
    int vertex_count = total_lines * 2; // 2 vertices per line

    // Allocate vertex data (position3 + color4)
    size_t vertex_size = sizeof(float) * 7;
    float *vertices = pz_alloc(vertex_count * vertex_size);
    if (!vertices) {
        return;
    }

    float *v = vertices;
    float tile_size = editor->map->tile_size;
    float half_w = editor->map->world_width / 2.0f;
    float half_h = editor->map->world_height / 2.0f;
    float grid_y = 0.02f; // Slightly above ground to avoid z-fighting

    // Determine grid extents in world space
    float world_min_x = min_x * tile_size - half_w;
    float world_max_x = max_x * tile_size - half_w;
    float world_min_z = min_y * tile_size - half_h;
    float world_max_z = max_y * tile_size - half_h;

    // Horizontal lines (along X axis)
    for (int y = min_y; y <= max_y; y++) {
        float world_z = y * tile_size - half_h;

        // Determine if this line is inside or outside the map
        bool inside = (y >= 0 && y <= editor->map->height);
        float alpha
            = inside ? EDITOR_GRID_LINE_ALPHA : EDITOR_GRID_EXPANSION_ALPHA;

        // Start point
        *v++ = world_min_x;
        *v++ = grid_y;
        *v++ = world_z;
        *v++ = 1.0f;
        *v++ = 1.0f;
        *v++ = 1.0f;
        *v++ = alpha;

        // End point
        *v++ = world_max_x;
        *v++ = grid_y;
        *v++ = world_z;
        *v++ = 1.0f;
        *v++ = 1.0f;
        *v++ = 1.0f;
        *v++ = alpha;
    }

    // Vertical lines (along Z axis)
    for (int x = min_x; x <= max_x; x++) {
        float world_x = x * tile_size - half_w;

        // Determine if this line is inside or outside the map
        bool inside = (x >= 0 && x <= editor->map->width);
        float alpha
            = inside ? EDITOR_GRID_LINE_ALPHA : EDITOR_GRID_EXPANSION_ALPHA;

        // Start point
        *v++ = world_x;
        *v++ = grid_y;
        *v++ = world_min_z;
        *v++ = 1.0f;
        *v++ = 1.0f;
        *v++ = 1.0f;
        *v++ = alpha;

        // End point
        *v++ = world_x;
        *v++ = grid_y;
        *v++ = world_max_z;
        *v++ = 1.0f;
        *v++ = 1.0f;
        *v++ = 1.0f;
        *v++ = alpha;
    }

    // Create or update vertex buffer
    if (editor->grid_vb != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(editor->renderer, editor->grid_vb);
        editor->grid_vb = PZ_INVALID_HANDLE;
    }

    pz_buffer_desc vb_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STATIC,
        .data = vertices,
        .size = vertex_count * vertex_size,
    };
    editor->grid_vb = pz_renderer_create_buffer(editor->renderer, &vb_desc);
    editor->grid_vertex_count = vertex_count;

    pz_free(vertices);
}
