/*
 * Tank Game - Debug Overlay Implementation
 */

#include "pz_debug_overlay.h"
#include "../core/pz_log.h"
#include "../core/pz_mem.h"
#include "../core/pz_platform.h"
#include "../core/pz_str.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================
 */

#define FRAME_TIME_HISTORY 120 // Number of frames to track for graph
#define FONT_CHAR_WIDTH 8 // Pixels per character
#define FONT_CHAR_HEIGHT 8 // Pixels per character
#define FONT_FIRST_CHAR 32 // ASCII start (space)
#define FONT_LAST_CHAR 126 // ASCII end (~)
#define FONT_CHARS_PER_ROW 16 // Characters per row in texture
#define MAX_TEXT_CHARS 4096 // Maximum characters per frame
#define GRAPH_WIDTH 120 // Graph width in pixels
#define GRAPH_HEIGHT 60 // Graph height in pixels

/* ============================================================================
 * Embedded 8x8 Font Data (CP437-style)
 *
 * Each character is 8 bytes (8x8 pixels, 1 bit per pixel, MSB = left)
 * Characters 32-126 (printable ASCII)
 * ============================================================================
 */

// clang-format off
static const uint8_t font_8x8_data[] = {
    // 32: Space
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 33: !
    0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00,
    // 34: "
    0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 35: #
    0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00,
    // 36: $
    0x18, 0x7E, 0xC0, 0x7C, 0x06, 0xFC, 0x18, 0x00,
    // 37: %
    0x00, 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC6, 0x00,
    // 38: &
    0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00,
    // 39: '
    0x30, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 40: (
    0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00,
    // 41: )
    0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00,
    // 42: *
    0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00,
    // 43: +
    0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00,
    // 44: ,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30,
    // 45: -
    0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00,
    // 46: .
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00,
    // 47: /
    0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00,
    // 48: 0
    0x7C, 0xC6, 0xCE, 0xDE, 0xF6, 0xE6, 0x7C, 0x00,
    // 49: 1
    0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x7E, 0x00,
    // 50: 2
    0x7C, 0xC6, 0x06, 0x1C, 0x30, 0x66, 0xFE, 0x00,
    // 51: 3
    0x7C, 0xC6, 0x06, 0x3C, 0x06, 0xC6, 0x7C, 0x00,
    // 52: 4
    0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x1E, 0x00,
    // 53: 5
    0xFE, 0xC0, 0xC0, 0xFC, 0x06, 0xC6, 0x7C, 0x00,
    // 54: 6
    0x38, 0x60, 0xC0, 0xFC, 0xC6, 0xC6, 0x7C, 0x00,
    // 55: 7
    0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00,
    // 56: 8
    0x7C, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0x7C, 0x00,
    // 57: 9
    0x7C, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0x78, 0x00,
    // 58: :
    0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
    // 59: ;
    0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30,
    // 60: <
    0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00,
    // 61: =
    0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00,
    // 62: >
    0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00,
    // 63: ?
    0x7C, 0xC6, 0x0C, 0x18, 0x18, 0x00, 0x18, 0x00,
    // 64: @
    0x7C, 0xC6, 0xDE, 0xDE, 0xDE, 0xC0, 0x78, 0x00,
    // 65: A
    0x38, 0x6C, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00,
    // 66: B
    0xFC, 0x66, 0x66, 0x7C, 0x66, 0x66, 0xFC, 0x00,
    // 67: C
    0x3C, 0x66, 0xC0, 0xC0, 0xC0, 0x66, 0x3C, 0x00,
    // 68: D
    0xF8, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00,
    // 69: E
    0xFE, 0x62, 0x68, 0x78, 0x68, 0x62, 0xFE, 0x00,
    // 70: F
    0xFE, 0x62, 0x68, 0x78, 0x68, 0x60, 0xF0, 0x00,
    // 71: G
    0x3C, 0x66, 0xC0, 0xC0, 0xCE, 0x66, 0x3A, 0x00,
    // 72: H
    0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00,
    // 73: I
    0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00,
    // 74: J
    0x1E, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78, 0x00,
    // 75: K
    0xE6, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0xE6, 0x00,
    // 76: L
    0xF0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xFE, 0x00,
    // 77: M
    0xC6, 0xEE, 0xFE, 0xFE, 0xD6, 0xC6, 0xC6, 0x00,
    // 78: N
    0xC6, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0xC6, 0x00,
    // 79: O
    0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00,
    // 80: P
    0xFC, 0x66, 0x66, 0x7C, 0x60, 0x60, 0xF0, 0x00,
    // 81: Q
    0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xCE, 0x7C, 0x0E,
    // 82: R
    0xFC, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0xE6, 0x00,
    // 83: S
    0x7C, 0xC6, 0x60, 0x38, 0x0C, 0xC6, 0x7C, 0x00,
    // 84: T
    0x7E, 0x7E, 0x5A, 0x18, 0x18, 0x18, 0x3C, 0x00,
    // 85: U
    0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00,
    // 86: V
    0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00,
    // 87: W
    0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0xFE, 0x6C, 0x00,
    // 88: X
    0xC6, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0xC6, 0x00,
    // 89: Y
    0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x00,
    // 90: Z
    0xFE, 0xC6, 0x8C, 0x18, 0x32, 0x66, 0xFE, 0x00,
    // 91: [
    0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00,
    // 92: backslash
    0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00,
    // 93: ]
    0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00,
    // 94: ^
    0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00,
    // 95: _
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
    // 96: `
    0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 97: a
    0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0x00,
    // 98: b
    0xE0, 0x60, 0x60, 0x7C, 0x66, 0x66, 0xDC, 0x00,
    // 99: c
    0x00, 0x00, 0x78, 0xCC, 0xC0, 0xCC, 0x78, 0x00,
    // 100: d
    0x1C, 0x0C, 0x0C, 0x7C, 0xCC, 0xCC, 0x76, 0x00,
    // 101: e
    0x00, 0x00, 0x78, 0xCC, 0xFC, 0xC0, 0x78, 0x00,
    // 102: f
    0x38, 0x6C, 0x64, 0xF0, 0x60, 0x60, 0xF0, 0x00,
    // 103: g
    0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0xF8,
    // 104: h
    0xE0, 0x60, 0x6C, 0x76, 0x66, 0x66, 0xE6, 0x00,
    // 105: i
    0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00,
    // 106: j
    0x06, 0x00, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C,
    // 107: k
    0xE0, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0xE6, 0x00,
    // 108: l
    0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00,
    // 109: m
    0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xD6, 0xC6, 0x00,
    // 110: n
    0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x00,
    // 111: o
    0x00, 0x00, 0x78, 0xCC, 0xCC, 0xCC, 0x78, 0x00,
    // 112: p
    0x00, 0x00, 0xDC, 0x66, 0x66, 0x7C, 0x60, 0xF0,
    // 113: q
    0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0x1E,
    // 114: r
    0x00, 0x00, 0xDC, 0x76, 0x66, 0x60, 0xF0, 0x00,
    // 115: s
    0x00, 0x00, 0x7C, 0xC0, 0x70, 0x1C, 0xF8, 0x00,
    // 116: t
    0x10, 0x30, 0x7C, 0x30, 0x30, 0x34, 0x18, 0x00,
    // 117: u
    0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00,
    // 118: v
    0x00, 0x00, 0xCC, 0xCC, 0xCC, 0x78, 0x30, 0x00,
    // 119: w
    0x00, 0x00, 0xC6, 0xC6, 0xD6, 0xFE, 0x6C, 0x00,
    // 120: x
    0x00, 0x00, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0x00,
    // 121: y
    0x00, 0x00, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0xF8,
    // 122: z
    0x00, 0x00, 0xFC, 0x98, 0x30, 0x64, 0xFC, 0x00,
    // 123: {
    0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00,
    // 124: |
    0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00,
    // 125: }
    0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00,
    // 126: ~
    0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// clang-format on

/* ============================================================================
 * Vertex Structures
 * ============================================================================
 */

typedef struct text_vertex {
    float x, y; // Position (screen space)
    float u, v; // Texture coordinates
    float r, g, b, a; // Color
} text_vertex;

typedef struct line_vertex {
    float x, y; // Position (screen space)
    float r, g, b, a; // Color
} line_vertex;

/* ============================================================================
 * Debug Overlay Structure
 * ============================================================================
 */

struct pz_debug_overlay {
    pz_renderer *renderer;
    bool visible;

    // Font texture (16x6 grid of 8x8 characters = 128x48 texture)
    pz_texture_handle font_texture;

    // Shaders
    pz_shader_handle text_shader;
    pz_shader_handle line_shader;

    // Pipelines
    pz_pipeline_handle text_pipeline;
    pz_pipeline_handle line_pipeline;

    // Dynamic buffers
    pz_buffer_handle text_vb;
    pz_buffer_handle line_vb;

    // Text vertex data (rebuilt each frame)
    text_vertex *text_vertices;
    int text_vertex_count;
    int text_vertex_capacity;

    // Line vertex data (for graph)
    line_vertex *line_vertices;
    int line_vertex_count;
    int line_vertex_capacity;

    // Frame timing
    double frame_start_time;
    float frame_times[FRAME_TIME_HISTORY];
    int frame_time_index;
    float fps;
    float avg_frame_time_ms;

    // Viewport cache
    int viewport_width;
    int viewport_height;
};

/* ============================================================================
 * Shader Sources
 * ============================================================================
 */

static const char *text_vertex_shader
    = "#version 330 core\n"
      "layout(location = 0) in vec2 a_position;\n"
      "layout(location = 1) in vec2 a_texcoord;\n"
      "layout(location = 2) in vec4 a_color;\n"
      "out vec2 v_texcoord;\n"
      "out vec4 v_color;\n"
      "uniform vec2 u_screen_size;\n"
      "void main() {\n"
      "    vec2 pos = (a_position / u_screen_size) * 2.0 - 1.0;\n"
      "    pos.y = -pos.y;\n"
      "    gl_Position = vec4(pos, 0.0, 1.0);\n"
      "    v_texcoord = a_texcoord;\n"
      "    v_color = a_color;\n"
      "}\n";

static const char *text_fragment_shader
    = "#version 330 core\n"
      "in vec2 v_texcoord;\n"
      "in vec4 v_color;\n"
      "out vec4 frag_color;\n"
      "uniform sampler2D u_texture;\n"
      "void main() {\n"
      "    float alpha = texture(u_texture, v_texcoord).r;\n"
      "    frag_color = vec4(v_color.rgb, v_color.a * alpha);\n"
      "}\n";

static const char *line_vertex_shader
    = "#version 330 core\n"
      "layout(location = 0) in vec2 a_position;\n"
      "layout(location = 1) in vec4 a_color;\n"
      "out vec4 v_color;\n"
      "uniform vec2 u_screen_size;\n"
      "void main() {\n"
      "    vec2 pos = (a_position / u_screen_size) * 2.0 - 1.0;\n"
      "    pos.y = -pos.y;\n"
      "    gl_Position = vec4(pos, 0.0, 1.0);\n"
      "    v_color = a_color;\n"
      "}\n";

static const char *line_fragment_shader = "#version 330 core\n"
                                          "in vec4 v_color;\n"
                                          "out vec4 frag_color;\n"
                                          "void main() {\n"
                                          "    frag_color = v_color;\n"
                                          "}\n";

/* ============================================================================
 * Internal Functions
 * ============================================================================
 */

static pz_texture_handle
create_font_texture(pz_renderer *renderer)
{
    // Create a 128x48 texture (16 chars wide, 6 rows tall, 8x8 each)
    // But we only use 95 chars (32-126), so some will be blank
    int tex_width = FONT_CHARS_PER_ROW * FONT_CHAR_WIDTH; // 128
    int tex_height = 6 * FONT_CHAR_HEIGHT; // 48

    uint8_t *pixels = pz_calloc(1, tex_width * tex_height);

    int num_chars = FONT_LAST_CHAR - FONT_FIRST_CHAR + 1; // 95 characters

    for (int i = 0; i < num_chars; i++) {
        int row = i / FONT_CHARS_PER_ROW;
        int col = i % FONT_CHARS_PER_ROW;

        int base_x = col * FONT_CHAR_WIDTH;
        int base_y = row * FONT_CHAR_HEIGHT;

        const uint8_t *glyph = &font_8x8_data[i * 8];

        for (int y = 0; y < 8; y++) {
            uint8_t bits = glyph[y];
            for (int x = 0; x < 8; x++) {
                if (bits & (0x80 >> x)) {
                    int px = base_x + x;
                    int py = base_y + y;
                    pixels[py * tex_width + px] = 255;
                }
            }
        }
    }

    pz_texture_desc desc = {
        .width = tex_width,
        .height = tex_height,
        .format = PZ_TEXTURE_R8,
        .filter = PZ_FILTER_NEAREST,
        .wrap = PZ_WRAP_CLAMP,
        .data = pixels,
    };

    pz_texture_handle tex = pz_renderer_create_texture(renderer, &desc);
    pz_free(pixels);

    return tex;
}

static void
add_text_quad(pz_debug_overlay *overlay, float x, float y, float w, float h,
    float u0, float v0, float u1, float v1, pz_vec4 color)
{
    if (overlay->text_vertex_count + 6 > overlay->text_vertex_capacity) {
        return; // Buffer full
    }

    text_vertex *v = &overlay->text_vertices[overlay->text_vertex_count];

    // Triangle 1
    v[0] = (text_vertex) { x, y, u0, v0, color.x, color.y, color.z, color.w };
    v[1] = (text_vertex) { x + w, y, u1, v0, color.x, color.y, color.z,
        color.w };
    v[2] = (text_vertex) { x + w, y + h, u1, v1, color.x, color.y, color.z,
        color.w };

    // Triangle 2
    v[3] = (text_vertex) { x, y, u0, v0, color.x, color.y, color.z, color.w };
    v[4] = (text_vertex) { x + w, y + h, u1, v1, color.x, color.y, color.z,
        color.w };
    v[5] = (text_vertex) { x, y + h, u0, v1, color.x, color.y, color.z,
        color.w };

    overlay->text_vertex_count += 6;
}

static void
add_line(pz_debug_overlay *overlay, float x0, float y0, float x1, float y1,
    pz_vec4 color)
{
    if (overlay->line_vertex_count + 2 > overlay->line_vertex_capacity) {
        return;
    }

    line_vertex *v = &overlay->line_vertices[overlay->line_vertex_count];
    v[0] = (line_vertex) { x0, y0, color.x, color.y, color.z, color.w };
    v[1] = (line_vertex) { x1, y1, color.x, color.y, color.z, color.w };

    overlay->line_vertex_count += 2;
}

static void
render_text_internal(
    pz_debug_overlay *overlay, int x, int y, pz_vec4 color, const char *text)
{
    float tex_width = FONT_CHARS_PER_ROW * FONT_CHAR_WIDTH; // 128
    float tex_height = 6 * FONT_CHAR_HEIGHT; // 48

    float char_w = FONT_CHAR_WIDTH;
    float char_h = FONT_CHAR_HEIGHT;

    float cursor_x = (float)x;
    float cursor_y = (float)y;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            cursor_x = (float)x;
            cursor_y += char_h;
            continue;
        }

        if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
            cursor_x += char_w;
            continue;
        }

        int char_index = c - FONT_FIRST_CHAR;
        int row = char_index / FONT_CHARS_PER_ROW;
        int col = char_index % FONT_CHARS_PER_ROW;

        float u0 = (col * char_w) / tex_width;
        float v0 = (row * char_h) / tex_height;
        float u1 = ((col + 1) * char_w) / tex_width;
        float v1 = ((row + 1) * char_h) / tex_height;

        add_text_quad(
            overlay, cursor_x, cursor_y, char_w, char_h, u0, v0, u1, v1, color);

        cursor_x += char_w;
    }
}

