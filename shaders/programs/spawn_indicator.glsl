// ============================================================================
// Spawn Indicator Shader - For rendering 2D spawn indicators
// ============================================================================

@vs spawn_indicator_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec4 a_color;

layout(std140, binding=0) uniform spawn_indicator_vs_params {
    mat4 u_projection;
};

layout(location=0) out vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_color = a_color;
}
@end

@fs spawn_indicator_fs
layout(location=0) in vec4 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = v_color;
}
@end

@program spawn_indicator spawn_indicator_vs spawn_indicator_fs
