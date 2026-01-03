/*
 * Tank Game - Math Library
 *
 * Vector and matrix types for 3D rendering and 2D gameplay.
 */

#ifndef PZ_MATH_H
#define PZ_MATH_H

#include <math.h>
#include <stdbool.h>

// Constants
#define PZ_PI 3.14159265358979323846f
#define PZ_DEG2RAD (PZ_PI / 180.0f)
#define PZ_RAD2DEG (180.0f / PZ_PI)

// Utility
static inline float
pz_clampf(float x, float min, float max)
{
    return x < min ? min : (x > max ? max : x);
}

static inline float
pz_lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline float
pz_minf(float a, float b)
{
    return a < b ? a : b;
}
static inline float
pz_maxf(float a, float b)
{
    return a > b ? a : b;
}
static inline float
pz_absf(float x)
{
    return x < 0 ? -x : x;
}

// ============================================================================
// vec2 - 2D vector
// ============================================================================

typedef struct pz_vec2 {
    float x, y;
} pz_vec2;

static inline pz_vec2
pz_vec2_new(float x, float y)
{
    return (pz_vec2) { x, y };
}

static inline pz_vec2
pz_vec2_zero(void)
{
    return (pz_vec2) { 0.0f, 0.0f };
}

static inline pz_vec2
pz_vec2_add(pz_vec2 a, pz_vec2 b)
{
    return (pz_vec2) { a.x + b.x, a.y + b.y };
}

static inline pz_vec2
pz_vec2_sub(pz_vec2 a, pz_vec2 b)
{
    return (pz_vec2) { a.x - b.x, a.y - b.y };
}

static inline pz_vec2
pz_vec2_scale(pz_vec2 v, float s)
{
    return (pz_vec2) { v.x * s, v.y * s };
}

static inline pz_vec2
pz_vec2_neg(pz_vec2 v)
{
    return (pz_vec2) { -v.x, -v.y };
}

static inline float
pz_vec2_dot(pz_vec2 a, pz_vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

static inline float
pz_vec2_len_sq(pz_vec2 v)
{
    return v.x * v.x + v.y * v.y;
}

static inline float
pz_vec2_len(pz_vec2 v)
{
    return sqrtf(pz_vec2_len_sq(v));
}

static inline pz_vec2
pz_vec2_normalize(pz_vec2 v)
{
    float len = pz_vec2_len(v);
    if (len < 0.0001f)
        return pz_vec2_zero();
    return pz_vec2_scale(v, 1.0f / len);
}

static inline pz_vec2
pz_vec2_rotate(pz_vec2 v, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);
    return (pz_vec2) { v.x * c - v.y * s, v.x * s + v.y * c };
}

static inline pz_vec2
pz_vec2_reflect(pz_vec2 v, pz_vec2 normal)
{
    float d = 2.0f * pz_vec2_dot(v, normal);
    return pz_vec2_sub(v, pz_vec2_scale(normal, d));
}

static inline float
pz_vec2_dist(pz_vec2 a, pz_vec2 b)
{
    return pz_vec2_len(pz_vec2_sub(a, b));
}

static inline pz_vec2
pz_vec2_lerp(pz_vec2 a, pz_vec2 b, float t)
{
    return (pz_vec2) { pz_lerpf(a.x, b.x, t), pz_lerpf(a.y, b.y, t) };
}

// ============================================================================
// vec3 - 3D vector
// ============================================================================

typedef struct pz_vec3 {
    float x, y, z;
} pz_vec3;

static inline pz_vec3
pz_vec3_new(float x, float y, float z)
{
    return (pz_vec3) { x, y, z };
}

static inline pz_vec3
pz_vec3_zero(void)
{
    return (pz_vec3) { 0.0f, 0.0f, 0.0f };
}