static void
render_builtin_overlay(pz_debug_overlay *overlay)
{
    // Background for the overlay panel
    int panel_x = 8;
    int panel_y = 8;
    int panel_w = 200;
    int panel_h = 90;

    // Semi-transparent background (rendered as a solid quad using the font
    // texture)
    // We'll use a filled area in the font texture (or just skip this for now)

    pz_vec4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    pz_vec4 green = { 0.3f, 1.0f, 0.3f, 1.0f };
    pz_vec4 yellow = { 1.0f, 1.0f, 0.3f, 1.0f };
    pz_vec4 red = { 1.0f, 0.3f, 0.3f, 1.0f };
    pz_vec4 cyan = { 0.3f, 1.0f, 1.0f, 1.0f };
    (void)panel_w;
    (void)panel_h;
    (void)white;
    (void)cyan;

    // FPS display
    char buf[128];
    pz_vec4 fps_color = green;
    if (overlay->fps < 30.0f) {
        fps_color = red;
    } else if (overlay->fps < 55.0f) {
        fps_color = yellow;
    }

    snprintf(buf, sizeof(buf), "FPS: %.1f", overlay->fps);
    render_text_internal(overlay, panel_x, panel_y, fps_color, buf);

    snprintf(buf, sizeof(buf), "Frame: %.2f ms", overlay->avg_frame_time_ms);
    render_text_internal(overlay, panel_x, panel_y + 10, fps_color, buf);

    // Frame time graph
    int graph_x = panel_x;
    int graph_y = panel_y + 28;

    // Draw graph background lines (16.67ms = 60fps, 33.33ms = 30fps)
    pz_vec4 grid_color = { 0.3f, 0.3f, 0.3f, 0.5f };
    pz_vec4 graph_color = { 0.3f, 1.0f, 0.3f, 0.8f };

    // 60 FPS line (16.67ms)
    float y_60fps = graph_y + GRAPH_HEIGHT - (16.67f / 50.0f) * GRAPH_HEIGHT;
    add_line(
        overlay, graph_x, y_60fps, graph_x + GRAPH_WIDTH, y_60fps, grid_color);

    // 30 FPS line (33.33ms)
    float y_30fps = graph_y + GRAPH_HEIGHT - (33.33f / 50.0f) * GRAPH_HEIGHT;
    add_line(
        overlay, graph_x, y_30fps, graph_x + GRAPH_WIDTH, y_30fps, grid_color);

    // Draw frame time graph (line strip as individual lines)
    for (int i = 0; i < FRAME_TIME_HISTORY - 1; i++) {
        int idx0 = (overlay->frame_time_index + 1 + i) % FRAME_TIME_HISTORY;
        int idx1 = (overlay->frame_time_index + 2 + i) % FRAME_TIME_HISTORY;

        float t0 = overlay->frame_times[idx0];
        float t1 = overlay->frame_times[idx1];

        // Clamp to 50ms max for display
        t0 = pz_clampf(t0, 0.0f, 50.0f);
        t1 = pz_clampf(t1, 0.0f, 50.0f);

        float x0 = graph_x + i;
        float y0 = graph_y + GRAPH_HEIGHT - (t0 / 50.0f) * GRAPH_HEIGHT;
        float x1 = graph_x + i + 1;
        float y1 = graph_y + GRAPH_HEIGHT - (t1 / 50.0f) * GRAPH_HEIGHT;

        // Color based on frame time
        pz_vec4 line_color = graph_color;
        if (t0 > 33.33f || t1 > 33.33f) {
            line_color = red;
        } else if (t0 > 16.67f || t1 > 16.67f) {
            line_color = yellow;
        }

        add_line(overlay, x0, y0, x1, y1, line_color);
    }
}

