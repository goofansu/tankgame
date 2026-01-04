@vs test_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_color;

layout(std140, binding=0) uniform test_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec3 v_color;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
@end

@fs test_fs
layout(location=0) in vec3 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = vec4(v_color, 1.0);
}
@end

@program test test_vs test_fs

