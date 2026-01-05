/*
 * Tank Game - Toxic Cloud Rendering Helpers
 */

#include "pz_toxic_cloud.h"

#include <math.h>

#include "pz_particle.h"

static float
pz_toxic_cloud_safe_area(const pz_toxic_cloud *cloud)
{
    float width = cloud->boundary_right - cloud->boundary_left;
    float height = cloud->boundary_bottom - cloud->boundary_top;
    float radius = cloud->corner_radius;

    float max_radius = pz_minf(width, height) * 0.5f;
    radius = pz_clampf(radius, 0.0f, max_radius);

    float area = width * height;
    float corner_cut = (4.0f - PZ_PI) * radius * radius;
    return pz_maxf(area - corner_cut, 0.0f);
}

static float
pz_toxic_cloud_distance_to_safe_boundary(
    const pz_toxic_cloud *cloud, pz_vec2 pos)
{
    float center_x = (cloud->boundary_left + cloud->boundary_right) * 0.5f;
    float center_y = (cloud->boundary_top + cloud->boundary_bottom) * 0.5f;
    float half_w = (cloud->boundary_right - cloud->boundary_left) * 0.5f;
    float half_h = (cloud->boundary_bottom - cloud->boundary_top) * 0.5f;
    float radius = cloud->corner_radius;

    float max_radius = pz_minf(half_w, half_h);
    radius = pz_clampf(radius, 0.0f, max_radius);

    float px = fabsf(pos.x - center_x);
    float py = fabsf(pos.y - center_y);
    float inner_w = pz_maxf(half_w - radius, 0.0f);
    float inner_h = pz_maxf(half_h - radius, 0.0f);

    float qx = px - inner_w;
    float qy = py - inner_h;
    float mx = pz_maxf(qx, 0.0f);
    float my = pz_maxf(qy, 0.0f);
    float outside = sqrtf(mx * mx + my * my) - radius;
    float inside = pz_minf(pz_maxf(qx, qy), 0.0f) - radius;
    float signed_dist = outside + inside;
    return signed_dist > 0.0f ? signed_dist : 0.0f;
}

static pz_vec2
pz_toxic_cloud_random_position(
    const pz_toxic_cloud *cloud, pz_particle_manager *particles)
{
    float half_w = cloud->map_width * 0.5f;
    float half_h = cloud->map_height * 0.5f;
    float map_left = cloud->map_center.x - half_w;
    float map_right = cloud->map_center.x + half_w;
    float map_top = cloud->map_center.y - half_h;
    float map_bottom = cloud->map_center.y + half_h;

    for (int i = 0; i < 24; i++) {
        float x = pz_rng_range(&particles->rng, map_left, map_right);
        float y = pz_rng_range(&particles->rng, map_top, map_bottom);
        pz_vec2 pos = { x, y };
        if (pz_toxic_cloud_is_inside(cloud, pos)) {
            return pos;
        }
    }

    float x = pz_rng_range(&particles->rng, map_left, map_right);
    float y = pz_rng_range(&particles->rng, map_top, map_bottom);
    return (pz_vec2) { x, y };
}

void
pz_toxic_cloud_spawn_particles(
    pz_toxic_cloud *cloud, pz_particle_manager *particles, float dt)
{
    if (!cloud || !particles || !cloud->config.enabled) {
        return;
    }

    if (cloud->closing_progress <= 0.0f) {
        cloud->spawn_timer = 0.0f;
        return;
    }

    float map_area = cloud->map_width * cloud->map_height;
    float safe_area = pz_toxic_cloud_safe_area(cloud);
    safe_area = pz_clampf(safe_area, 0.0f, map_area);

    float toxic_area = map_area - safe_area;
    if (toxic_area <= 0.01f) {
        return;
    }

    int max_active = (int)(PZ_MAX_PARTICLES * 0.75f);
    if (pz_particle_count(particles) >= max_active) {
        return;
    }

    float density = 2.0f;
    float target_active = toxic_area / density;
    float avg_lifetime = 3.2f;
    float spawn_rate = target_active / avg_lifetime;

    cloud->spawn_timer += dt * spawn_rate;
    int spawn_count = (int)cloud->spawn_timer;
    if (spawn_count <= 0) {
        return;
    }

    cloud->spawn_timer -= (float)spawn_count;

    for (int i = 0; i < spawn_count; i++) {
        if (pz_particle_count(particles) >= max_active) {
            break;
        }
        pz_vec2 pos = pz_toxic_cloud_random_position(cloud, particles);
        if (!pz_toxic_cloud_is_inside(cloud, pos)) {
            continue;
        }

        float distance = pz_toxic_cloud_distance_to_safe_boundary(cloud, pos);
        float edge_fade = pz_clampf(distance / 3.0f, 0.3f, 1.0f);
        pz_particle_spawn_toxic(particles, (pz_vec3) { pos.x, 0.0f, pos.y },
            cloud->config.color, edge_fade);
    }
}
