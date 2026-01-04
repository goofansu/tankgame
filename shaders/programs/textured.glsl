@vs textured_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform textured_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec2 v_texcoord;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
}
@end

@fs textured_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;

layout(location=0) in vec2 v_texcoord;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(u_texture, u_texture_smp), v_texcoord);
}
@end

@program textured textured_vs textured_fs

