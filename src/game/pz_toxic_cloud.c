/*
 * Tank Game - Toxic Cloud System Implementation
 */

#include "pz_toxic_cloud.h"

#include <math.h>
#include <string.h>

#include "../core/pz_mem.h"

#define PZ_TOXIC_DAMAGE_INSET 0.5f

pz_toxic_cloud_config
pz_toxic_cloud_config_default(pz_vec2 map_center)
{
    pz_toxic_cloud_config config = {
        .enabled = false,
        .delay = PZ_TOXIC_DEFAULT_DELAY,
        .duration = PZ_TOXIC_DEFAULT_DURATION,
        .safe_zone_ratio = PZ_TOXIC_DEFAULT_SAFE_ZONE_RATIO,
        .damage = PZ_TOXIC_DEFAULT_DAMAGE,
        .damage_interval = PZ_TOXIC_DEFAULT_DAMAGE_INTERVAL,
        .slowdown = PZ_TOXIC_DEFAULT_SLOWDOWN,
        .color = { 0.2f, 0.8f, 0.3f },
        .center = map_center,
        .grace_period = PZ_TOXIC_DEFAULT_GRACE_PERIOD,
    };
    return config;
}

static void
pz_toxic_cloud_update_boundary(pz_toxic_cloud *cloud)
{
    float half_w = cloud->map_width * 0.5f;
    float half_h = cloud->map_height * 0.5f;
    float map_left = cloud->map_center.x - half_w;
    float map_right = cloud->map_center.x + half_w;
    float map_top = cloud->map_center.y - half_h;
    float map_bottom = cloud->map_center.y + half_h;

    float safe_ratio = pz_clampf(cloud->config.safe_zone_ratio, 0.01f, 1.0f);
    float safe_radius
        = pz_minf(cloud->map_width, cloud->map_height) * safe_ratio * 0.5f;

    pz_vec2 center = cloud->config.center;
    float target_left = center.x - safe_radius;
    float target_right = center.x + safe_radius;
    float target_top = center.y - safe_radius;
    float target_bottom = center.y + safe_radius;

    target_left = pz_clampf(target_left, map_left, map_right);
    target_right = pz_clampf(target_right, map_left, map_right);
    target_top = pz_clampf(target_top, map_top, map_bottom);
    target_bottom = pz_clampf(target_bottom, map_top, map_bottom);

    cloud->boundary_left
        = pz_lerpf(map_left, target_left, cloud->closing_progress);
    cloud->boundary_right
        = pz_lerpf(map_right, target_right, cloud->closing_progress);
    cloud->boundary_top
        = pz_lerpf(map_top, target_top, cloud->closing_progress);
    cloud->boundary_bottom
        = pz_lerpf(map_bottom, target_bottom, cloud->closing_progress);

    float boundary_width = cloud->boundary_right - cloud->boundary_left;
    float boundary_height = cloud->boundary_bottom - cloud->boundary_top;
    float max_radius = pz_minf(boundary_width, boundary_height) * 0.5f;
    cloud->corner_radius
        = pz_clampf(cloud->closing_progress * max_radius, 0.0f, max_radius);
}

static bool
pz_toxic_cloud_inside_safe_zone(
    const pz_toxic_cloud *cloud, pz_vec2 pos, float inset)
{
    float left = cloud->boundary_left - inset;
    float right = cloud->boundary_right + inset;
    float top = cloud->boundary_top - inset;
    float bottom = cloud->boundary_bottom + inset;
    float radius = cloud->corner_radius + inset;

    float width = right - left;
    float height = bottom - top;
    float max_radius = pz_minf(width, height) * 0.5f;
    if (radius > max_radius) {
        radius = max_radius;
    }

    if (radius <= 0.0f) {
        return pos.x >= left && pos.x <= right && pos.y >= top
            && pos.y <= bottom;
    }

    float inner_left = left + radius;
    float inner_right = right - radius;
    float inner_top = top + radius;
    float inner_bottom = bottom - radius;

    float clamped_x = pz_clampf(pos.x, inner_left, inner_right);
    float clamped_y = pz_clampf(pos.y, inner_top, inner_bottom);
    float dx = pos.x - clamped_x;
    float dy = pos.y - clamped_y;

    return (dx * dx + dy * dy) <= (radius * radius);
}

