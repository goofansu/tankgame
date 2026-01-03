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

// Projectile-projectile collision radius (slightly larger than visual for
// better gameplay)
static const float PROJECTILE_VS_PROJECTILE_RADIUS = 0.18f;

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
    .scale = 1.0f,
    .color = { 1.0f, 0.8f, 0.2f, 1.0f }, // Yellow/orange
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
 * Hit Recording (for particle spawning)
 * ============================================================================
 */

static void
record_hit(pz_projectile_manager *mgr, pz_projectile_hit_type type, pz_vec2 pos)
{
    if (mgr->hit_count >= PZ_MAX_PROJECTILE_HITS)
        return;

    mgr->hits[mgr->hit_count].type = type;
    mgr->hits[mgr->hit_count].pos = pos;
    mgr->hit_count++;
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
    proj->bounce_cooldown = 0.0f;
    proj->owner_id = owner_id;
    proj->damage = config->damage;
    proj->scale = config->scale;
    proj->color = config->color;

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

    // Clear hits from previous frame
    mgr->hit_count = 0;

    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
        pz_projectile *proj = &mgr->projectiles[i];
        if (!proj->active)
            continue;

        // Update age, lifetime, and bounce cooldown
        proj->age += dt;
        if (proj->bounce_cooldown > 0.0f) {
            proj->bounce_cooldown -= dt;
        }

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
                // Apply damage and record death event if killed
                bool killed
                    = pz_tank_apply_damage(tank_mgr, hit_tank, proj->damage);

                pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
                    "Projectile hit tank %d (damage=%d, killed=%d)",
                    hit_tank->id, proj->damage, killed);

                // Record hit for particle spawning
                record_hit(mgr, PZ_HIT_TANK, new_pos);

                // Destroy projectile
                proj->active = false;
                mgr->active_count--;
                continue;
            }
        }

        // Check for projectile-projectile collision
        // Both projectiles are destroyed regardless of bullet strength
        for (int j = i + 1; j < PZ_MAX_PROJECTILES; j++) {
            pz_projectile *other = &mgr->projectiles[j];
            if (!other->active)
                continue;

            // Use collision radius for bullet interception
            float dist = pz_vec2_len(pz_vec2_sub(new_pos, other->pos));
            if (dist < PROJECTILE_VS_PROJECTILE_RADIUS * 2.0f) {
                // Both bullets destroy each other
                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "Projectiles %d and %d collided and destroyed each other",
                    i, j);

                // Record hit at midpoint for particle spawning
                pz_vec2 hit_pos
                    = pz_vec2_scale(pz_vec2_add(new_pos, other->pos), 0.5f);
                record_hit(mgr, PZ_HIT_PROJECTILE, hit_pos);

                proj->active = false;
                other->active = false;
                mgr->active_count -= 2;
                break;
            }
        }

        // Skip if this projectile was destroyed by collision with another
        if (!proj->active)
            continue;

        // Check for wall collision (only if not in bounce cooldown)
        if (map && pz_map_is_solid(map, new_pos)
            && proj->bounce_cooldown <= 0.0f) {
            // Hit a wall - bounce or destroy
            if (proj->bounces_remaining > 0) {
                // Get wall normal based on current (safe) position
                pz_vec2 dir = pz_vec2_normalize(proj->velocity);
                pz_vec2 normal = get_wall_normal(map, proj->pos, dir);

                // Reflect velocity
                proj->velocity = pz_vec2_reflect(proj->velocity, normal);

                // Push back from wall until we're in a safe position
                // Start from the old (safe) position and push along normal
                new_pos = proj->pos;
                float push_dist = 0.15f;

                // Find a safe position outside the wall
                while (pz_map_is_solid(map, new_pos) && push_dist < 1.0f) {
                    new_pos = pz_vec2_add(
                        proj->pos, pz_vec2_scale(normal, push_dist));
                    push_dist += 0.05f;
                }

                // If we couldn't find a safe spot, just stay at old position
                if (pz_map_is_solid(map, new_pos)) {
                    new_pos = proj->pos;
                }

                proj->bounces_remaining--;

                // Set cooldown to prevent immediate re-bounce
                // Cooldown = time to travel ~0.3 units at current speed
                proj->bounce_cooldown = 0.3f / proj->speed;

                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "Projectile bounced, %d bounces left, pushed %.2f",
                    proj->bounces_remaining, push_dist - 0.15f);
            } else {
                // No bounces left - destroy
                // Record hit for particle spawning
                record_hit(mgr, PZ_HIT_WALL, proj->pos);

                proj->active = false;
                mgr->active_count--;
                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "Projectile destroyed (no bounces left)");
                continue;
            }
        } else if (map && pz_map_is_solid(map, new_pos)
            && proj->bounce_cooldown > 0.0f) {
            // Still in cooldown but hitting wall - don't move into it
            new_pos = proj->pos;
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
    const pz_mat4 *view_projection, const pz_projectile_render_params *params)
{
    if (!mgr || !renderer || !view_projection)
        return;

    if (!mgr->render_ready || mgr->active_count == 0)
        return;

    // Light parameters (same as entity rendering)
    pz_vec3 light_dir = { 0.5f, 1.0f, 0.3f };
    pz_vec3 light_color = { 0.6f, 0.55f, 0.5f };
    pz_vec3 ambient = { 0.15f, 0.18f, 0.2f };

    // Set shared uniforms
    pz_renderer_set_uniform_vec3(
        renderer, mgr->shader, "u_light_dir", light_dir);
    pz_renderer_set_uniform_vec3(
        renderer, mgr->shader, "u_light_color", light_color);
    pz_renderer_set_uniform_vec3(renderer, mgr->shader, "u_ambient", ambient);

    // Set light map uniforms
    if (params && params->light_texture != PZ_INVALID_HANDLE
        && params->light_texture != 0) {
        pz_renderer_bind_texture(renderer, 0, params->light_texture);
        pz_renderer_set_uniform_int(
            renderer, mgr->shader, "u_light_texture", 0);
        pz_renderer_set_uniform_int(renderer, mgr->shader, "u_use_lighting", 1);
        pz_renderer_set_uniform_vec2(renderer, mgr->shader, "u_light_scale",
            (pz_vec2) { params->light_scale_x, params->light_scale_z });
        pz_renderer_set_uniform_vec2(renderer, mgr->shader, "u_light_offset",
            (pz_vec2) { params->light_offset_x, params->light_offset_z });
    } else {
        pz_renderer_set_uniform_int(renderer, mgr->shader, "u_use_lighting", 0);
    }

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

        // Build model matrix with scale
        // Projectile mesh is built along Z axis, so rotate_y to face direction
        pz_mat4 model = pz_mat4_identity();
        model = pz_mat4_mul(model,
            pz_mat4_translate((pz_vec3) { proj->pos.x, height, proj->pos.y }));
        model = pz_mat4_mul(model, pz_mat4_rotate_y(angle));
        model = pz_mat4_mul(model,
            pz_mat4_scale((pz_vec3) { proj->scale, proj->scale, proj->scale }));

        pz_mat4 mvp = pz_mat4_mul(*view_projection, model);

        // Set per-projectile uniforms
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_mvp", &mvp);
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_model", &model);
        pz_renderer_set_uniform_vec4(
            renderer, mgr->shader, "u_color", proj->color);

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

int
pz_projectile_count_by_owner(const pz_projectile_manager *mgr, int owner_id)
{
    if (!mgr)
        return 0;

    int count = 0;
    for (int i = 0; i < PZ_MAX_PROJECTILES; i++) {
        if (mgr->projectiles[i].active
            && mgr->projectiles[i].owner_id == owner_id) {
            count++;
        }
    }
    return count;
}

int
pz_projectile_get_hits(
    const pz_projectile_manager *mgr, pz_projectile_hit *hits, int max_hits)
{
    if (!mgr || !hits)
        return 0;

    int count = mgr->hit_count < max_hits ? mgr->hit_count : max_hits;
    for (int i = 0; i < count; i++) {
        hits[i] = mgr->hits[i];
    }
    return count;
}
