// ============================================================================
// UI Quad Shader - For rendering 2D UI elements (solid color only)
// ============================================================================

@vs ui_quad_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_texcoord;
layout(location=2) in vec4 a_color;

layout(std140, binding=0) uniform ui_quad_vs_params {
    mat4 u_projection;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}
@end

@fs ui_quad_fs
layout(location=0) in vec2 v_texcoord;
layout(location=1) in vec4 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    // Simple solid color (texcoord unused)
    frag_color = v_color;
}
@end

@program ui_quad ui_quad_vs ui_quad_fs
