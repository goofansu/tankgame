@vs particle_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform particle_vs_params {
    mat4 u_mvp;
    vec3 u_world_pos;      // World position of particle center
    float u_alpha;
    vec3 u_color;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out float v_alpha;
layout(location=2) out vec3 v_color;
layout(location=3) out vec3 v_world_pos;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_alpha = u_alpha;
    v_color = u_color;
    v_world_pos = u_world_pos;
}
@end

@fs particle_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;
layout(binding=2) uniform texture2D u_light_texture;
layout(binding=2) uniform sampler u_light_texture_smp;

layout(std140, binding=1) uniform particle_fs_params {
    vec3 u_ambient;
    int u_use_lighting;
    vec2 u_light_scale;
    vec2 u_light_offset;
};

layout(location=0) in vec2 v_texcoord;
layout(location=1) in float v_alpha;
layout(location=2) in vec3 v_color;
layout(location=3) in vec3 v_world_pos;
layout(location=0) out vec4 frag_color;

void main() {
    vec4 tex = texture(sampler2D(u_texture, u_texture_smp), v_texcoord);

    if (tex.a < 0.01) {
        discard;
    }

    float brightness = tex.r;
    vec3 base_color;
    if (brightness > 0.7) {
        base_color = v_color;
    } else if (brightness > 0.35) {
        base_color = v_color * 0.75;
    } else {
        base_color = v_color * 0.5;
    }

    // Apply lighting
    vec3 lighting = u_ambient;
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
                    light_uv + offset).rgb * weight;
                total_weight += weight;
            }
        }
        dynamic_light /= total_weight;
        lighting = u_ambient + dynamic_light;
    }

    vec3 final_color = base_color * lighting;
    float final_alpha = tex.a * v_alpha;
    frag_color = vec4(final_color, final_alpha);
}
@end

@program particle particle_vs particle_fs

