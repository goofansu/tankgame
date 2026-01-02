/*
 * Tank Game - Camera System
 *
 * Handles view/projection matrices for the game's near-top-down perspective.
 * Supports mouse picking (screen to world coordinate conversion).
 */

#ifndef PZ_CAMERA_H
#define PZ_CAMERA_H

#include "../core/pz_math.h"
#include <stdbool.h>

/* ============================================================================
 * Camera Structure
 * ============================================================================
 */

typedef struct pz_camera {
    // Position and orientation
    pz_vec3 position; // World position (where camera looks from)
    pz_vec3 target; // Point camera looks at
    pz_vec3 up; // Up vector (usually 0,1,0)

    // Projection parameters
    float fov; // Field of view in degrees (vertical)
    float aspect; // Aspect ratio (width/height)
    float near_plane; // Near clipping plane
    float far_plane; // Far clipping plane

    // Cached matrices (computed in pz_camera_update)
    pz_mat4 view;
    pz_mat4 projection;
    pz_mat4 view_projection;
    pz_mat4 inverse_view_projection;

    // Viewport dimensions (for screen->world conversion)
    int viewport_width;
    int viewport_height;
} pz_camera;

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

// Initialize camera with default game settings (near-top-down view)
void pz_camera_init(pz_camera *cam, int viewport_width, int viewport_height);

// Update cached matrices after changing position/target/projection params
void pz_camera_update(pz_camera *cam);

/* ============================================================================
 * Setters
 * ============================================================================
 */

// Set camera position
void pz_camera_set_position(pz_camera *cam, pz_vec3 position);

// Set look-at target
void pz_camera_set_target(pz_camera *cam, pz_vec3 target);

// Set both position and target
void pz_camera_look_at(pz_camera *cam, pz_vec3 position, pz_vec3 target);

// Set viewport dimensions
void pz_camera_set_viewport(pz_camera *cam, int width, int height);

// Set field of view (degrees)
void pz_camera_set_fov(pz_camera *cam, float fov);

/* ============================================================================
 * Getters
 * ============================================================================
 */

// Get the view matrix
const pz_mat4 *pz_camera_get_view(const pz_camera *cam);

// Get the projection matrix
const pz_mat4 *pz_camera_get_projection(const pz_camera *cam);

// Get the combined view-projection matrix
const pz_mat4 *pz_camera_get_view_projection(const pz_camera *cam);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================
 */

// Convert screen coordinates to world position on the XZ ground plane (Y=0)
// screen_x/y are in pixel coordinates (0,0 = top-left)
// Returns the world position, or (0,0,0) if no intersection
pz_vec3 pz_camera_screen_to_world(
    const pz_camera *cam, int screen_x, int screen_y);

// Convert world position to screen coordinates
// Returns screen position in pixels, with z = depth (0-1)
pz_vec3 pz_camera_world_to_screen(const pz_camera *cam, pz_vec3 world_pos);

// Get a ray from camera through screen point
// Returns ray direction (normalized), origin is camera position
pz_vec3 pz_camera_screen_to_ray(
    const pz_camera *cam, int screen_x, int screen_y);

/* ============================================================================
 * Movement Helpers
 * ============================================================================
 */

// Move camera and target by an offset
void pz_camera_translate(pz_camera *cam, pz_vec3 offset);

// Zoom by adjusting the height while keeping target on ground
void pz_camera_zoom(pz_camera *cam, float delta);

/* ============================================================================
 * Default Game Camera Setup
 * ============================================================================
 */

// Set up the camera for the game's typical view:
// - Near-top-down (~15-20° from vertical)
// - Looking at a point on the ground
// - height: how high above the ground
// - pitch_degrees: angle from vertical (0 = straight down, 90 = horizontal)
void pz_camera_setup_game_view(
    pz_camera *cam, pz_vec3 look_at_point, float height, float pitch_degrees);

// Set up camera to fit an entire map in view
// map_width/map_height: world size of the map
// pitch_degrees: camera tilt angle
void pz_camera_fit_map(
    pz_camera *cam, float map_width, float map_height, float pitch_degrees);

#endif // PZ_CAMERA_H
