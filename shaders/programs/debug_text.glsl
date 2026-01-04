@vs debug_text_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_texcoord;
layout(location=2) in vec4 a_color;

layout(std140, binding=0) uniform debug_text_vs_params {
    vec2 u_screen_size;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out vec4 v_color;

void main() {
    vec2 pos = (a_position / u_screen_size) * 2.0 - 1.0;
    pos.y = -pos.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}
@end

@fs debug_text_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;

layout(location=0) in vec2 v_texcoord;
layout(location=1) in vec4 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    float alpha = texture(sampler2D(u_texture, u_texture_smp), v_texcoord).r;
    frag_color = vec4(v_color.rgb, v_color.a * alpha);
}
@end

@program debug_text debug_text_vs debug_text_fs

