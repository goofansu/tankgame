@vs entity_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec2 a_texcoord;

layout(std140, binding=0) uniform entity_vs_params {
    mat4 u_mvp;
    mat4 u_model;
};

layout(location=0) out vec3 v_normal;
layout(location=1) out vec2 v_texcoord;
layout(location=2) out vec3 v_world_pos;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_normal = mat3(u_model) * a_normal;
    v_texcoord = a_texcoord;
    v_world_pos = (u_model * vec4(a_position, 1.0)).xyz;
}
@end

@fs entity_fs
layout(binding=0) uniform texture2D u_entity_light_texture;
layout(binding=0) uniform sampler u_entity_light_texture_smp;

layout(std140, binding=1) uniform entity_fs_params {
    vec4 u_color;
    vec3 u_light_dir;
    vec3 u_light_color;
    vec3 u_ambient;
    int u_use_lighting;
    vec2 u_light_scale;
    vec2 u_light_offset;
    vec2 u_shadow_params; // x = softness, y = use_falloff
};

layout(location=0) in vec3 v_normal;
layout(location=1) in vec2 v_texcoord;
layout(location=2) in vec3 v_world_pos;
layout(location=0) out vec4 frag_color;

void main() {
    vec3 n = normalize(v_normal);
    vec3 lighting;

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
                    sampler2D(u_entity_light_texture, u_entity_light_texture_smp),
                    light_uv + offset)
                    .rgb
                    * weight;
                total_weight += weight;
            }
        }
        dynamic_light /= total_weight;

        float ndotl = max(dot(n, normalize(u_light_dir)), 0.0);
        float shading = 0.6 + 0.4 * ndotl;

        lighting = dynamic_light * shading;
    } else {
        float ndotl = max(dot(n, normalize(u_light_dir)), 0.0);
        vec3 diffuse = u_light_color * ndotl;
        lighting = u_ambient + diffuse;
    }

    float shadow_alpha = u_color.a;
    if (u_shadow_params.y > 0.5) {
        float edge = min(min(v_texcoord.x, 1.0 - v_texcoord.x),
            min(v_texcoord.y, 1.0 - v_texcoord.y));
        float falloff = smoothstep(0.0, u_shadow_params.x, edge);
        shadow_alpha *= falloff;
    }

    frag_color = vec4(u_color.rgb * lighting, shadow_alpha);
}
@end

@program entity entity_vs entity_fs
@program tank entity_vs entity_fs
@program projectile entity_vs entity_fs
@program powerup entity_vs entity_fs

