/*
 * Tank Game - Collision Primitives
 */

#ifndef PZ_COLLISION_H
#define PZ_COLLISION_H

#include "../core/pz_math.h"

typedef enum pz_collider_type {
    PZ_COLLIDER_CIRCLE,
    PZ_COLLIDER_AABB,
} pz_collider_type;

typedef struct pz_circle {
    pz_vec2 center;
    float radius;
} pz_circle;

typedef struct pz_aabb {
    pz_vec2 min;
    pz_vec2 max;
} pz_aabb;

typedef struct pz_collider {
    pz_collider_type type;
    union {
        pz_circle circle;
        pz_aabb aabb;
    };
} pz_collider;

static inline pz_circle
pz_circle_new(pz_vec2 center, float radius)
{
    return (pz_circle) { center, radius };
}

static inline pz_aabb
pz_aabb_new(pz_vec2 min, pz_vec2 max)
{
    return (pz_aabb) { min, max };
}

static inline pz_aabb
pz_aabb_from_center(pz_vec2 center, pz_vec2 half_extents)
{
    return (pz_aabb) {
        .min = { center.x - half_extents.x, center.y - half_extents.y },
        .max = { center.x + half_extents.x, center.y + half_extents.y },
    };
}

bool pz_collision_circle_circle(
    pz_circle a, pz_circle b, pz_vec2 *normal, float *penetration);
bool pz_collision_circle_aabb(pz_circle circle, pz_aabb box, pz_vec2 *push_out);
bool pz_collision_aabb_aabb(pz_aabb a, pz_aabb b);

#endif // PZ_COLLISION_H
