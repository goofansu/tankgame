/*
 * Tank Game - Projectile System Implementation
 */

#include "pz_projectile.h"
#include "pz_tank.h"

#include <math.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

// Projectile collision radius for tank hits
static const float PROJECTILE_RADIUS = 0.15f;

// Grace period before projectile can hit its owner (seconds)
static const float SELF_DAMAGE_GRACE_PERIOD = 0.5f;

/* ============================================================================
 * Default Configuration
 * ============================================================================
 */

const pz_projectile_config PZ_PROJECTILE_DEFAULT = {
    .speed = 11.25f, // 25% slower than original 15.0
    .max_bounces = 1,
    .lifetime = -1.0f, // Infinite lifetime (only dies on max bounces)
    .damage = 5, // 2 hits to kill (10 HP tank)
};

/* ============================================================================
 * Manager Lifecycle
 * ============================================================================
 */

pz_projectile_manager *
pz_projectile_manager_create(pz_renderer *renderer)
{
    pz_projectile_manager *mgr = pz_calloc(1, sizeof(pz_projectile_manager));

    // Create projectile mesh
    mgr->mesh = pz_mesh_create_projectile();
    if (mgr->mesh) {
        pz_mesh_upload(mgr->mesh, renderer);
    }

    // Load shader (reuse entity shader)
    mgr->shader = pz_renderer_load_shader(
        renderer, "shaders/entity.vert", "shaders/entity.frag", "projectile");

    if (mgr->shader != PZ_INVALID_HANDLE) {
        pz_pipeline_desc desc = {
            .shader = mgr->shader,
            .vertex_layout = pz_mesh_get_vertex_layout(),
            .blend = PZ_BLEND_NONE,
            .depth = PZ_DEPTH_READ_WRITE,
            .cull = PZ_CULL_BACK,
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        mgr->pipeline = pz_renderer_create_pipeline(renderer, &desc);
        mgr->render_ready = (mgr->pipeline != PZ_INVALID_HANDLE);
    }

    if (!mgr->render_ready) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Projectile rendering not available (shader/pipeline failed)");
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Projectile manager created");
    return mgr;
}

void
pz_projectile_manager_destroy(pz_projectile_manager *mgr, pz_renderer *renderer)
{
    if (!mgr)
        return;

    if (mgr->pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(renderer, mgr->pipeline);
    }
    if (mgr->shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(renderer, mgr->shader);
    }
    if (mgr->mesh) {
        pz_mesh_destroy(mgr->mesh, renderer);
    }

    pz_free(mgr);
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Projectile manager destroyed");
}

/* ============================================================================
 * Projectile Spawning
 * ============================================================================
 */

int
pz_projectile_spawn(pz_projectile_manager *mgr, pz_vec2 pos, pz_vec2 direction,
    const pz_projectile_config *config, int owner_id)
{
    if (!mgr)
        return -1;

    // Use default config if none provided
    if (!config) {
        config = &PZ_PROJECTILE_DEFAULT;
    }

    // Find free slot
    int slot = -1;
    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
        if (!mgr->projectiles[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "No free projectile slots (max=%d)", PZ_MAX_PROJECTILES);
        return -1;
    }

    // Normalize direction
    float len = pz_vec2_len(direction);
    if (len < 0.001f) {
        direction = (pz_vec2) { 0.0f, 1.0f }; // Default forward
    } else {
        direction = pz_vec2_scale(direction, 1.0f / len);
    }

    // Initialize projectile
    pz_projectile *proj = &mgr->projectiles[slot];
    proj->active = true;
    proj->pos = pos;
    proj->velocity = pz_vec2_scale(direction, config->speed);
    proj->speed = config->speed;
    proj->bounces_remaining = config->max_bounces;
    proj->lifetime = config->lifetime;
    proj->age = 0.0f;
    proj->owner_id = owner_id;
    proj->damage = config->damage;

    mgr->active_count++;

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
        "Projectile spawned at (%.2f, %.2f) dir (%.2f, %.2f)", pos.x, pos.y,
        direction.x, direction.y);

    return slot;
}

/* ============================================================================
 * Projectile Update
 * ============================================================================
 */

// Get wall normal at a position given movement direction
static pz_vec2
get_wall_normal(const pz_map *map, pz_vec2 pos, pz_vec2 dir)
{
    // Sample nearby tiles to determine wall orientation
    float step = 0.05f;

    // Check which axis is more blocked
    bool blocked_x
        = pz_map_is_solid(map, (pz_vec2) { pos.x + dir.x * step, pos.y });
    bool blocked_y
        = pz_map_is_solid(map, (pz_vec2) { pos.x, pos.y + dir.y * step });

    if (blocked_x && !blocked_y) {
        // Wall is perpendicular to X axis
        return (pz_vec2) { (dir.x > 0) ? -1.0f : 1.0f, 0.0f };
    } else if (blocked_y && !blocked_x) {
        // Wall is perpendicular to Y axis
        return (pz_vec2) { 0.0f, (dir.y > 0) ? -1.0f : 1.0f };
    } else {
        // Corner or both blocked - pick dominant direction
        if (fabsf(dir.x) > fabsf(dir.y)) {
            return (pz_vec2) { (dir.x > 0) ? -1.0f : 1.0f, 0.0f };
        } else {
            return (pz_vec2) { 0.0f, (dir.y > 0) ? -1.0f : 1.0f };
        }
    }
}

