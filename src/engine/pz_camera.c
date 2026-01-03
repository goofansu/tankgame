/*
 * Tank Game - Camera System Implementation
 */

#include "pz_camera.h"
#include "../core/pz_log.h"
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================
 */

#define DEFAULT_FOV 45.0f
#define DEFAULT_NEAR 0.1f
#define DEFAULT_FAR 1000.0f
#define DEFAULT_HEIGHT 20.0f
#define DEFAULT_PITCH 20.0f // degrees from vertical

/* ============================================================================
 * Lifecycle
 * ============================================================================
 */

void
pz_camera_init(pz_camera *cam, int viewport_width, int viewport_height)
{
    cam->viewport_width = viewport_width;
    cam->viewport_height = viewport_height;

    // Set default projection parameters
    cam->fov = DEFAULT_FOV;
    cam->aspect = (float)viewport_width / (float)viewport_height;
    cam->near_plane = DEFAULT_NEAR;
    cam->far_plane = DEFAULT_FAR;

    // Default up vector
    cam->up = (pz_vec3) { 0.0f, 1.0f, 0.0f };

    // Set up default game view
    pz_camera_setup_game_view(
        cam, (pz_vec3) { 0, 0, 0 }, DEFAULT_HEIGHT, DEFAULT_PITCH);
}

void
pz_camera_update(pz_camera *cam)
{
    // Update aspect ratio
    if (cam->viewport_height > 0) {
        cam->aspect = (float)cam->viewport_width / (float)cam->viewport_height;
    }

    // Compute view matrix
    cam->view = pz_mat4_look_at(cam->position, cam->target, cam->up);

    // Compute projection matrix
    cam->projection = pz_mat4_perspective(cam->fov * (PZ_PI / 180.0f),
        cam->aspect, cam->near_plane, cam->far_plane);

    // Compute combined view-projection
    cam->view_projection = pz_mat4_mul(cam->projection, cam->view);

    // Compute inverse for screen->world conversion
    cam->inverse_view_projection = pz_mat4_inverse(cam->view_projection);
}

/* ============================================================================
 * Setters
 * ============================================================================
 */

void
pz_camera_set_position(pz_camera *cam, pz_vec3 position)
{
    cam->position = position;
    pz_camera_update(cam);
}

void
pz_camera_set_target(pz_camera *cam, pz_vec3 target)
{
    cam->target = target;
    pz_camera_update(cam);
}

void
pz_camera_look_at(pz_camera *cam, pz_vec3 position, pz_vec3 target)
{
    cam->position = position;
    cam->target = target;
    pz_camera_update(cam);
}

void
pz_camera_set_viewport(pz_camera *cam, int width, int height)
{
    cam->viewport_width = width;
    cam->viewport_height = height;
    pz_camera_update(cam);
}

void
pz_camera_set_fov(pz_camera *cam, float fov)
{
    cam->fov = fov;
    pz_camera_update(cam);
}

/* ============================================================================
 * Getters
 * ============================================================================
 */

const pz_mat4 *
pz_camera_get_view(const pz_camera *cam)
{
    return &cam->view;
}

const pz_mat4 *
pz_camera_get_projection(const pz_camera *cam)
{
    return &cam->projection;
}

const pz_mat4 *
pz_camera_get_view_projection(const pz_camera *cam)
{
    return &cam->view_projection;
}

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================
 */

pz_vec3
pz_camera_screen_to_ray(const pz_camera *cam, int screen_x, int screen_y)
{
    // Convert screen coordinates to normalized device coordinates (-1 to 1)
    float ndc_x = (2.0f * screen_x) / (float)cam->viewport_width - 1.0f;
    float ndc_y
        = 1.0f - (2.0f * screen_y) / (float)cam->viewport_height; // Flip Y

    // Create points at near and far planes in NDC
    pz_vec4 near_point = { ndc_x, ndc_y, -1.0f, 1.0f };
    pz_vec4 far_point = { ndc_x, ndc_y, 1.0f, 1.0f };

    // Transform to world space
    pz_vec4 near_world
        = pz_mat4_mul_vec4(cam->inverse_view_projection, near_point);
    pz_vec4 far_world
        = pz_mat4_mul_vec4(cam->inverse_view_projection, far_point);

    // Perspective divide
    if (fabsf(near_world.w) > 0.0001f) {
        near_world.x /= near_world.w;
        near_world.y /= near_world.w;
        near_world.z /= near_world.w;
    }
    if (fabsf(far_world.w) > 0.0001f) {
        far_world.x /= far_world.w;
        far_world.y /= far_world.w;
        far_world.z /= far_world.w;
    }

    // Ray direction
    pz_vec3 dir = {
        far_world.x - near_world.x,
        far_world.y - near_world.y,
        far_world.z - near_world.z,
    };

    return pz_vec3_normalize(dir);
}

