// ============================================================================
// UI Textured Shader - For rendering textured 2D UI elements
// ============================================================================

@vs ui_textured_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_texcoord;
layout(location=2) in vec4 a_color;

layout(std140, binding=0) uniform ui_textured_vs_params {
    mat4 u_projection;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}
@end

@fs ui_textured_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;

layout(location=0) in vec2 v_texcoord;
layout(location=1) in vec4 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    vec4 tex_color = texture(sampler2D(u_texture, u_texture_smp), v_texcoord);
    frag_color = tex_color * v_color;
}
@end

@program ui_textured ui_textured_vs ui_textured_fs
