/*
 * Tank Game - Particle System Implementation
 *
 * Cel-shaded smoke effects with Wind Waker-style aesthetics.
 */

#include "pz_particle.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../core/pz_log.h"
#include "../core/pz_mem.h"

/* ============================================================================
 * Default Configurations
 * ============================================================================
 */

const pz_smoke_config PZ_SMOKE_BULLET_IMPACT = {
    .position = { 0.0f, 0.0f, 0.0f },
    .count = 6,
    .spread = 0.5f,
    .scale_min = 1.0f,
    .scale_max = 1.75f,
    .lifetime_min = 0.5f,
    .lifetime_max = 0.8f,
    .velocity_up = 1.5f,
    .velocity_spread = 1.0f,
};

const pz_smoke_config PZ_SMOKE_TANK_HIT = {
    .position = { 0.0f, 0.0f, 0.0f },
    .count = 10,
    .spread = 0.75f,
    .scale_min = 1.25f,
    .scale_max = 2.25f,
    .lifetime_min = 0.6f,
    .lifetime_max = 1.0f,
    .velocity_up = 2.0f,
    .velocity_spread = 1.5f,
};

const pz_smoke_config PZ_SMOKE_TANK_EXPLOSION = {
    .position = { 0.0f, 0.0f, 0.0f },
    .count = 20,
    .spread = 1.5f,
    .scale_min = 2.0f,
    .scale_max = 4.0f,
    .lifetime_min = 0.8f,
    .lifetime_max = 1.5f,
    .velocity_up = 3.5f,
    .velocity_spread = 3.0f,
};

/* ============================================================================
 * Helper Functions
 * ============================================================================
 */

// Simple random float [0, 1]
static float
randf(void)
{
    return (float)rand() / (float)RAND_MAX;
}

// Random float in range [min, max]
static float
randf_range(float min, float max)
{
    return min + randf() * (max - min);
}

// Check if a point is inside a circle
static bool
point_in_circle(float px, float py, float cx, float cy, float r)
{
    float dx = px - cx;
    float dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

// Distance from point to a spiral curve
// Returns distance to nearest point on an Archimedean spiral
static float
distance_to_spiral(float px, float py, float cx, float cy, float a, float b,
    float start_angle, float turns)
{
    // Archimedean spiral: r = a + b*theta
    // We sample the spiral and find minimum distance
    float min_dist = 1e10f;
    float end_angle = start_angle + turns * 2.0f * PZ_PI;

    for (float theta = start_angle; theta < end_angle; theta += 0.1f) {
        float r = a + b * (theta - start_angle);
        float sx = cx + r * cosf(theta);
        float sy = cy + r * sinf(theta);

        float dx = px - sx;
        float dy = py - sy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < min_dist) {
            min_dist = dist;
        }
    }

    return min_dist;
}

