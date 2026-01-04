@vs track_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_texcoord;
layout(location=2) in float a_strength;

layout(location=0) out vec2 v_texcoord;
layout(location=1) out float v_strength;

void main() {
    vec2 ndc = a_position * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_strength = a_strength;
}
@end

@fs track_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;

layout(std140, binding=0) uniform track_fs_params {
    vec4 u_color;
    int u_use_texture;
};

layout(location=0) in vec2 v_texcoord;
layout(location=1) in float v_strength;
layout(location=0) out vec4 frag_color;

void main() {
    vec4 color = u_color;

    if (u_use_texture != 0) {
        vec4 tex = texture(sampler2D(u_texture, u_texture_smp), v_texcoord);
        color.a *= tex.a;
    }

    color.a *= v_strength;
    frag_color = color;
}
@end

@program track track_vs track_fs

