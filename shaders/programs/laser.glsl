@vs laser_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform laser_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec2 v_texcoord;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
}
@end

@fs laser_fs
layout(std140, binding=1) uniform laser_fs_params {
    vec4 u_color;
};

layout(location=0) in vec2 v_texcoord;
layout(location=0) out vec4 frag_color;

void main() {
    float edge_fade = 1.0 - abs(v_texcoord.x * 2.0 - 1.0);
    edge_fade = pow(edge_fade, 0.5);

    float length_fade = 1.0 - v_texcoord.y * 0.3;

    float alpha = u_color.a * edge_fade * length_fade;
    frag_color = vec4(u_color.rgb, alpha);
}
@end

@program laser laser_vs laser_fs