// Generate Wind Waker style cloud/smoke texture
// Features: bumpy cloud outline + inner spiral swirl
static uint8_t *
generate_smoke_texture(int size)
{
    uint8_t *data = pz_alloc(size * size * 4);
    memset(data, 0, size * size * 4);

    float cx = size / 2.0f;
    float cy = size / 2.0f;
    float base_radius = size * 0.38f;

    // Cloud puff positions - creates the bumpy outline
    // These are relative offsets from center, creating overlapping circles
    typedef struct {
        float ox, oy, r;
    } puff;

    puff puffs[] = {
        { 0.0f, 0.0f, 0.65f }, // Center large
        { -0.35f, 0.0f, 0.45f }, // Left
        { 0.35f, 0.05f, 0.45f }, // Right
        { 0.0f, -0.35f, 0.42f }, // Bottom
        { 0.0f, 0.38f, 0.40f }, // Top
        { -0.25f, -0.25f, 0.35f }, // Bottom-left
        { 0.25f, -0.25f, 0.35f }, // Bottom-right
        { -0.22f, 0.28f, 0.32f }, // Top-left
        { 0.25f, 0.28f, 0.32f }, // Top-right
    };
    int num_puffs = sizeof(puffs) / sizeof(puffs[0]);

    // First pass: determine if each pixel is inside the cloud shape
    float *cloud_mask = pz_calloc(size * size, sizeof(float));
    float *edge_dist = pz_calloc(size * size, sizeof(float));

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float px = (float)x;
            float py = (float)y;

            // Check all puffs - pixel is inside if inside any puff
            float max_inside = 0.0f;
            for (int p = 0; p < num_puffs; p++) {
                float puff_cx = cx + puffs[p].ox * base_radius;
                float puff_cy = cy + puffs[p].oy * base_radius;
                float puff_r = puffs[p].r * base_radius;

                float dx = px - puff_cx;
                float dy = py - puff_cy;
                float dist = sqrtf(dx * dx + dy * dy);

                // How far inside this puff (1.0 at center, 0.0 at edge)
                float inside = 1.0f - (dist / puff_r);
                if (inside > max_inside) {
                    max_inside = inside;
                }
            }

            cloud_mask[y * size + x] = max_inside;

            // Calculate distance to edge (for outline)
            // Approximate by checking distance to nearest puff edge
            float min_edge_dist = 1e10f;
            for (int p = 0; p < num_puffs; p++) {
                float puff_cx = cx + puffs[p].ox * base_radius;
                float puff_cy = cy + puffs[p].oy * base_radius;
                float puff_r = puffs[p].r * base_radius;

                float dx = px - puff_cx;
                float dy = py - puff_cy;
                float dist_to_edge = fabsf(sqrtf(dx * dx + dy * dy) - puff_r);
                if (dist_to_edge < min_edge_dist) {
                    min_edge_dist = dist_to_edge;
                }
            }
            edge_dist[y * size + x] = min_edge_dist;
        }
    }

    // Spiral parameters - positioned slightly off-center for that WW look
    float spiral_cx = cx + base_radius * 0.05f;
    float spiral_cy = cy + base_radius * 0.05f;
    float spiral_a = base_radius * 0.08f; // Starting radius
    float spiral_b = base_radius * 0.06f; // Growth rate
    float spiral_start = 0.5f; // Starting angle
    float spiral_turns = 1.8f; // Number of turns
    float spiral_width = size * 0.045f; // Line thickness

    // Second pass: render with cel-shading
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            float inside = cloud_mask[y * size + x];
            float dist_edge = edge_dist[y * size + x];

            if (inside <= 0.0f) {
                // Outside cloud
                data[idx + 0] = 0;
                data[idx + 1] = 0;
                data[idx + 2] = 0;
                data[idx + 3] = 0;
                continue;
            }

            float px = (float)x;
            float py = (float)y;

            // Check distance to spiral
            float spiral_dist = distance_to_spiral(px, py, spiral_cx, spiral_cy,
                spiral_a, spiral_b, spiral_start, spiral_turns);

            // Determine brightness based on:
            // 1. Distance to edge (outline)
            // 2. Distance to spiral (inner detail)

            float brightness = 1.0f; // Default: bright fill
            float outline_width = size * 0.04f;

            // Outer dark outline
            if (inside < 0.15f || dist_edge < outline_width) {
                // Near edge - dark outline
                float edge_t = dist_edge / outline_width;
                if (edge_t < 0.5f) {
                    brightness = 0.15f; // Dark outline
                } else if (edge_t < 1.0f) {
                    brightness = 0.4f; // Transition
                }
            }

            // Spiral swirl (dark line in center area)
            if (spiral_dist < spiral_width && inside > 0.2f) {
                float spiral_t = spiral_dist / spiral_width;
                if (spiral_t < 0.5f) {
                    brightness = 0.2f; // Dark spiral core
                } else if (spiral_t < 0.8f) {
                    brightness = 0.45f; // Spiral edge
                } else {
                    // Blend with current brightness
                    brightness = brightness * 0.7f + 0.3f * 0.6f;
                }
            }

            // Alpha: solid inside, soft at very edge
            float alpha = 1.0f;
            if (inside < 0.1f) {
                alpha = inside / 0.1f;
            }

            uint8_t b = (uint8_t)(brightness * 255);
            uint8_t a = (uint8_t)(alpha * 255);

            data[idx + 0] = b;
            data[idx + 1] = b;
            data[idx + 2] = b;
            data[idx + 3] = a;
        }
    }

    pz_free(cloud_mask);
    pz_free(edge_dist);

    return data;
}

/* ============================================================================
 * Manager Lifecycle
 * ============================================================================
 */

