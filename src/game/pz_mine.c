/*
 * Tank Game - Mine System Implementation
 */

#include "pz_mine.h"
#include "pz_projectile.h"
#include "pz_tank.h"

#include <math.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

// Visual parameters
static const float MINE_BOB_SPEED = 2.0f;
static const float MINE_BOB_AMPLITUDE = 0.08f;
static const float MINE_ROTATE_SPEED = 0.0f; // No rotation for sphere
static const float MINE_BASE_HEIGHT = 0.3f;
static const float MINE_SCALE = 1.2f; // Bigger than projectile for visibility

// Collision radius for projectile hits
static const float MINE_HIT_RADIUS = 0.4f;

/* ============================================================================
 * Manager Lifecycle
 * ============================================================================
 */

pz_mine_manager *
pz_mine_manager_create(pz_renderer *renderer)
{
    pz_mine_manager *mgr = pz_calloc(1, sizeof(pz_mine_manager));

    // Create mine mesh (dome shape)
    mgr->mesh = pz_mesh_create_mine();
    if (mgr->mesh) {
        pz_mesh_upload(mgr->mesh, renderer);
    }

    // Load entity shader (reuse existing entity shader)
    mgr->shader = pz_renderer_load_shader(
        renderer, "shaders/entity.vert", "shaders/entity.frag", "entity");

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
            "Mine rendering not available (shader/pipeline failed)");
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Mine manager created");
    return mgr;
}

void
pz_mine_manager_destroy(pz_mine_manager *mgr, pz_renderer *renderer)
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
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Mine manager destroyed");
}

/* ============================================================================
 * Explosion Recording
 * ============================================================================
 */

static void
record_explosion(pz_mine_manager *mgr, pz_vec2 pos, int owner_id)
{
    if (mgr->explosion_count >= PZ_MAX_MINE_EXPLOSIONS)
        return;

    mgr->explosions[mgr->explosion_count].pos = pos;
    mgr->explosions[mgr->explosion_count].owner_id = owner_id;
    mgr->explosion_count++;
}

/* ============================================================================
 * Mine Explosion
 * ============================================================================
 */

static void
mine_explode(pz_mine_manager *mgr, pz_mine *mine, pz_tank_manager *tank_mgr)
{
    if (!mine->active)
        return;

    // Record explosion for particles
    record_explosion(mgr, mine->pos, mine->owner_id);

    // Damage tanks in radius
    if (tank_mgr) {
        for (int i = 0; i < PZ_MAX_TANKS; i++) {
            pz_tank *tank = &tank_mgr->tanks[i];
            if (!(tank->flags & PZ_TANK_FLAG_ACTIVE))
                continue;
            if (tank->flags & PZ_TANK_FLAG_DEAD)
                continue;

            float dx = tank->pos.x - mine->pos.x;
            float dz = tank->pos.y - mine->pos.y;
            float dist = sqrtf(dx * dx + dz * dz);

            if (dist < PZ_MINE_DAMAGE_RADIUS + tank_mgr->collision_radius) {
                pz_tank_apply_damage(tank_mgr, tank, PZ_MINE_DAMAGE);
            }
        }
    }

    // Deactivate mine
    mine->active = false;
    mgr->active_count--;
}

/* ============================================================================
 * Mine Placement
 * ============================================================================
 */

int
pz_mine_place(pz_mine_manager *mgr, pz_vec2 pos, int owner_id)
{
    if (!mgr)
        return -1;

    // Find free slot
    int slot = -1;
    for (int i = 0; i < PZ_MAX_MINES; i++) {
        if (!mgr->mines[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "No free mine slots (max=%d)",
            PZ_MAX_MINES);
        return -1;
    }

    pz_mine *mine = &mgr->mines[slot];
    mine->active = true;
    mine->pos = pos;
    mine->owner_id = owner_id;
    mine->arm_timer = PZ_MINE_ARM_TIME;
    mine->bob_offset = (float)(slot % 7) * 0.9f; // Stagger animation
    mine->rotation = 0.0f;

    mgr->active_count++;

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "Mine placed at (%.2f, %.2f) by %d",
        pos.x, pos.y, owner_id);

    return slot;
}

/* ============================================================================
 * Update
 * ============================================================================
 */