static inline pz_vec3
pz_vec3_add(pz_vec3 a, pz_vec3 b)
{
    return (pz_vec3) { a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline pz_vec3
pz_vec3_sub(pz_vec3 a, pz_vec3 b)
{
    return (pz_vec3) { a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline pz_vec3
pz_vec3_scale(pz_vec3 v, float s)
{
    return (pz_vec3) { v.x * s, v.y * s, v.z * s };
}

static inline pz_vec3
pz_vec3_neg(pz_vec3 v)
{
    return (pz_vec3) { -v.x, -v.y, -v.z };
}

static inline float
pz_vec3_dot(pz_vec3 a, pz_vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline pz_vec3
pz_vec3_cross(pz_vec3 a, pz_vec3 b)
{
    return (pz_vec3) { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x };
}

static inline float
pz_vec3_len_sq(pz_vec3 v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline float
pz_vec3_len(pz_vec3 v)
{
    return sqrtf(pz_vec3_len_sq(v));
}

static inline pz_vec3
pz_vec3_normalize(pz_vec3 v)
{
    float len = pz_vec3_len(v);
    if (len < 0.0001f)
        return pz_vec3_zero();
    return pz_vec3_scale(v, 1.0f / len);
}

static inline pz_vec3
pz_vec3_lerp(pz_vec3 a, pz_vec3 b, float t)
{
    return (pz_vec3) { pz_lerpf(a.x, b.x, t), pz_lerpf(a.y, b.y, t),
        pz_lerpf(a.z, b.z, t) };
}

static inline pz_vec3
pz_vec3_mul(pz_vec3 a, pz_vec3 b)
{
    return (pz_vec3) { a.x * b.x, a.y * b.y, a.z * b.z };
}

// ============================================================================
// Color utilities (colors stored as vec3 with RGB in 0-1 range)
// ============================================================================

// Darken a color by a factor (0 = black, 1 = unchanged)
static inline pz_vec3
pz_color_darken(pz_vec3 color, float factor)
{
    return pz_vec3_scale(color, pz_clampf(factor, 0.0f, 1.0f));
}

// Lighten a color toward white by a factor (0 = unchanged, 1 = white)
static inline pz_vec3
pz_color_lighten(pz_vec3 color, float factor)
{
    factor = pz_clampf(factor, 0.0f, 1.0f);
    return (pz_vec3) {
        color.x + (1.0f - color.x) * factor,
        color.y + (1.0f - color.y) * factor,
        color.z + (1.0f - color.z) * factor,
    };
}

// Mix two colors
static inline pz_vec3
pz_color_mix(pz_vec3 a, pz_vec3 b, float t)
{
    return pz_vec3_lerp(a, b, pz_clampf(t, 0.0f, 1.0f));
}

// Convert hex color (0xRRGGBB) to vec3
static inline pz_vec3
pz_color_from_hex(unsigned int hex)
{
    return (pz_vec3) {
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8) & 0xFF) / 255.0f,
        (hex & 0xFF) / 255.0f,
    };
}

// Adjust saturation (0 = grayscale, 1 = unchanged, >1 = more saturated)
static inline pz_vec3
pz_color_saturate(pz_vec3 color, float factor)
{
    float gray = 0.299f * color.x + 0.587f * color.y + 0.114f * color.z;
    return (pz_vec3) {
        gray + (color.x - gray) * factor,
        gray + (color.y - gray) * factor,
        gray + (color.z - gray) * factor,
    };
}

// ============================================================================
// vec4 - 4D vector / homogeneous coordinates
// ============================================================================

typedef struct pz_vec4 {
    float x, y, z, w;
} pz_vec4;

static inline pz_vec4
pz_vec4_new(float x, float y, float z, float w)
{
    return (pz_vec4) { x, y, z, w };
}

static inline pz_vec4
pz_vec4_from_vec3(pz_vec3 v, float w)
{
    return (pz_vec4) { v.x, v.y, v.z, w };
}

static inline pz_vec4
pz_vec4_add(pz_vec4 a, pz_vec4 b)
{
    return (pz_vec4) { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

static inline pz_vec4
pz_vec4_scale(pz_vec4 v, float s)
{
    return (pz_vec4) { v.x * s, v.y * s, v.z * s, v.w * s };
}

// ============================================================================
// mat4 - 4x4 matrix (column-major for OpenGL)
// ============================================================================

typedef struct pz_mat4 {
    float m[16];
} pz_mat4;

// Identity matrix
pz_mat4 pz_mat4_identity(void);

// Matrix multiplication: result = a * b
pz_mat4 pz_mat4_mul(pz_mat4 a, pz_mat4 b);

// Transform vec4 by matrix
pz_vec4 pz_mat4_mul_vec4(pz_mat4 m, pz_vec4 v);

// Translation matrix
pz_mat4 pz_mat4_translate(pz_vec3 t);

// Rotation matrices (angle in radians)
pz_mat4 pz_mat4_rotate_x(float angle);
pz_mat4 pz_mat4_rotate_y(float angle);
pz_mat4 pz_mat4_rotate_z(float angle);

// Scale matrix
pz_mat4 pz_mat4_scale(pz_vec3 s);

// Perspective projection
// fov: field of view in radians
pz_mat4 pz_mat4_perspective(float fov, float aspect, float near, float far);

// Orthographic projection
pz_mat4 pz_mat4_ortho(
    float left, float right, float bottom, float top, float near, float far);

// Look-at view matrix
pz_mat4 pz_mat4_look_at(pz_vec3 eye, pz_vec3 target, pz_vec3 up);

// Inverse of a matrix (returns identity if singular)
pz_mat4 pz_mat4_inverse(pz_mat4 m);

#endif // PZ_MATH_H