pz_particle_manager *
pz_particle_manager_create(pz_renderer *renderer)
{
    pz_particle_manager *mgr = pz_calloc(1, sizeof(pz_particle_manager));

    // Generate procedural smoke texture
    int tex_size = 128; // Higher res for better quality
    uint8_t *tex_data = generate_smoke_texture(tex_size);

    pz_texture_desc tex_desc = {
        .width = tex_size,
        .height = tex_size,
        .format = PZ_TEXTURE_RGBA8,
        .filter = PZ_FILTER_LINEAR,
        .wrap = PZ_WRAP_CLAMP,
    };

    mgr->smoke_texture = pz_renderer_create_texture(renderer, &tex_desc);
    if (mgr->smoke_texture != PZ_INVALID_HANDLE) {
        pz_renderer_update_texture(
            renderer, mgr->smoke_texture, 0, 0, tex_size, tex_size, tex_data);
    }
    pz_free(tex_data);

    // Create quad buffer for billboards
    // Simple quad: position (3) + texcoord (2) = 5 floats per vertex, 6 verts
    float quad_verts[] = {
        // Position           TexCoord
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, // Bottom-left
        0.5f, -0.5f, 0.0f, 1.0f, 1.0f, // Bottom-right
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f, // Top-right
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, // Bottom-left
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f, // Top-right
        -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, // Top-left
    };

    pz_buffer_desc buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STATIC,
        .data = quad_verts,
        .size = sizeof(quad_verts),
    };
    mgr->quad_buffer = pz_renderer_create_buffer(renderer, &buf_desc);

    // Load shader
    mgr->shader = pz_renderer_load_shader(
        renderer, "shaders/particle.vert", "shaders/particle.frag", "particle");

    if (mgr->shader != PZ_INVALID_HANDLE) {
        // Vertex layout: position (3) + texcoord (2)
        pz_vertex_attr particle_attrs[] = {
            { .name = "a_position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
            { .name = "a_texcoord",
                .type = PZ_ATTR_FLOAT2,
                .offset = 3 * sizeof(float) },
        };

        pz_pipeline_desc desc = {
            .shader = mgr->shader,
            .vertex_layout = { .attrs = particle_attrs,
                .attr_count = 2,
                .stride = 5 * sizeof(float) },
            .blend = PZ_BLEND_ALPHA, // Use standard alpha blending
            .depth = PZ_DEPTH_READ, // Read depth but don't write
            .cull = PZ_CULL_NONE, // Billboards face camera
            .primitive = PZ_PRIMITIVE_TRIANGLES,
        };
        mgr->pipeline = pz_renderer_create_pipeline(renderer, &desc);
        mgr->render_ready = (mgr->pipeline != PZ_INVALID_HANDLE);
    }

    if (!mgr->render_ready) {
        pz_log(PZ_LOG_WARN, PZ_LOG_CAT_GAME,
            "Particle rendering not available (shader/pipeline failed)");
    }

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Particle manager created");
    return mgr;
}

void
pz_particle_manager_destroy(pz_particle_manager *mgr, pz_renderer *renderer)
{
    if (!mgr)
        return;

    if (mgr->pipeline != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_pipeline(renderer, mgr->pipeline);
    }
    if (mgr->shader != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_shader(renderer, mgr->shader);
    }
    if (mgr->smoke_texture != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_texture(renderer, mgr->smoke_texture);
    }
    if (mgr->quad_buffer != PZ_INVALID_HANDLE) {
        pz_renderer_destroy_buffer(renderer, mgr->quad_buffer);
    }

    pz_free(mgr);
    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_GAME, "Particle manager destroyed");
}

/* ============================================================================
 * Particle Spawning
 * ============================================================================
 */

void
pz_particle_spawn(pz_particle_manager *mgr, const pz_particle *template)
{
    if (!mgr || !template)
        return;

    // Find free slot
    for (int i = 0; i < PZ_MAX_PARTICLES; i++) {
        if (!mgr->particles[i].active) {
            mgr->particles[i] = *template;
            mgr->particles[i].active = true;
            mgr->active_count++;
            return;
        }
    }

    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_GAME, "No free particle slots");
}