pz_toxic_cloud *
pz_toxic_cloud_create(
    const pz_toxic_cloud_config *config, float map_width, float map_height)
{
    pz_toxic_cloud *cloud = pz_alloc(sizeof(pz_toxic_cloud));
    if (!cloud) {
        return NULL;
    }

    memset(cloud, 0, sizeof(*cloud));
    cloud->map_width = map_width;
    cloud->map_height = map_height;
    cloud->map_center = pz_vec2_zero();

    if (config) {
        cloud->config = *config;
    } else {
        cloud->config = pz_toxic_cloud_config_default(cloud->map_center);
    }

    cloud->elapsed = 0.0f;
    cloud->closing_progress = 0.0f;
    cloud->closing_started = false;
    cloud->spawn_timer = 0.0f;
    pz_toxic_cloud_update_boundary(cloud);
    return cloud;
}

void
pz_toxic_cloud_destroy(pz_toxic_cloud *cloud)
{
    if (!cloud) {
        return;
    }
    pz_free(cloud);
}

void
pz_toxic_cloud_update(pz_toxic_cloud *cloud, float dt)
{
    if (!cloud) {
        return;
    }

    if (dt < 0.0f) {
        dt = 0.0f;
    }

    cloud->elapsed += dt;

    if (!cloud->config.enabled) {
        cloud->closing_started = false;
        cloud->closing_progress = 0.0f;
        cloud->spawn_timer = 0.0f;
        pz_toxic_cloud_update_boundary(cloud);
        return;
    }

    cloud->closing_started = cloud->elapsed >= cloud->config.delay;
    if (!cloud->closing_started) {
        cloud->closing_progress = 0.0f;
        pz_toxic_cloud_update_boundary(cloud);
        return;
    }

    if (cloud->config.duration <= 0.0f) {
        cloud->closing_progress = 1.0f;
    } else {
        cloud->closing_progress = pz_clampf(
            (cloud->elapsed - cloud->config.delay) / cloud->config.duration,
            0.0f, 1.0f);
    }

    pz_toxic_cloud_update_boundary(cloud);
}

bool
pz_toxic_cloud_is_inside(const pz_toxic_cloud *cloud, pz_vec2 pos)
{
    if (!cloud || !cloud->config.enabled) {
        return false;
    }

    return !pz_toxic_cloud_inside_safe_zone(cloud, pos, 0.0f);
}

bool
pz_toxic_cloud_is_damaging(const pz_toxic_cloud *cloud, pz_vec2 pos)
{
    if (!cloud || !cloud->config.enabled) {
        return false;
    }

    if (!pz_toxic_cloud_is_inside(cloud, pos)) {
        return false;
    }

    return !pz_toxic_cloud_inside_safe_zone(cloud, pos, PZ_TOXIC_DAMAGE_INSET);
}

float
pz_toxic_cloud_get_progress(const pz_toxic_cloud *cloud)
{
    if (!cloud) {
        return 0.0f;
    }
    return cloud->closing_progress;
}

void
pz_toxic_cloud_get_boundary(const pz_toxic_cloud *cloud, float *left,
    float *right, float *top, float *bottom, float *corner_radius)
{
    if (!cloud) {
        return;
    }

    if (left) {
        *left = cloud->boundary_left;
    }
    if (right) {
        *right = cloud->boundary_right;
    }
    if (top) {
        *top = cloud->boundary_top;
    }
    if (bottom) {
        *bottom = cloud->boundary_bottom;
    }
    if (corner_radius) {
        *corner_radius = cloud->corner_radius;
    }
}

