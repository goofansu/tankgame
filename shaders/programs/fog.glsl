// =============================================================================
// Fog shader - translucent ground fog with subtle motion
// =============================================================================

@vs fog_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform fog_vs_params {
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

@fs fog_fs
layout(binding=0) uniform texture2D u_fog_track_texture;
layout(binding=0) uniform sampler u_fog_track_texture_smp;

layout(std140, binding=1) uniform fog_fs_params {
    float u_time;
    float u_fog_alpha;
    int u_use_tracks;
    vec2 u_track_scale;
    vec2 u_track_offset;
    vec3 u_fog_color;
    int u_fog_disturb_count;
    float u_fog_disturb_strength;
    vec4 u_fog_disturb0;
    vec4 u_fog_disturb1;
    vec4 u_fog_disturb2;
    vec4 u_fog_disturb3;
    vec4 u_fog_disturb4;
    vec4 u_fog_disturb5;
    vec4 u_fog_disturb6;
    vec4 u_fog_disturb7;
};

layout(location=0) in vec2 v_texcoord;
layout(location=1) in vec3 v_world_pos;
layout(location=0) out vec4 frag_color;

void main() {
    float noise = sin(v_world_pos.x * 1.7 + u_time * 0.6);
    noise += sin(v_world_pos.z * 1.3 - u_time * 0.4);
    noise += sin((v_texcoord.x + v_texcoord.y) * 6.283 + u_time * 0.2) * 0.05;
    noise = noise * 0.25 + 0.5;

    float alpha = u_fog_alpha * (0.65 + 0.35 * noise);
    vec3 fog_color = u_fog_color;
    float disturb = 0.0;

    if (u_fog_disturb_count > 0) {
        vec4 d0 = u_fog_disturb0;
        vec4 d1 = u_fog_disturb1;
        vec4 d2 = u_fog_disturb2;
        vec4 d3 = u_fog_disturb3;
        vec4 d4 = u_fog_disturb4;
        vec4 d5 = u_fog_disturb5;
        vec4 d6 = u_fog_disturb6;
        vec4 d7 = u_fog_disturb7;

        vec4 d[8] = vec4[8](d0, d1, d2, d3, d4, d5, d6, d7);
        for (int i = 0; i < 8; i++) {
            if (i >= u_fog_disturb_count) {
                break;
            }
            float dist = length(v_world_pos.xz - d[i].xz);
            float influence = smoothstep(d[i].w, 0.0, dist) * d[i].y;
            disturb = max(disturb, influence);
        }
    }

    if (u_use_tracks != 0) {
        vec2 track_uv = v_world_pos.xz * u_track_scale + u_track_offset;
        float track = texture(
            sampler2D(u_fog_track_texture, u_fog_track_texture_smp), track_uv)
                          .r;
        float track_disturb = clamp(1.0 - track, 0.0, 1.0);
        disturb = max(disturb, track_disturb);
    }

    if (disturb > 0.0) {
        float ripple = sin(v_world_pos.x * 5.0 + v_world_pos.z * 4.0
                         + u_time * 2.0);
        alpha *= 1.0 - disturb * (u_fog_disturb_strength + 0.2 * ripple);
        fog_color = mix(fog_color, vec3(1.0), disturb * 0.3);
    }

    frag_color = vec4(fog_color, alpha);
}
@end

@program fog fog_vs fog_fs

