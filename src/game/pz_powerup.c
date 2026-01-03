/*
 * Tank Game - Powerup System Implementation
 */

#include "pz_powerup.h"
#include "pz_tank.h"

#include <math.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

/* ============================================================================
 * Constants
 * ============================================================================
 */

// Powerup collision radius for tank pickup
static const float POWERUP_RADIUS = 0.6f;

// Animation parameters
static const float BOB_AMPLITUDE = 0.15f; // How much it bobs up and down
static const float BOB_SPEED = 3.0f; // Bob frequency
static const float ROTATION_SPEED = 2.0f; // Rotation speed (rad/s)

// Height above ground
static const float POWERUP_HEIGHT = 0.8f;

/* ============================================================================
 * Weapon Stats Definitions
 * ============================================================================
 */

// Default weapon (normal tank cannon)
// Fire cooldown: 0.325s (30% slower than base 0.25s)
static const pz_weapon_stats WEAPON_DEFAULT = {
    .fire_cooldown = 0.325f, // 30% slower than 0.25s
    .projectile_speed = 11.25f, // Same as before
    .damage = 5, // 2 hits to kill
    .max_bounces = 1,
    .projectile_scale = 1.0f,
    .projectile_color = { 1.0f, 0.8f, 0.2f, 1.0f }, // Yellow/orange
    .auto_fire = false, // Must click for each shot
    .max_active_projectiles = 8, // Max 8 bullets in flight
};

// Machine gun - fires twice as fast, 1 damage, smaller darker bullets, no
// bounce
static const pz_weapon_stats WEAPON_MACHINE_GUN = {
    .fire_cooldown = 0.1625f, // Half of default (twice as fast)
    .projectile_speed = 14.0f, // Slightly faster
    .damage = 1, // 10 hits to kill
    .max_bounces = 0, // No bouncing
    .projectile_scale = 0.4f, // Much smaller
    .projectile_color = { 0.3f, 0.25f, 0.2f, 1.0f }, // Dark brown/gray
    .auto_fire = true, // Hold to spray
    .max_active_projectiles = 12, // Max 12 bullets in flight
};

// Ricochet - bounces twice, green bullets, slightly faster
static const pz_weapon_stats WEAPON_RICOCHET = {
    .fire_cooldown = 0.3f, // Slightly faster than default (0.325s)
    .projectile_speed = 12.5f, // Slightly faster than default (11.25)
    .damage = 5, // Same as default
    .max_bounces = 2, // Bounces twice
    .projectile_scale = 1.0f, // Normal size
    .projectile_color = { 0.2f, 0.9f, 0.3f, 1.0f }, // Green
    .auto_fire = false, // Must click for each shot
    .max_active_projectiles = 6, // Max 6 bullets in flight
};

const pz_weapon_stats *
pz_weapon_get_stats(pz_powerup_type weapon)
{
    switch (weapon) {
    case PZ_POWERUP_MACHINE_GUN:
        return &WEAPON_MACHINE_GUN;
    case PZ_POWERUP_RICOCHET:
        return &WEAPON_RICOCHET;
    case PZ_POWERUP_NONE:
    default:
        return &WEAPON_DEFAULT;
    }
}

/* ============================================================================
 * Manager Lifecycle
 * ============================================================================
 */

pz_powerup_manager *
pz_powerup_manager_create(pz_renderer *renderer)
{
    pz_powerup_manager *mgr = pz_calloc(1, sizeof(pz_powerup_manager));

    // Create powerup mesh (a floating box/crate shape)
    mgr->mesh = pz_mesh_create_powerup();
    if (mgr->mesh) {
        pz_mesh_upload(mgr->mesh, renderer);
    }

    // Load shader (reuse entity shader)
    mgr->shader = pz_renderer_load_shader(
        renderer, "shaders/entity.vert", "shaders/entity.frag", "powerup");

    if (mgr->shader != PZ_INVALID_HANDLE) {
        // Create transparent pipeline with alpha blending
        pz_pipeline_desc desc = {
            .shader = mgr->shader,
            .vertex_layout = pz_mesh_get_vertex_layout(),
            .blend = PZ_BLEND_ALPHA,
            .depth = PZ_DEPTH_READ_WRITE,
            .cull = PZ_CULL_BACK,
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        mgr->pipeline = pz_renderer_create_pipeline(renderer, &desc);
        mgr->pipeline_transparent = mgr->pipeline; // Same pipeline, uses alpha
        mgr->render_ready = (mgr->pipeline != PZ_INVALID_HANDLE);
    }

    mgr->time = 0.0f;

    if (!mgr->render_ready) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Powerup rendering not available (shader/pipeline failed)");
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Powerup manager created");
    return mgr;
}

void
pz_powerup_manager_destroy(pz_powerup_manager *mgr, pz_renderer *renderer)
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
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Powerup manager destroyed");
}

