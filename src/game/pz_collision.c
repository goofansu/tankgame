/*
 * Tank Game - Collision Primitives
 */

#include "pz_collision.h"

#include <math.h>

bool
pz_collision_circle_circle(
    pz_circle a, pz_circle b, pz_vec2 *normal, float *penetration)
{
    pz_vec2 delta = pz_vec2_sub(b.center, a.center);
    float dist_sq = pz_vec2_len_sq(delta);
    float radius_sum = a.radius + b.radius;
    float radius_sum_sq = radius_sum * radius_sum;

    if (dist_sq >= radius_sum_sq) {
        return false;
    }

    if (normal || penetration) {
        if (dist_sq > 0.000001f) {
            float dist = sqrtf(dist_sq);
            if (normal) {
                *normal = pz_vec2_scale(delta, 1.0f / dist);
            }
            if (penetration) {
                *penetration = radius_sum - dist;
            }
        } else {
            if (normal) {
                *normal = (pz_vec2) { 1.0f, 0.0f };
            }
            if (penetration) {
                *penetration = radius_sum;
            }
        }
    }

    return true;
}

bool
pz_collision_circle_aabb(pz_circle circle, pz_aabb box, pz_vec2 *push_out)
{
    float nearest_x = pz_clampf(circle.center.x, box.min.x, box.max.x);
    float nearest_y = pz_clampf(circle.center.y, box.min.y, box.max.y);
    float dx = circle.center.x - nearest_x;
    float dy = circle.center.y - nearest_y;
    float dist_sq = dx * dx + dy * dy;
    float radius_sq = circle.radius * circle.radius;

    if (dist_sq >= radius_sq) {
        return false;
    }

    if (!push_out) {
        return true;
    }

    if (dist_sq > 0.000001f) {
        float dist = sqrtf(dist_sq);
        float push = circle.radius - dist;
        push_out->x = (dx / dist) * push;
        push_out->y = (dy / dist) * push;
        return true;
    }

    float left = circle.center.x - box.min.x;
    float right = box.max.x - circle.center.x;
    float bottom = circle.center.y - box.min.y;
    float top = box.max.y - circle.center.y;

    float min_dist = left;
    pz_vec2 normal = { -1.0f, 0.0f };
    if (right < min_dist) {
        min_dist = right;
        normal = (pz_vec2) { 1.0f, 0.0f };
    }
    if (bottom < min_dist) {
        min_dist = bottom;
        normal = (pz_vec2) { 0.0f, -1.0f };
    }
    if (top < min_dist) {
        min_dist = top;
        normal = (pz_vec2) { 0.0f, 1.0f };
    }

    push_out->x = normal.x * (circle.radius + min_dist);
    push_out->y = normal.y * (circle.radius + min_dist);
    return true;
}

bool
pz_collision_aabb_aabb(pz_aabb a, pz_aabb b)
{
    if (a.max.x < b.min.x || a.min.x > b.max.x)
        return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y)
        return false;
    return true;
}