void
pz_particle_spawn_smoke(pz_particle_manager *mgr, const pz_smoke_config *config)
{
    if (!mgr || !config)
        return;

    // Blue-gray smoke colors (Wind Waker style)
    // Varying shades for each particle
    pz_vec3 base_colors[] = {
        { 0.69f, 0.77f, 0.87f }, // Light steel blue
        { 0.60f, 0.68f, 0.78f }, // Lighter blue-gray
        { 0.53f, 0.60f, 0.70f }, // Medium blue-gray
        { 0.47f, 0.53f, 0.60f }, // Slate gray
    };

    for (int i = 0; i < config->count; i++) {
        pz_particle p = { 0 };
        p.type = PZ_PARTICLE_SMOKE;

        // Random offset from center
        float ox = randf_range(-config->spread, config->spread);
        float oy = randf_range(0.0f, config->spread * 0.5f);
        float oz = randf_range(-config->spread, config->spread);

        p.pos.x = config->position.x + ox;
        p.pos.y = config->position.y + oy;
        p.pos.z = config->position.z + oz;

        // Velocity: upward with some horizontal spread
        p.velocity.x
            = randf_range(-config->velocity_spread, config->velocity_spread);
        p.velocity.y = config->velocity_up * randf_range(0.7f, 1.3f);
        p.velocity.z
            = randf_range(-config->velocity_spread, config->velocity_spread);

        // Random rotation
        p.rotation = randf() * 2.0f * PZ_PI;
        p.rotation_speed = randf_range(-2.0f, 2.0f);

        // Scale animation: start small, grow, then shrink slightly
        float base_scale = randf_range(config->scale_min, config->scale_max);
        p.scale_start = base_scale * 0.3f;
        p.scale_end = base_scale * 1.4f;
        p.scale = p.scale_start;

        // Alpha: fade out
        p.alpha_start = 0.85f;
        p.alpha_end = 0.0f;
        p.alpha = p.alpha_start;

        // Random color from palette
        p.color = base_colors[i % 4];

        // Lifetime
        p.lifetime = randf_range(config->lifetime_min, config->lifetime_max);
        p.age = 0.0f;

        // Random variant (for future sprite variation)
        p.variant = rand() % 4;

        pz_particle_spawn(mgr, &p);
    }
}

void
pz_particle_spawn_fog(
    pz_particle_manager *mgr, pz_vec3 position, float idle_factor)
{
    if (!mgr)
        return;

    idle_factor = pz_clampf(idle_factor, 0.0f, 1.0f);

    pz_vec3 base_colors[] = {
        { 0.68f, 0.70f, 0.72f },
        { 0.60f, 0.62f, 0.65f },
        { 0.54f, 0.56f, 0.60f },
        { 0.48f, 0.50f, 0.54f },
    };

    pz_particle p = { 0 };
    p.type = PZ_PARTICLE_FOG;

    // Subtle spread around the trail position
    p.pos.x = position.x + randf_range(-0.25f, 0.25f);
    p.pos.y = position.y + randf_range(0.0f, 0.2f);
    p.pos.z = position.z + randf_range(-0.25f, 0.25f);

    // Gentle drift, mostly upward
    p.velocity.x = randf_range(-0.15f, 0.15f);
    p.velocity.y = randf_range(0.08f, 0.25f);
    p.velocity.z = randf_range(-0.15f, 0.15f);

    // Soft rotation
    p.rotation = randf() * 2.0f * PZ_PI;
    p.rotation_speed = randf_range(-0.6f, 0.6f);

    float scale_bias = pz_lerpf(0.9f, 1.25f, idle_factor);
    float base_scale = randf_range(1.08f, 1.68f) * scale_bias; // 20% bigger
    p.scale_start = base_scale * 0.5f;
    p.scale_end
        = base_scale * pz_lerpf(1.6f, 2.4f, idle_factor); // scales up more
    p.scale = p.scale_start;

    p.alpha_start = pz_lerpf(0.35f, 0.65f, idle_factor);
    p.alpha_end = 0.0f;
    p.alpha = p.alpha_start;

    p.color = base_colors[rand() % 4];

    float lifetime
        = pz_lerpf(1.1f, 3.4f, idle_factor) + randf_range(-0.2f, 0.2f);
    p.lifetime = pz_minf(pz_maxf(lifetime, 0.6f), 4.0f);
    p.age = 0.0f;

    p.variant = rand() % 4;

    pz_particle_spawn(mgr, &p);
}

/* ============================================================================
 * Update
 * ============================================================================
 */

void
pz_particle_update(pz_particle_manager *mgr, float dt)
{
    if (!mgr)
        return;

    for (int i = 0; i < PZ_MAX_PARTICLES; i++) {
        pz_particle *p = &mgr->particles[i];
        if (!p->active)
            continue;

        p->age += dt;

        // Check lifetime
        if (p->age >= p->lifetime) {
            p->active = false;
            mgr->active_count--;
            continue;
        }

        float t = p->age / p->lifetime;

        // Update position
        p->pos.x += p->velocity.x * dt;
        p->pos.y += p->velocity.y * dt;
        p->pos.z += p->velocity.z * dt;

        // Slow down horizontal velocity
        p->velocity.x *= (1.0f - dt * 2.0f);
        p->velocity.z *= (1.0f - dt * 2.0f);

        // Reduce upward velocity over time
        p->velocity.y *= (1.0f - dt * 1.5f);

        // Update rotation
        p->rotation += p->rotation_speed * dt;

        // Animate scale: ease-out growth
        // Quick growth in first 30%, then slower expansion
        float scale_t;
        if (t < 0.3f) {
            scale_t = t / 0.3f;
            scale_t = 1.0f - (1.0f - scale_t) * (1.0f - scale_t); // ease-out
            scale_t *= 0.7f; // Scale to 70% at t=0.3
        } else {
            // Linear growth from 70% to 100%
            scale_t = 0.7f + (t - 0.3f) / 0.7f * 0.3f;
        }
        p->scale = p->scale_start + (p->scale_end - p->scale_start) * scale_t;

        // Animate alpha: hold steady then fade out
        if (t < 0.4f) {
            p->alpha = p->alpha_start;
        } else {
            float fade_t = (t - 0.4f) / 0.6f;
            fade_t = fade_t * fade_t; // ease-in (accelerating fade)
            p->alpha
                = p->alpha_start + (p->alpha_end - p->alpha_start) * fade_t;
        }
    }
}