/* ============================================================================
 * Powerup Spawning
 * ============================================================================
 */

int
pz_powerup_add(pz_powerup_manager *mgr, pz_vec2 pos, pz_powerup_type type,
    float respawn_time)
{
    if (!mgr)
        return -1;

    // Find free slot
    int slot = -1;
    for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
        if (!mgr->powerups[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME, "No free powerup slots (max=%d)",
            PZ_MAX_POWERUPS);
        return -1;
    }

    pz_powerup *powerup = &mgr->powerups[slot];
    memset(powerup, 0, sizeof(pz_powerup));

    powerup->active = true;
    powerup->collected = false;
    powerup->type = type;
    powerup->pos = pos;
    powerup->bob_offset
        = (float)slot * 0.5f; // Offset animation phase per powerup
    powerup->rotation = 0.0f;
    powerup->respawn_time = respawn_time;
    powerup->respawn_timer = 0.0f;

    mgr->active_count++;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME,
        "Powerup spawned at (%.2f, %.2f), type=%s, respawn=%.1fs", pos.x, pos.y,
        pz_powerup_type_name(type), respawn_time);

    return slot;
}

/* ============================================================================
 * Powerup Update
 * ============================================================================
 */

void
pz_powerup_update(pz_powerup_manager *mgr, float dt)
{
    if (!mgr)
        return;

    // Update global time for flicker effects
    mgr->time += dt;

    for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
        pz_powerup *powerup = &mgr->powerups[i];
        if (!powerup->active)
            continue;

        // Update animation
        powerup->bob_offset += dt * BOB_SPEED;
        powerup->rotation += dt * ROTATION_SPEED;

        // Handle respawn timer
        if (powerup->collected) {
            powerup->respawn_timer -= dt;
            if (powerup->respawn_timer <= 0.0f) {
                powerup->collected = false;
                powerup->respawn_timer = 0.0f;
                pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME,
                    "Powerup respawned at (%.2f, %.2f)", powerup->pos.x,
                    powerup->pos.y);
            }
        }
    }
}

/* ============================================================================
 * Powerup Collection
 * ============================================================================
 */

pz_powerup_type
pz_powerup_check_collection(
    pz_powerup_manager *mgr, pz_vec2 tank_pos, float tank_radius)
{
    if (!mgr)
        return PZ_POWERUP_NONE;

    for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
        pz_powerup *powerup = &mgr->powerups[i];

        // Skip inactive or already collected powerups
        if (!powerup->active || powerup->collected)
            continue;

        // Circle-circle collision
        float dist = pz_vec2_dist(tank_pos, powerup->pos);
        float combined_radius = tank_radius + POWERUP_RADIUS;

        if (dist < combined_radius) {
            // Collected!
            powerup->collected = true;
            powerup->respawn_timer = powerup->respawn_time;

            pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Powerup collected: %s",
                pz_powerup_type_name(powerup->type));

            return powerup->type;
        }
    }

    return PZ_POWERUP_NONE;
}

/* ============================================================================
 * Powerup Rendering
 * ============================================================================
 */