pz_vec2
pz_toxic_cloud_escape_direction(const pz_toxic_cloud *cloud, pz_vec2 pos)
{
    if (!cloud || !cloud->config.enabled) {
        return pz_vec2_zero();
    }

    if (!pz_toxic_cloud_is_inside(cloud, pos)) {
        return pz_vec2_zero();
    }

    float left = cloud->boundary_left;
    float right = cloud->boundary_right;
    float top = cloud->boundary_top;
    float bottom = cloud->boundary_bottom;
    float radius = cloud->corner_radius;

    float inner_left = left + radius;
    float inner_right = right - radius;
    float inner_top = top + radius;
    float inner_bottom = bottom - radius;

    float nearest_x = pz_clampf(pos.x, inner_left, inner_right);
    float nearest_y = pz_clampf(pos.y, inner_top, inner_bottom);

    float dx = pos.x - nearest_x;
    float dy = pos.y - nearest_y;
    float dist_sq = dx * dx + dy * dy;
    if (radius > 0.0f && dist_sq > radius * radius) {
        float dist = sqrtf(dist_sq);
        if (dist > 0.0001f) {
            nearest_x += dx / dist * radius;
            nearest_y += dy / dist * radius;
        }
    }

    pz_vec2 target = { nearest_x, nearest_y };
    return pz_vec2_normalize(pz_vec2_sub(target, pos));
}

float
pz_toxic_cloud_distance_to_boundary(const pz_toxic_cloud *cloud, pz_vec2 pos)
{
    if (!cloud || !cloud->config.enabled) {
        return -1000.0f; // Very safe (negative = inside safe zone)
    }

    float left = cloud->boundary_left;
    float right = cloud->boundary_right;
    float top = cloud->boundary_top;
    float bottom = cloud->boundary_bottom;
    float radius = cloud->corner_radius;

    // Handle corner radius - find inner rectangle
    float inner_left = left + radius;
    float inner_right = right - radius;
    float inner_top = top + radius;
    float inner_bottom = bottom - radius;

    // Clamp to inner rectangle
    float nearest_x = pz_clampf(pos.x, inner_left, inner_right);
    float nearest_y = pz_clampf(pos.y, inner_top, inner_bottom);

    float dx = pos.x - nearest_x;
    float dy = pos.y - nearest_y;
    float dist_to_inner = sqrtf(dx * dx + dy * dy);

    // If in corner region, distance is relative to corner circle
    if (dist_to_inner > 0.001f) {
        // In corner region - distance is from corner circle edge
        return dist_to_inner - radius;
    }

    // In the inner rectangle - find distance to nearest edge
    float dist_left = pos.x - left;
    float dist_right = right - pos.x;
    float dist_top = pos.y - top;
    float dist_bottom = bottom - pos.y;

    float min_dist = pz_minf(
        pz_minf(dist_left, dist_right), pz_minf(dist_top, dist_bottom));

    // Negative distance means inside safe zone
    return -min_dist;
}

pz_vec2
pz_toxic_cloud_get_safe_position(
    const pz_toxic_cloud *cloud, pz_vec2 from, float margin)
{
    if (!cloud || !cloud->config.enabled) {
        return from;
    }

    // The safe zone center is where we want to go
    pz_vec2 center = cloud->config.center;

    float left = cloud->boundary_left + margin;
    float right = cloud->boundary_right - margin;
    float top = cloud->boundary_top + margin;
    float bottom = cloud->boundary_bottom - margin;

    // Clamp margins if zone is too small
    if (left > right) {
        left = right = (cloud->boundary_left + cloud->boundary_right) * 0.5f;
    }
    if (top > bottom) {
        top = bottom = (cloud->boundary_top + cloud->boundary_bottom) * 0.5f;
    }

    // If already safely inside (with margin), stay put
    if (from.x >= left && from.x <= right && from.y >= top
        && from.y <= bottom) {
        return from;
    }

    // ALWAYS move toward the center of the safe zone, not just the nearest
    // edge! This ensures AI moves to a stable position that will remain safe.
    pz_vec2 target = center;

    // Clamp center to the safe area (in case center is outside current bounds)
    target.x = pz_clampf(target.x, left, right);
    target.y = pz_clampf(target.y, top, bottom);

    return target;
}