/* ============================================================================
 * Public API
 * ============================================================================
 */

pz_debug_overlay *
pz_debug_overlay_create(pz_renderer *renderer)
{
    pz_debug_overlay *overlay = pz_calloc(1, sizeof(pz_debug_overlay));
    overlay->renderer = renderer;
    overlay->visible = false;

    pz_renderer_get_viewport(
        renderer, &overlay->viewport_width, &overlay->viewport_height);

    // Create font texture
    overlay->font_texture = create_font_texture(renderer);
    if (overlay->font_texture == PZ_INVALID_HANDLE) {
        pz_log(PZ_LOG_ERROR, PZ_LOG_CAT_RENDER,
            "Failed to create debug font texture");
        pz_free(overlay);
        return NULL;
    }

    // Create text shader
    pz_shader_desc text_shader_desc = {
        .vertex_source = text_vertex_shader,
        .fragment_source = text_fragment_shader,
        .name = "debug_text",
    };
    overlay->text_shader
        = pz_renderer_create_shader(renderer, &text_shader_desc);

    // Create line shader
    pz_shader_desc line_shader_desc = {
        .vertex_source = line_vertex_shader,
        .fragment_source = line_fragment_shader,
        .name = "debug_line",
    };
    overlay->line_shader
        = pz_renderer_create_shader(renderer, &line_shader_desc);

    // Create text pipeline
    pz_vertex_attr text_attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT2, .offset = 0 },
        { .name = "a_texcoord",
            .type = PZ_ATTR_FLOAT2,
            .offset = 2 * sizeof(float) },
        { .name = "a_color",
            .type = PZ_ATTR_FLOAT4,
            .offset = 4 * sizeof(float) },
    };

    pz_pipeline_desc text_pipeline_desc = {
        .shader = overlay->text_shader,
        .vertex_layout = {
            .attrs = text_attrs,
            .attr_count = 3,
            .stride = sizeof(text_vertex),
        },
        .blend = PZ_BLEND_ALPHA,
        .depth = PZ_DEPTH_NONE,
        .cull = PZ_CULL_NONE,
        .primitive = PZ_PRIMITIVE_TRIANGLES,
    };
    overlay->text_pipeline
        = pz_renderer_create_pipeline(renderer, &text_pipeline_desc);

    // Create line pipeline
    pz_vertex_attr line_attrs[] = {
        { .name = "a_position", .type = PZ_ATTR_FLOAT2, .offset = 0 },
        { .name = "a_color",
            .type = PZ_ATTR_FLOAT4,
            .offset = 2 * sizeof(float) },
    };

    pz_pipeline_desc line_pipeline_desc = {
        .shader = overlay->line_shader,
        .vertex_layout = {
            .attrs = line_attrs,
            .attr_count = 2,
            .stride = sizeof(line_vertex),
        },
        .blend = PZ_BLEND_ALPHA,
        .depth = PZ_DEPTH_NONE,
        .cull = PZ_CULL_NONE,
        .primitive = PZ_PRIMITIVE_LINES,
    };
    overlay->line_pipeline
        = pz_renderer_create_pipeline(renderer, &line_pipeline_desc);

    // Create dynamic vertex buffers
    overlay->text_vertex_capacity = MAX_TEXT_CHARS * 6;
    overlay->text_vertices
        = pz_alloc(overlay->text_vertex_capacity * sizeof(text_vertex));

    pz_buffer_desc text_vb_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_DYNAMIC,
        .data = NULL,
        .size = overlay->text_vertex_capacity * sizeof(text_vertex),
    };
    overlay->text_vb = pz_renderer_create_buffer(renderer, &text_vb_desc);

    overlay->line_vertex_capacity = 1024;
    overlay->line_vertices
        = pz_alloc(overlay->line_vertex_capacity * sizeof(line_vertex));

    pz_buffer_desc line_vb_desc = {
        .type = PZ_BUFFER_VERTEX,
        .usage = PZ_BUFFER_DYNAMIC,
        .data = NULL,
        .size = overlay->line_vertex_capacity * sizeof(line_vertex),
    };
    overlay->line_vb = pz_renderer_create_buffer(renderer, &line_vb_desc);

    // Initialize timing
    memset(overlay->frame_times, 0, sizeof(overlay->frame_times));
    overlay->frame_time_index = 0;
    overlay->fps = 0.0f;
    overlay->avg_frame_time_ms = 0.0f;

    pz_log(PZ_LOG_INFO, PZ_LOG_CAT_RENDER,
        "Debug overlay created (press F2 to toggle)");

    return overlay;
}

