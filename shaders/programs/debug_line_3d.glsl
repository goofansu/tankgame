// ============================================================================
// Debug Line 3D Shader - For world-space debug lines
// ============================================================================

@vs debug_line_3d_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec4 a_color;

layout(std140, binding=0) uniform debug_line_3d_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec4 v_color;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
@end

@fs debug_line_3d_fs
layout(location=0) in vec4 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = v_color;
}
@end

@program debug_line_3d debug_line_3d_vs debug_line_3d_fs