void
pz_powerup_render(pz_powerup_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection)
{
    if (!mgr || !renderer || !view_projection)
        return;

    if (!mgr->render_ready)
        return;

    // Count visible powerups
    int visible = 0;
    for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
        if (mgr->powerups[i].active && !mgr->powerups[i].collected) {
            visible++;
        }
    }
    if (visible == 0)
        return;

    // Light parameters (same as entity rendering)
    pz_vec3 light_dir = { 0.5f, 1.0f, 0.3f };
    pz_vec3 light_color = { 0.8f, 0.75f, 0.7f };
    pz_vec3 ambient = { 0.3f, 0.35f, 0.4f };

    // Set shared uniforms
    pz_renderer_set_uniform_vec3(
        renderer, mgr->shader, "u_light_dir", light_dir);
    pz_renderer_set_uniform_vec3(
        renderer, mgr->shader, "u_light_color", light_color);
    pz_renderer_set_uniform_vec3(renderer, mgr->shader, "u_ambient", ambient);

    for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
        pz_powerup *powerup = &mgr->powerups[i];

        // Skip inactive or collected powerups
        if (!powerup->active || powerup->collected)
            continue;

        // Calculate height with bobbing animation
        float bob = sinf(powerup->bob_offset) * BOB_AMPLITUDE;
        float height = POWERUP_HEIGHT + bob;

        // Color matches the weapon's projectile color
        const pz_weapon_stats *stats = pz_weapon_get_stats(powerup->type);
        pz_vec4 color = stats->projectile_color;

        // Animated transparency (10% to 30% translucent = 70% to 90% alpha)
        float alpha = pz_powerup_get_alpha(mgr, i);
        color.w = alpha;

        // Build model matrix
        pz_mat4 model = pz_mat4_identity();
        model = pz_mat4_mul(model,
            pz_mat4_translate(
                (pz_vec3) { powerup->pos.x, height, powerup->pos.y }));
        model = pz_mat4_mul(model, pz_mat4_rotate_y(powerup->rotation));

        pz_mat4 mvp = pz_mat4_mul(*view_projection, model);

        // Set per-powerup uniforms
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_mvp", &mvp);
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_model", &model);
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
 * Utility
 * ============================================================================
 */

int
pz_powerup_count(const pz_powerup_manager *mgr)
{
    if (!mgr)
        return 0;

    int count = 0;
    for (int i = 0; i < PZ_MAX_POWERUPS; i++) {
        if (mgr->powerups[i].active && !mgr->powerups[i].collected) {
            count++;
        }
    }
    return count;
}

const char *
pz_powerup_type_name(pz_powerup_type type)
{
    switch (type) {
    case PZ_POWERUP_MACHINE_GUN:
        return "Machine Gun";
    case PZ_POWERUP_RICOCHET:
        return "Ricochet";
    case PZ_POWERUP_NONE:
    default:
        return "None";
    }
}

pz_powerup_type
pz_powerup_type_from_name(const char *name)
{
    if (!name) {
        return PZ_POWERUP_NONE;
    }
    if (strcmp(name, "machine_gun") == 0) {
        return PZ_POWERUP_MACHINE_GUN;
    }
    if (strcmp(name, "ricochet") == 0) {
        return PZ_POWERUP_RICOCHET;
    }
    return PZ_POWERUP_NONE;
}

float
pz_powerup_get_flicker(const pz_powerup_manager *mgr, int index)
{
    if (!mgr || index < 0 || index >= PZ_MAX_POWERUPS) {
        return 1.0f;
    }

    const pz_powerup *powerup = &mgr->powerups[index];
    if (!powerup->active || powerup->collected) {
        return 0.0f;
    }

    // Multi-frequency flicker for organic feel
    // Use powerup index as phase offset so each one flickers differently
    float phase = (float)index * 1.7f;
    float t = mgr->time;

    // Combine multiple sine waves for complex flicker
    float flicker = 0.7f // Base intensity
        + 0.15f * sinf(t * 4.0f + phase) // Slow pulse
        + 0.10f * sinf(t * 9.0f + phase * 2.0f) // Medium flicker
        + 0.05f * sinf(t * 17.0f + phase * 3.0f); // Fast shimmer

    // Clamp to reasonable range (0.5 to 1.0)
    if (flicker < 0.5f)
        flicker = 0.5f;
    if (flicker > 1.0f)
        flicker = 1.0f;

    return flicker;
}

float
pz_powerup_get_alpha(const pz_powerup_manager *mgr, int index)
{
    if (!mgr || index < 0 || index >= PZ_MAX_POWERUPS) {
        return 1.0f;
    }

    const pz_powerup *powerup = &mgr->powerups[index];
    if (!powerup->active || powerup->collected) {
        return 0.0f;
    }

    // Animate between 70% and 90% alpha (10% to 30% translucent)
    float phase = (float)index * 2.3f;
    float t = mgr->time;

    // Smooth sine wave animation
    float alpha_factor = 0.8f + 0.1f * sinf(t * 2.5f + phase);

    return alpha_factor;
}