void
pz_projectile_update(pz_projectile_manager *mgr, const pz_map *map,
    pz_tank_manager *tank_mgr, float dt)
{
    if (!mgr)
        return;

    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
        pz_projectile *proj = &mgr->projectiles[i];
        if (!proj->active)
            continue;

        // Update age and lifetime
        proj->age += dt;

        if (proj->lifetime > 0.0f) {
            proj->lifetime -= dt;
            if (proj->lifetime <= 0.0f) {
                proj->active = false;
                mgr->active_count--;
                continue;
            }
        }

        // Calculate new position
        pz_vec2 new_pos
            = pz_vec2_add(proj->pos, pz_vec2_scale(proj->velocity, dt));

        // Check for tank collision first (before wall collision)
        // Exclude owner only during grace period - after that, can hit self
        if (tank_mgr) {
            int exclude_id = (proj->age < SELF_DAMAGE_GRACE_PERIOD)
                ? proj->owner_id
                : -1; // -1 means exclude nobody

            pz_tank *hit_tank = pz_tank_check_collision(
                tank_mgr, new_pos, PROJECTILE_RADIUS, exclude_id);

            if (hit_tank) {
                // Apply damage
                bool killed = pz_tank_damage(hit_tank, proj->damage);

                pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
                    "Projectile hit tank %d (damage=%d, killed=%d)",
                    hit_tank->id, proj->damage, killed);

                // Destroy projectile
                proj->active = false;
                mgr->active_count--;
                continue;
            }
        }

        // Check for wall collision
        if (map && pz_map_is_solid(map, new_pos)) {
            // Hit a wall - bounce or destroy
            if (proj->bounces_remaining > 0) {
                // Get wall normal
                pz_vec2 dir = pz_vec2_normalize(proj->velocity);
                pz_vec2 normal = get_wall_normal(map, proj->pos, dir);

                // Reflect velocity
                proj->velocity = pz_vec2_reflect(proj->velocity, normal);

                // Move slightly away from wall to avoid getting stuck
                new_pos = pz_vec2_add(proj->pos, pz_vec2_scale(normal, 0.05f));

                proj->bounces_remaining--;

                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "Projectile bounced, %d bounces left",
                    proj->bounces_remaining);
            } else {
                // No bounces left - destroy
                proj->active = false;
                mgr->active_count--;
                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "Projectile destroyed (no bounces left)");
                continue;
            }
        }

        // Check for out of bounds
        if (map && !pz_map_in_bounds_world(map, new_pos)) {
            proj->active = false;
            mgr->active_count--;
            continue;
        }

        proj->pos = new_pos;
    }
}

/* ============================================================================
 * Projectile Rendering
 * ============================================================================
 */

void
pz_projectile_render(pz_projectile_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection)
{
    if (!mgr || !renderer || !view_projection)
        return;

    if (!mgr->render_ready || mgr->active_count == 0)
        return;

    // Light parameters (same as entity rendering)
    pz_vec3 light_dir = { 0.5f, 1.0f, 0.3f };
    pz_vec3 light_color = { 0.8f, 0.75f, 0.7f };
    pz_vec3 ambient = { 0.3f, 0.35f, 0.4f };

    // Projectile color (bright yellow/orange)
    pz_vec4 proj_color = { 1.0f, 0.8f, 0.2f, 1.0f };

    // Set shared uniforms
    pz_renderer_set_uniform_vec3(
        renderer, mgr->shader, "u_light_dir", light_dir);
    pz_renderer_set_uniform_vec3(
        renderer, mgr->shader, "u_light_color", light_color);
    pz_renderer_set_uniform_vec3(renderer, mgr->shader, "u_ambient", ambient);
    pz_renderer_set_uniform_vec4(renderer, mgr->shader, "u_color", proj_color);

    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
        pz_projectile *proj = &mgr->projectiles[i];
        if (!proj->active)
            continue;

        // Calculate rotation angle from velocity (in XZ plane)
        float angle = atan2f(proj->velocity.x, proj->velocity.y);

        // Projectile flies at turret barrel height
        // turret_y_offset (0.65) + base_height (0.35) + barrel_radius (0.18)
        // = 1.18
        float height = 1.18f;

        // Build model matrix
        // Projectile mesh is built along Z axis, so rotate_y to face direction
        pz_mat4 model = pz_mat4_identity();
        model = pz_mat4_mul(model,
            pz_mat4_translate((pz_vec3) { proj->pos.x, height, proj->pos.y }));
        model = pz_mat4_mul(model, pz_mat4_rotate_y(angle));

        pz_mat4 mvp = pz_mat4_mul(*view_projection, model);

        // Set per-projectile uniforms
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_mvp", &mvp);
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_model", &model);

        // Draw
        pz_draw_cmd cmd = {
            .pipeline = mgr->pipeline,
            .vertex_buffer = mgr->mesh->buffer,
            .index_buffer = PZ_INVALID_HANDLE,
            .vertex_count = mgr->mesh->vertex_count,
            .index_count = 0,
            .vertex_offset = 0,
            .index_offset = 0,
        };
        pz_renderer_draw(renderer, &cmd);
    }
}

/* ============================================================================
 * Utility
 * ============================================================================
 */

int
pz_projectile_count(const pz_projectile_manager *mgr)
{
    return mgr ? mgr->active_count : 0;
}