void
pz_debug_overlay_destroy(pz_debug_overlay *overlay)
{
    if (!overlay)
        return;

    pz_renderer_destroy_buffer(overlay->renderer, overlay->text_vb);
    pz_renderer_destroy_buffer(overlay->renderer, overlay->line_vb);
    pz_renderer_destroy_pipeline(overlay->renderer, overlay->text_pipeline);
    pz_renderer_destroy_pipeline(overlay->renderer, overlay->line_pipeline);
    pz_renderer_destroy_shader(overlay->renderer, overlay->text_shader);
    pz_renderer_destroy_shader(overlay->renderer, overlay->line_shader);
    pz_renderer_destroy_texture(overlay->renderer, overlay->font_texture);

    pz_free(overlay->text_vertices);
    pz_free(overlay->line_vertices);
    pz_free(overlay);
}

void
pz_debug_overlay_toggle(pz_debug_overlay *overlay)
{
    if (!overlay)
        return;
    overlay->visible = !overlay->visible;
    pz_log(PZ_LOG_DEBUG, PZ_LOG_CAT_RENDER, "Debug overlay: %s",
        overlay->visible ? "visible" : "hidden");
}

void
pz_debug_overlay_set_visible(pz_debug_overlay *overlay, bool visible)
{
    if (!overlay)
        return;
    overlay->visible = visible;
}