pz_vec2
pz_toxic_cloud_get_safe_position_spread(const pz_toxic_cloud *cloud,
    pz_vec2 from, float margin, int index, int total)
{
    if (!cloud || !cloud->config.enabled) {
        return from;
    }

    // Get basic safe zone bounds with margin
    float left = cloud->boundary_left + margin;
    float right = cloud->boundary_right - margin;
    float top = cloud->boundary_top + margin;
    float bottom = cloud->boundary_bottom - margin;

    // Clamp margins if zone is too small
    if (left > right) {
        left = right = (cloud->boundary_left + cloud->boundary_right) * 0.5f;
    }
    if (top > bottom) {
        top = bottom = (cloud->boundary_top + cloud->boundary_bottom) * 0.5f;
    }

    // If already safely inside, stay put
    if (from.x >= left && from.x <= right && from.y >= top
        && from.y <= bottom) {
        return from;
    }

    pz_vec2 center = cloud->config.center;

    // Clamp center to safe area
    center.x = pz_clampf(center.x, left, right);
    center.y = pz_clampf(center.y, top, bottom);

    // If only one entity or invalid total, just go to center
    if (total <= 1 || index < 0) {
        return center;
    }

    // Calculate spread positions around center in a circle
    // Use angle based on index to distribute entities evenly
    float angle = (float)index * (2.0f * PZ_PI / (float)total);

    // Calculate spread radius - fraction of the safe zone size
    float zone_width = right - left;
    float zone_height = bottom - top;
    float spread_radius = pz_minf(zone_width, zone_height) * 0.35f;

    // Offset from center based on angle
    pz_vec2 target = {
        center.x + spread_radius * sinf(angle),
        center.y + spread_radius * cosf(angle),
    };

    // Clamp to safe bounds
    target.x = pz_clampf(target.x, left, right);
    target.y = pz_clampf(target.y, top, bottom);

    return target;
}

bool
pz_toxic_cloud_will_be_inside(
    const pz_toxic_cloud *cloud, pz_vec2 pos, float future_progress)
{
    if (!cloud || !cloud->config.enabled) {
        return false;
    }

    // Calculate what the boundary will be at future_progress
    float half_w = cloud->map_width * 0.5f;
    float half_h = cloud->map_height * 0.5f;
    float map_left = cloud->map_center.x - half_w;
    float map_right = cloud->map_center.x + half_w;
    float map_top = cloud->map_center.y - half_h;
    float map_bottom = cloud->map_center.y + half_h;

    float safe_ratio = pz_clampf(cloud->config.safe_zone_ratio, 0.01f, 1.0f);
    float safe_radius
        = pz_minf(cloud->map_width, cloud->map_height) * safe_ratio * 0.5f;

    pz_vec2 center = cloud->config.center;
    float target_left = pz_clampf(center.x - safe_radius, map_left, map_right);
    float target_right = pz_clampf(center.x + safe_radius, map_left, map_right);
    float target_top = pz_clampf(center.y - safe_radius, map_top, map_bottom);
    float target_bottom
        = pz_clampf(center.y + safe_radius, map_top, map_bottom);

    float progress = pz_clampf(future_progress, 0.0f, 1.0f);
    float future_left = pz_lerpf(map_left, target_left, progress);
    float future_right = pz_lerpf(map_right, target_right, progress);
    float future_top = pz_lerpf(map_top, target_top, progress);
    float future_bottom = pz_lerpf(map_bottom, target_bottom, progress);

    // Simple rect check (ignoring corner radius for prediction)
    return pos.x < future_left || pos.x > future_right || pos.y < future_top
        || pos.y > future_bottom;
}