pz_vec3
pz_camera_screen_to_world(const pz_camera *cam, int screen_x, int screen_y)
{
    // Get ray from camera through screen point
    pz_vec3 ray_dir = pz_camera_screen_to_ray(cam, screen_x, screen_y);
    pz_vec3 ray_origin = cam->position;

    // Intersect with Y=0 plane
    // ray: P = origin + t * dir
    // plane: y = 0
    // origin.y + t * dir.y = 0
    // t = -origin.y / dir.y

    if (fabsf(ray_dir.y) < 0.0001f) {
        // Ray is parallel to ground plane
        return (pz_vec3) { 0, 0, 0 };
    }

    float t = -ray_origin.y / ray_dir.y;

    if (t < 0) {
        // Intersection is behind camera
        return (pz_vec3) { 0, 0, 0 };
    }

    return (pz_vec3) {
        ray_origin.x + t * ray_dir.x,
        0.0f,
        ray_origin.z + t * ray_dir.z,
    };
}

pz_vec3
pz_camera_world_to_screen(const pz_camera *cam, pz_vec3 world_pos)
{
    // Transform to clip space
    pz_vec4 clip = pz_mat4_mul_vec4(cam->view_projection,
        (pz_vec4) { world_pos.x, world_pos.y, world_pos.z, 1.0f });

    // Perspective divide
    if (fabsf(clip.w) < 0.0001f) {
        return (pz_vec3) { 0, 0, 0 };
    }

    pz_vec3 ndc = {
        clip.x / clip.w,
        clip.y / clip.w,
        clip.z / clip.w,
    };

    // Convert to screen coordinates
    float screen_x = (ndc.x + 1.0f) * 0.5f * cam->viewport_width;
    float screen_y = (1.0f - ndc.y) * 0.5f * cam->viewport_height; // Flip Y

    // Depth (0 to 1)
    float depth = (ndc.z + 1.0f) * 0.5f;

    return (pz_vec3) { screen_x, screen_y, depth };
}

/* ============================================================================
 * Movement Helpers
 * ============================================================================
 */

void
pz_camera_translate(pz_camera *cam, pz_vec3 offset)
{
    cam->position = pz_vec3_add(cam->position, offset);
    cam->target = pz_vec3_add(cam->target, offset);
    pz_camera_update(cam);
}

void
pz_camera_zoom(pz_camera *cam, float delta)
{
    // Move camera along the direction from target to camera
    pz_vec3 dir = pz_vec3_sub(cam->position, cam->target);
    float len = pz_vec3_len(dir);

    // Clamp zoom range
    float new_len = len + delta;
    if (new_len < 5.0f)
        new_len = 5.0f;
    if (new_len > 100.0f)
        new_len = 100.0f;

    // Scale direction to new length
    dir = pz_vec3_scale(pz_vec3_normalize(dir), new_len);
    cam->position = pz_vec3_add(cam->target, dir);

    pz_camera_update(cam);
}

/* ============================================================================
 * Default Game Camera Setup
 * ============================================================================
 */

void
pz_camera_setup_game_view(
    pz_camera *cam, pz_vec3 look_at_point, float height, float pitch_degrees)
{
    cam->target = look_at_point;

    // Convert pitch to radians
    // pitch_degrees: 0 = looking straight down, 90 = horizontal
    float pitch_rad = pitch_degrees * (PZ_PI / 180.0f);

    // Calculate camera offset from target
    // Camera is above and in front of the target (looks toward -Z)
    float horizontal_dist = height * tanf(pitch_rad);

    cam->position = (pz_vec3) {
        look_at_point.x, look_at_point.y + height,
        look_at_point.z + horizontal_dist, // Camera is in front of target
    };

    pz_camera_update(cam);
}

void
pz_camera_fit_map(
    pz_camera *cam, float map_width, float map_height, float pitch_degrees)
{
    // Simple approach: calculate height needed for width, add extra for depth,
    // then position camera to center the map.

    float pitch_rad = pitch_degrees * (PZ_PI / 180.0f);
    float fov_rad = cam->fov * (PZ_PI / 180.0f);

    // Height needed to fit map width horizontally
    float hfov_rad = 2.0f * atanf(tanf(fov_rad / 2.0f) * cam->aspect);
    float height_for_width = (map_width / 2.0f) / tanf(hfov_rad / 2.0f);

    // For depth: approximate by treating it as foreshortened
    float cos_pitch = cosf(pitch_rad);
    float apparent_depth = map_height * cos_pitch;
    float height_for_depth = (apparent_depth / 2.0f) / tanf(fov_rad / 2.0f);

    // Take the larger, add small margin
    float height = height_for_depth > height_for_width ? height_for_depth
                                                       : height_for_width;
    height *= 1.05f; // 5% margin to ensure everything fits

    // Position: look slightly behind map center (negative Z) to
    // move the map up on screen and reduce empty space at top
    float z_offset = -map_height * 0.05f;
    pz_vec3 look_at = { 0.0f, 0.0f, z_offset };

    pz_camera_setup_game_view(cam, look_at, height, pitch_degrees);
}
