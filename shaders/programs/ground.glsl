@vs ground_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform ground_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out vec3 v_world_pos;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_world_pos = a_position;
}
@end

@fs ground_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;
layout(binding=1) uniform texture2D u_track_texture;
layout(binding=1) uniform sampler u_track_texture_smp;
layout(binding=2) uniform texture2D u_light_texture;
layout(binding=2) uniform sampler u_light_texture_smp;

layout(std140, binding=1) uniform ground_fs_params {
    int u_use_tracks;
    int u_use_lighting;
    vec2 u_track_scale;
    vec2 u_track_offset;
    vec2 u_light_scale;
    vec2 u_light_offset;
    int u_has_sun;
    vec3 u_sun_direction;
    vec3 u_sun_color;
};

layout(location=0) in vec2 v_texcoord;
layout(location=1) in vec3 v_world_pos;
layout(location=0) out vec4 frag_color;

void main() {
    vec4 terrain = texture(sampler2D(u_texture, u_texture_smp), v_texcoord);

    if (u_use_tracks != 0) {
        vec2 track_uv = v_world_pos.xz * u_track_scale + u_track_offset;
        vec4 tracks
            = texture(sampler2D(u_track_texture, u_track_texture_smp), track_uv);
        terrain.rgb *= tracks.rgb;
    }

    vec3 final_light = vec3(1.0);

    if (u_has_sun != 0) {
        float sun_intensity = max(0.0, -u_sun_direction.y);
        final_light = u_sun_color * sun_intensity;
    }

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

        if (u_has_sun != 0) {
            final_light += dynamic_light * 0.5;
        } else {
            final_light = dynamic_light;
        }
    }

    terrain.rgb *= final_light;
    frag_color = terrain;
}
@end

@program ground ground_vs ground_fs