/* ============================================================================
 * Rendering
 * ============================================================================
 */

void
pz_particle_render(pz_particle_manager *mgr, pz_renderer *renderer,
    const pz_mat4 *view_projection, pz_vec3 camera_right, pz_vec3 camera_up)
{
    if (!mgr || !renderer || !view_projection)
        return;

    if (!mgr->render_ready || mgr->active_count == 0)
        return;

    // Bind texture once
    pz_renderer_bind_texture(renderer, 0, mgr->smoke_texture);
    pz_renderer_set_uniform_int(renderer, mgr->shader, "u_texture", 0);

    // Sort particles by distance for proper blending?
    // For now, render in order (smoke is fairly forgiving)

    for (int i = 0; i < PZ_MAX_PARTICLES; i++) {
        pz_particle *p = &mgr->particles[i];
        if (!p->active)
            continue;

        // Build billboard model matrix
        // Billboard faces camera using provided right/up vectors
        pz_vec3 right = pz_vec3_scale(camera_right, p->scale);
        pz_vec3 up = pz_vec3_scale(camera_up, p->scale);
        pz_vec3 forward = pz_vec3_cross(camera_right, camera_up);

        // Apply rotation around the forward (view) direction
        float c = cosf(p->rotation);
        float s = sinf(p->rotation);
        pz_vec3 rotated_right
            = pz_vec3_add(pz_vec3_scale(right, c), pz_vec3_scale(up, s));
        pz_vec3 rotated_up
            = pz_vec3_add(pz_vec3_scale(right, -s), pz_vec3_scale(up, c));

        // Build final billboard matrix (column-major order)
        // Column 0: X axis (right)
        // Column 1: Y axis (up)
        // Column 2: Z axis (forward)
        // Column 3: translation
        pz_mat4 billboard;
        billboard.m[0] = rotated_right.x;
        billboard.m[1] = rotated_right.y;
        billboard.m[2] = rotated_right.z;
        billboard.m[3] = 0.0f;
        billboard.m[4] = rotated_up.x;
        billboard.m[5] = rotated_up.y;
        billboard.m[6] = rotated_up.z;
        billboard.m[7] = 0.0f;
        billboard.m[8] = forward.x;
        billboard.m[9] = forward.y;
        billboard.m[10] = forward.z;
        billboard.m[11] = 0.0f;
        billboard.m[12] = p->pos.x;
        billboard.m[13] = p->pos.y;
        billboard.m[14] = p->pos.z;
        billboard.m[15] = 1.0f;

        pz_mat4 mvp = pz_mat4_mul(*view_projection, billboard);

        // Set uniforms
        pz_renderer_set_uniform_mat4(renderer, mgr->shader, "u_mvp", &mvp);
        pz_renderer_set_uniform_float(
            renderer, mgr->shader, "u_alpha", p->alpha);
        pz_renderer_set_uniform_vec3(
            renderer, mgr->shader, "u_color", p->color);

        // Draw quad
        pz_draw_cmd cmd = {
            .pipeline = mgr->pipeline,
            .vertex_buffer = mgr->quad_buffer,
            .index_buffer = PZ_INVALID_HANDLE,
            .vertex_count = 6,
            .index_count = 0,
            .vertex_offset = 0,
            .index_offset = 0,
        };
        pz_renderer_draw(renderer, &cmd);
    }
}

int
pz_particle_count(const pz_particle_manager *mgr)
{
    return mgr ? mgr->active_count : 0;
}

void
pz_particle_clear(pz_particle_manager *mgr)
{
    if (!mgr) {
        return;
    }

    for (int i = 0; i < PZ_MAX_PARTICLES; i++) {
        mgr->particles[i].active = false;
    }
    mgr->active_count = 0;
}
