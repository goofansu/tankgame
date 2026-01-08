// Radial glow disc shader - soft feathered circle
// Used for player highlight

@vs glow_disc_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform glow_disc_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec2 v_uv;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_uv = a_texcoord;
}
@end

@fs glow_disc_fs
layout(std140, binding=1) uniform glow_disc_fs_params {
    vec4 u_color;
};

layout(location=0) in vec2 v_uv;
layout(location=0) out vec4 frag_color;

void main() {
    vec2 centered = v_uv * 2.0 - 1.0;
    float dist = length(centered);
    float falloff = smoothstep(1.0, 0.0, dist);
    falloff = falloff * falloff;
    frag_color = vec4(u_color.rgb, u_color.a * falloff);
}
@end

@program glow_disc glow_disc_vs glow_disc_fs
