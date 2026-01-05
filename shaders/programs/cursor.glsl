@vs cursor_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec4 a_color;

layout(std140, binding=0) uniform cursor_vs_params {
    vec2 u_screen_size;
};

layout(location=0) out vec4 v_color;

void main() {
    vec2 pos = (a_position / u_screen_size) * 2.0 - 1.0;
    pos.y = -pos.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    v_color = a_color;
}
@end

@fs cursor_fs
layout(location=0) in vec4 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = v_color;
}
@end

@program cursor cursor_vs cursor_fs