bool
pz_debug_overlay_is_visible(pz_debug_overlay *overlay)
{
    return overlay ? overlay->visible : false;
}

void
pz_debug_overlay_begin_frame(pz_debug_overlay *overlay)
{
    if (!overlay)
        return;

    overlay->frame_start_time = pz_time_now();

    // Reset vertex counts for this frame
    overlay->text_vertex_count = 0;
    overlay->line_vertex_count = 0;

    // Update viewport in case of resize
    pz_renderer_get_viewport(
        overlay->renderer, &overlay->viewport_width, &overlay->viewport_height);
}

void
pz_debug_overlay_end_frame(pz_debug_overlay *overlay)
{
    if (!overlay)
        return;

    double frame_end = pz_time_now();
    float frame_time_ms
        = (float)((frame_end - overlay->frame_start_time) * 1000.0);

    // Store frame time
    overlay->frame_times[overlay->frame_time_index] = frame_time_ms;
    overlay->frame_time_index
        = (overlay->frame_time_index + 1) % FRAME_TIME_HISTORY;

    // Calculate average frame time and FPS
    float total = 0.0f;
    for (int i = 0; i < FRAME_TIME_HISTORY; i++) {
        total += overlay->frame_times[i];
    }
    overlay->avg_frame_time_ms = total / FRAME_TIME_HISTORY;
    overlay->fps = (overlay->avg_frame_time_ms > 0.001f)
        ? (1000.0f / overlay->avg_frame_time_ms)
        : 0.0f;
}

