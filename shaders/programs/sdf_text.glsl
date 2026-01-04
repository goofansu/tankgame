@vs sdf_text_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_texcoord;
layout(location=2) in vec4 a_color;
layout(location=3) in vec4 a_outline_color;
layout(location=4) in float a_outline_width;

layout(std140, binding=0) uniform sdf_text_vs_params {
    vec2 u_screen_size;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out vec4 v_color;
layout(location=2) out vec4 v_outline_color;
layout(location=3) out float v_outline_width;

void main() {
    vec2 pos = (a_position / u_screen_size) * 2.0 - 1.0;
    pos.y = -pos.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
    v_outline_color = a_outline_color;
    v_outline_width = a_outline_width;
}
@end

@fs sdf_text_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;

layout(location=0) in vec2 v_texcoord;
layout(location=1) in vec4 v_color;
layout(location=2) in vec4 v_outline_color;
layout(location=3) in float v_outline_width;
layout(location=0) out vec4 frag_color;

void main() {
    float distance = texture(sampler2D(u_texture, u_texture_smp), v_texcoord).r;

    // SDF rendering with smooth edges
    // 0.5 is the edge (128/255 in our SDF)
    float edge = 0.5;

    // Calculate anti-aliasing width based on screen-space derivatives
    float aa_width = fwidth(distance);

    // Fill alpha (inside the glyph)
    float fill_alpha = smoothstep(edge - aa_width, edge + aa_width, distance);

    // Outline support
    if (v_outline_width > 0.0) {
        // Outline edge is further out (lower distance value)
        float outline_edge = edge - v_outline_width;
        float outline_alpha = smoothstep(outline_edge - aa_width, outline_edge + aa_width, distance);

        if (outline_alpha < 0.01) {
            discard;
        }

        // Blend between outline color (outside) and fill color (inside)
        vec3 color = mix(v_outline_color.rgb, v_color.rgb, fill_alpha);
        float alpha = outline_alpha * mix(v_outline_color.a, v_color.a, fill_alpha);

        frag_color = vec4(color, alpha);
    } else {
        // No outline - simple fill
        if (fill_alpha < 0.01) {
            discard;
        }
        frag_color = vec4(v_color.rgb, v_color.a * fill_alpha);
    }
}
@end

@program sdf_text sdf_text_vs sdf_text_fs

