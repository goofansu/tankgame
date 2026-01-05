@vs wall_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec2 a_texcoord;
layout(location=3) in float a_ao;

layout(std140, binding=0) uniform wall_vs_params {
    mat4 u_mvp;
    mat4 u_model;
};

layout(location=0) out vec3 v_normal;
layout(location=1) out vec2 v_texcoord;
layout(location=2) out vec3 v_world_pos;
layout(location=3) out float v_ao;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_normal = mat3(u_model) * a_normal;
    v_texcoord = a_texcoord;
    v_world_pos = (u_model * vec4(a_position, 1.0)).xyz;
    v_ao = a_ao;
}
@end

@fs wall_fs
layout(binding=0) uniform texture2D u_texture_top;
layout(binding=0) uniform sampler u_texture_top_smp;
layout(binding=1) uniform texture2D u_texture_side;
layout(binding=1) uniform sampler u_texture_side_smp;
layout(binding=2) uniform texture2D u_light_texture;
layout(binding=2) uniform sampler u_light_texture_smp;

layout(std140, binding=1) uniform wall_fs_params {
    vec3 u_light_dir;
    vec3 u_light_color;
    vec3 u_ambient;
    int u_use_lighting;
    vec2 u_light_scale;
    vec2 u_light_offset;
    int u_has_sun;
    vec3 u_sun_direction;
    vec3 u_sun_color;
    vec4 u_tint;
};

layout(location=0) in vec3 v_normal;
layout(location=1) in vec2 v_texcoord;
layout(location=2) in vec3 v_world_pos;
layout(location=3) in float v_ao;
layout(location=0) out vec4 frag_color;

void main() {
    vec3 n = normalize(v_normal);
    float is_top = step(0.5, n.y);

    vec4 tex_top
        = texture(sampler2D(u_texture_top, u_texture_top_smp), v_texcoord);
    vec4 tex_side
        = texture(sampler2D(u_texture_side, u_texture_side_smp), v_texcoord);
    vec4 tex_color = mix(tex_side, tex_top, is_top);

    vec3 lighting = vec3(0.0);

    if (is_top > 0.5) {
        if (u_has_sun != 0) {
            float sun_intensity = max(0.0, dot(n, -u_sun_direction));
            lighting = u_sun_color * sun_intensity;
            lighting += u_ambient * 0.3;
        } else {
            lighting = u_ambient;
        }
    } else {
        if (u_use_lighting != 0) {
            vec2 light_uv = v_world_pos.xz * u_light_scale + u_light_offset;
            vec2 texel_size = vec2(1.0 / 256.0);
            vec3 dynamic_light = vec3(0.0);
            float total_weight = 0.0;

            for (int y = -2; y <= 2; y++) {
                for (int x = -2; x <= 2; x++) {
                    vec2 offset = vec2(float(x), float(y)) * texel_size;
                    float weight = 1.0 / (1.0 + float(abs(x) + abs(y)));
                    dynamic_light += texture(
                        sampler2D(u_light_texture, u_light_texture_smp),
                        light_uv + offset)
                        .rgb
                        * weight;
                    total_weight += weight;
                }
            }
            dynamic_light /= total_weight;

            float side_factor
                = 0.7 + 0.3 * max(0.0, dot(n.xz, normalize(vec2(0.4, 0.3))));
            lighting = dynamic_light * side_factor;

            if (u_has_sun != 0) {
                float sun_side = max(0.0, dot(n, -u_sun_direction));
                lighting += u_sun_color * sun_side * 0.3;
            }
        } else {
            lighting = u_ambient;
        }
    }

    float ao = pow(clamp(v_ao, 0.0, 1.0), 1.2);
    ao = mix(1.0, ao, 0.5);
    vec3 final_color = tex_color.rgb * (lighting * ao) * u_tint.rgb;
    frag_color = vec4(final_color, tex_color.a * u_tint.a);
}
@end

@program wall wall_vs wall_fs

