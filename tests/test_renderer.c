/*
 * Tank Game - Renderer Tests
 *
 * Tests for the renderer abstraction using the null backend.
 */

#include "../src/core/pz_log.h"
#include "../src/core/pz_mem.h"
#include "../src/engine/render/pz_render_backend.h"
#include "../src/engine/render/pz_renderer.h"
#include "test_framework.h"

/* Stub for GL33 backend - not used in tests */
const pz_render_backend_vtable *
pz_render_backend_gl33_vtable(void)
{
    return NULL;
}

/* ============================================================================
 * Helper to create a null renderer for testing
 * ============================================================================
 */

static pz_renderer *
create_test_renderer(void)
{
    pz_renderer_config config = {
        .backend = PZ_BACKEND_NULL,
        .window_handle = NULL,
        .viewport_width = 800,
        .viewport_height = 600,
    };
    return pz_renderer_create(&config);
}

/* ============================================================================
 * Tests
 * ============================================================================
 */

TEST(renderer_null_backend_init)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);
    ASSERT_EQ((int)PZ_BACKEND_NULL, (int)pz_renderer_get_backend(r));

    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_viewport)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    int w, h;
    pz_renderer_get_viewport(r, &w, &h);
    ASSERT_EQ(800, w);
    ASSERT_EQ(600, h);

    pz_renderer_set_viewport(r, 1024, 768);
    pz_renderer_get_viewport(r, &w, &h);
    ASSERT_EQ(1024, w);
    ASSERT_EQ(768, h);

    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_shader_lifecycle)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    pz_shader_desc desc = {
        .vertex_source = "void main() {}",
        .fragment_source = "void main() {}",
        .name = "test_shader",
    };

    pz_shader_handle shader = pz_renderer_create_shader(r, &desc);
    ASSERT(shader != PZ_INVALID_HANDLE);

    pz_renderer_destroy_shader(r, shader);
    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_texture_lifecycle)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    uint8_t pixels[4] = { 255, 0, 0, 255 }; // Red pixel

    pz_texture_desc desc = {
        .width = 1,
        .height = 1,
        .format = PZ_TEXTURE_RGBA8,
        .filter = PZ_FILTER_NEAREST,
        .wrap = PZ_WRAP_REPEAT,
        .data = pixels,
    };

    pz_texture_handle texture = pz_renderer_create_texture(r, &desc);
    ASSERT(texture != PZ_INVALID_HANDLE);

    pz_renderer_destroy_texture(r, texture);
    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_buffer_lifecycle)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    float vertices[] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f };

    pz_buffer_desc desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STATIC,
        .data = vertices,
        .size = sizeof(vertices),
    };

    pz_buffer_handle buffer = pz_renderer_create_buffer(r, &desc);
    ASSERT(buffer != PZ_INVALID_HANDLE);

    pz_renderer_destroy_buffer(r, buffer);
    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_pipeline_lifecycle)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    // First create a shader
    pz_shader_desc shader_desc = {
        .vertex_source = "void main() {}",
        .fragment_source = "void main() {}",
        .name = "pipeline_test_shader",
    };
    pz_shader_handle shader = pz_renderer_create_shader(r, &shader_desc);
    ASSERT(shader != PZ_INVALID_HANDLE);

    // Create pipeline
    pz_vertex_attr attrs[] = {
        { .name = "position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
    };

    pz_pipeline_desc desc = {
        .shader = shader,
        .vertex_layout =
            {
                .attrs = attrs,
                .attr_count = 1,
                .stride = 3 * sizeof(float),
            },
        .blend = PZ_BLEND_NONE,
        .depth = PZ_DEPTH_READ_WRITE,
        .cull = PZ_CULL_BACK,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };

    pz_pipeline_handle pipeline = pz_renderer_create_pipeline(r, &desc);
    ASSERT(pipeline != PZ_INVALID_HANDLE);

    pz_renderer_destroy_pipeline(r, pipeline);
    pz_renderer_destroy_shader(r, shader);
    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_render_target_lifecycle)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    pz_render_target_desc desc = {
        .width = 256,
        .height = 256,
        .color_format = PZ_TEXTURE_RGBA8,
        .has_depth = true,
    };

    pz_render_target_handle rt = pz_renderer_create_render_target(r, &desc);
    ASSERT(rt != PZ_INVALID_HANDLE);

    pz_texture_handle rt_tex = pz_renderer_get_render_target_texture(r, rt);
    ASSERT(rt_tex != PZ_INVALID_HANDLE);

    pz_renderer_destroy_render_target(r, rt);
    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_frame_operations)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    // These should all succeed with null backend
    pz_renderer_begin_frame(r);
    pz_renderer_set_render_target(r, 0);
    pz_renderer_clear(r, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f);
    pz_renderer_end_frame(r);

    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_uniforms)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    pz_shader_desc shader_desc = {
        .vertex_source = "void main() {}",
        .fragment_source = "void main() {}",
        .name = "uniform_test_shader",
    };
    pz_shader_handle shader = pz_renderer_create_shader(r, &shader_desc);

    // These should all work (no-op in null backend)
    pz_renderer_set_uniform_float(r, shader, "u_float", 1.0f);
    pz_renderer_set_uniform_vec2(r, shader, "u_vec2", (pz_vec2) { 1.0f, 2.0f });
    pz_renderer_set_uniform_vec3(
        r, shader, "u_vec3", (pz_vec3) { 1.0f, 2.0f, 3.0f });
    pz_renderer_set_uniform_vec4(
        r, shader, "u_vec4", (pz_vec4) { 1.0f, 2.0f, 3.0f, 4.0f });
    pz_mat4 mat = pz_mat4_identity();
    pz_renderer_set_uniform_mat4(r, shader, "u_mat4", &mat);
    pz_renderer_set_uniform_int(r, shader, "u_int", 42);

    pz_renderer_destroy_shader(r, shader);
    pz_renderer_destroy(r);
    pz_log_shutdown();
}

TEST(renderer_draw)
{
    pz_log_init();

    pz_renderer *r = create_test_renderer();
    ASSERT_NOT_NULL(r);

    // Create resources
    pz_shader_desc shader_desc = {
        .vertex_source = "void main() {}",
        .fragment_source = "void main() {}",
        .name = "draw_test_shader",
    };
    pz_shader_handle shader = pz_renderer_create_shader(r, &shader_desc);

    float vertices[] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f };
    pz_buffer_desc buf_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_STATIC,
        .data = vertices,
        .size = sizeof(vertices),
    };
    pz_buffer_handle vbo = pz_renderer_create_buffer(r, &buf_desc);

    pz_vertex_attr attrs[] = {
        { .name = "position", .type = PZ_ATTR_FLOAT3, .offset = 0 },
    };
    pz_pipeline_desc pipe_desc = {
        .shader = shader,
        .vertex_layout =
            {
                .attrs = attrs,
                .attr_count = 1,
                .stride = 3 * sizeof(float),
            },
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    pz_pipeline_handle pipeline = pz_renderer_create_pipeline(r, &pipe_desc);

    // Draw
    pz_draw_cmd cmd = {
        .pipeline = pipeline,
        .vertex_buffer = vbo,
        .vertex_count = 3,
    };
    pz_renderer_draw(r, &cmd);

    // Cleanup
    pz_renderer_destroy_pipeline(r, pipeline);
    pz_renderer_destroy_buffer(r, vbo);
    pz_renderer_destroy_shader(r, shader);
    pz_renderer_destroy(r);
    pz_log_shutdown();
}

/* ============================================================================
 * Test Runner
 * ============================================================================
 */

TEST_MAIN()
