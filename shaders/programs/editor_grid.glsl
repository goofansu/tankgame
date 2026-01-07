// ============================================================================
// Editor Grid Shader - For rendering editor grid lines
// ============================================================================

@vs editor_grid_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec4 a_color;

layout(std140, binding=0) uniform editor_grid_vs_params {
    mat4 u_view_projection;
};

layout(location=0) out vec4 v_color;

void main() {
    gl_Position = u_view_projection * vec4(a_position, 1.0);
    v_color = a_color;
}
@end

@fs editor_grid_fs
layout(location=0) in vec4 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = v_color;
}
@end

@program editor_grid editor_grid_vs editor_grid_fs