void
pz_debug_overlay_text(
    pz_debug_overlay *overlay, int x, int y, const char *fmt, ...)
{
    if (!overlay || !overlay->visible)
        return;

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    pz_vec4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    render_text_internal(overlay, x, y, white, buf);
}

void
pz_debug_overlay_text_color(pz_debug_overlay *overlay, int x, int y,
    pz_vec4 color, const char *fmt, ...)
{
    if (!overlay || !overlay->visible)
        return;

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    render_text_internal(overlay, x, y, color, buf);
}

void
pz_debug_overlay_render(pz_debug_overlay *overlay)
{
    if (!overlay || !overlay->visible)
        return;

    // Build the default overlay (FPS, frame graph)
    render_builtin_overlay(overlay);

    // Upload text vertices
    if (overlay->text_vertex_count > 0) {
        pz_renderer_update_buffer(overlay->renderer, overlay->text_vb, 0,
            overlay->text_vertices,
            overlay->text_vertex_count * sizeof(text_vertex));
    }

    // Upload line vertices
    if (overlay->line_vertex_count > 0) {
        pz_renderer_update_buffer(overlay->renderer, overlay->line_vb, 0,
            overlay->line_vertices,
            overlay->line_vertex_count * sizeof(line_vertex));
    }

    // Set up screen size uniform
    pz_vec2 screen_size
        = { (float)overlay->viewport_width, (float)overlay->viewport_height };

    // Draw lines first (graph background)
    if (overlay->line_vertex_count > 0) {
        pz_renderer_set_uniform_vec2(overlay->renderer, overlay->line_shader,
            "u_screen_size", screen_size);

        pz_draw_cmd line_cmd = {
            .pipeline = overlay->line_pipeline,
            .vertex_buffer = overlay->line_vb,
            .vertex_count = overlay->line_vertex_count,
        };
        pz_renderer_draw(overlay->renderer, &line_cmd);
    }

    // Draw text
    if (overlay->text_vertex_count > 0) {
        pz_renderer_set_uniform_vec2(overlay->renderer, overlay->text_shader,
            "u_screen_size", screen_size);
        pz_renderer_set_uniform_int(
            overlay->renderer, overlay->text_shader, "u_texture", 0);
        pz_renderer_bind_texture(overlay->renderer, 0, overlay->font_texture);

        pz_draw_cmd text_cmd = {
            .pipeline = overlay->text_pipeline,
            .vertex_buffer = overlay->text_vb,
            .vertex_count = overlay->text_vertex_count,
        };
        pz_renderer_draw(overlay->renderer, &text_cmd);
    }
}

float
pz_debug_overlay_get_fps(pz_debug_overlay *overlay)
{
    return overlay ? overlay->fps : 0.0f;
}

float
pz_debug_overlay_get_frame_time_ms(pz_debug_overlay *overlay)
{
    return overlay ? overlay->avg_frame_time_ms : 0.0f;
}