int
pz_mine_update(pz_mine_manager *mgr, pz_tank_manager *tank_mgr,
    pz_projectile_manager *projectile_mgr, float dt)
{
    if (!mgr)
        return 0;

    // Clear explosion events from last frame
    mgr->explosion_count = 0;

    // Update time for animation
    mgr->time += dt;

    for (int i = 0; i < PZ_MAX_MINES; i++) {
        pz_mine *mine = &mgr->mines[i];
        if (!mine->active)
            continue;

        // Update animation
        mine->rotation += MINE_ROTATE_SPEED * dt;

        // Update arm timer
        if (mine->arm_timer > 0.0f) {
            mine->arm_timer -= dt;
            continue; // Not armed yet, skip proximity check
        }

        // Check proximity to tanks
        if (tank_mgr) {
            for (int t = 0; t < PZ_MAX_TANKS; t++) {
                pz_tank *tank = &tank_mgr->tanks[t];
                if (!(tank->flags & PZ_TANK_FLAG_ACTIVE))
                    continue;
                if (tank->flags & PZ_TANK_FLAG_DEAD)
                    continue;

                float dx = tank->pos.x - mine->pos.x;
                float dz = tank->pos.y - mine->pos.y;
                float dist = sqrtf(dx * dx + dz * dz);

                if (dist
                    < PZ_MINE_TRIGGER_RADIUS + tank_mgr->collision_radius) {
                    mine_explode(mgr, mine, tank_mgr);
                    break;
                }
            }
        }
    }

    return mgr->explosion_count;
}

/* ============================================================================
 * Projectile Collision
 * ============================================================================
 */

bool
pz_mine_check_projectile_hit(
    pz_mine_manager *mgr, pz_vec2 pos, float radius, pz_tank_manager *tank_mgr)
{
    if (!mgr)
        return false;

    for (int i = 0; i < PZ_MAX_MINES; i++) {
        pz_mine *mine = &mgr->mines[i];
        if (!mine->active)
            continue;

        float dx = pos.x - mine->pos.x;
        float dz = pos.y - mine->pos.y;
        float dist = sqrtf(dx * dx + dz * dz);

        if (dist < MINE_HIT_RADIUS + radius) {
            mine_explode(mgr, mine, tank_mgr);
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Rendering
 * ============================================================================
 */

void
pz_mine_render(pz_mine_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection, const pz_mine_render_params *params)
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
    pz_renderer_set_uniform_vec2(
        renderer, mgr->shader, "u_shadow_params", (pz_vec2) { 0.0f, 0.0f });

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

    for (int i = 0; i < PZ_MAX_MINES; i++) {
        pz_mine *mine = &mgr->mines[i];
        if (!mine->active)
            continue;

        // Calculate bob offset
        float bob = sinf(mgr->time * MINE_BOB_SPEED + mine->bob_offset)
            * MINE_BOB_AMPLITUDE;

        // Build model matrix
        pz_mat4 model = pz_mat4_identity();
        model = pz_mat4_mul(model,
            pz_mat4_translate((pz_vec3) {
                mine->pos.x, MINE_BASE_HEIGHT + bob, mine->pos.y }));
        model = pz_mat4_mul(model,
            pz_mat4_scale((pz_vec3) { MINE_SCALE, MINE_SCALE, MINE_SCALE }));

        pz_mat4 mvp = pz_mat4_mul(*view_projection, model);

        // Set per-mine uniforms
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_mvp", &mvp);
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_model", &model);

        // Mine color: bright yellow/green blob
        pz_vec4 color;
        if (mine->arm_timer > 0.0f) {
            // Flashing while arming
            float flash = sinf(mgr->time * 15.0f) * 0.5f + 0.5f;
            color = pz_vec4_new(
                0.8f + flash * 0.2f, 0.7f + flash * 0.2f, 0.1f, 1.0f);
        } else {
            // Armed: bright yellow-green
            color = pz_vec4_new(0.9f, 0.85f, 0.2f, 1.0f);
        }
        pz_renderer_set_uniform_vec4(renderer, mgr->shader, "u_color", color);

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
 * Queries
 * ============================================================================
 */

int
pz_mine_count(const pz_mine_manager *mgr)
{
    return mgr ? mgr->active_count : 0;
}

int
pz_mine_get_explosions(const pz_mine_manager *mgr,
    pz_mine_explosion *explosions, int max_explosions)
{
    if (!mgr || !explosions)
        return 0;

    int count = mgr->explosion_count < max_explosions ? mgr->explosion_count
                                                      : max_explosions;
    memcpy(explosions, mgr->explosions, count * sizeof(pz_mine_explosion));
    return count;
}

void
pz_mine_clear_all(pz_mine_manager *mgr)
{
    if (!mgr)
        return;

    for (int i = 0; i < PZ_MAX_MINES; i++) {
        mgr->mines[i].active = false;
    }
    mgr->active_count = 0;
    mgr->explosion_count = 0;
}
