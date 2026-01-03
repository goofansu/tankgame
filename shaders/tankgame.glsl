@module tankgame

@vs test_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_color;

layout(std140, binding=0) uniform test_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec3 v_color;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
@end

@fs test_fs
layout(location=0) in vec3 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = vec4(v_color, 1.0);
}
@end

@program test test_vs test_fs

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

// =============================================================================
// Water shader - animated water with caustic lines
// =============================================================================

@vs water_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform water_vs_params {
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

@fs water_fs
layout(binding=0) uniform texture2D u_water_light_texture;
layout(binding=0) uniform sampler u_water_light_texture_smp;
layout(binding=1) uniform texture2D u_water_caustic_texture;
layout(binding=1) uniform sampler u_water_caustic_texture_smp;

layout(std140, binding=1) uniform water_fs_params {
    float u_time;
    int u_use_lighting;
    vec2 u_light_scale;
    vec2 u_light_offset;
    vec3 u_water_color;
    vec3 u_water_dark;
    vec3 u_water_highlight;
};

layout(location=0) in vec2 v_texcoord;
layout(location=1) in vec3 v_world_pos;
layout(location=0) out vec4 frag_color;

#define SCALE 4.0

float calculateSurface(float x, float z, float t) {
    float y = 0.0;
    y += (sin(x * 1.0 / SCALE + t * 0.5) + sin(x * 2.3 / SCALE + t * 0.75) + sin(x * 3.3 / SCALE + t * 0.2)) / 3.0 * 0.3;
    y += (sin(z * 0.2 / SCALE + t * 0.9) + sin(z * 1.8 / SCALE + t * 0.9) + sin(z * 2.8 / SCALE + t * 0.4)) / 3.0 * 0.3;
    return y;
}

void main() {
    // Upper layer UV (bright caustics) - animated distortion like reference shader
    // Scale 0.23 (0.15 / 0.65) gives smaller caustic cells (65% of previous size)
    vec2 uv = v_world_pos.xz * 0.23 + vec2(u_time * -0.02);
    
    uv.y += 0.01 * (sin(uv.x * 3.5 + u_time * 0.35) + sin(uv.x * 4.8 + u_time * 1.05) + sin(uv.x * 7.3 + u_time * 0.45)) / 3.0;
    uv.x += 0.12 * (sin(uv.y * 4.0 + u_time * 0.5) + sin(uv.y * 6.8 + u_time * 0.75) + sin(uv.y * 11.3 + u_time * 0.2)) / 3.0;
    uv.y += 0.12 * (sin(uv.x * 4.2 + u_time * 0.64) + sin(uv.x * 6.3 + u_time * 1.65) + sin(uv.x * 8.2 + u_time * 0.45)) / 3.0;
    
    // Lower layer UV (dark caustics) - offset and slower drift like reference shader
    vec2 uv2 = uv + vec2(0.2);
    uv2 += 0.02 * vec2(sin(u_time * 0.07), cos(u_time * 0.05));
    
    // Sample caustic texture for both layers (use alpha channel for caustic intensity)
    vec4 tex1 = texture(sampler2D(u_water_caustic_texture, u_water_caustic_texture_smp), uv);
    vec4 tex2 = texture(sampler2D(u_water_caustic_texture, u_water_caustic_texture_smp), uv2);
    
    float wave = calculateSurface(v_world_pos.x, v_world_pos.z, u_time);
    float waveShade = 0.5 + wave * 0.5;
    
    // Auto-calculate lighter and darker colors from base water color
    // Light color: brighten toward white while preserving hue
    vec3 lightColor = mix(u_water_color, vec3(1.0), 0.6);
    // Dark color: darken the base color
    vec3 darkColor = u_water_color * 0.4;
    
    // Combine: base color + bright upper layer - dark lower layer (like reference shader)
    vec3 color = u_water_color;
    color += lightColor * tex1.a * 0.7;   // Upper bright caustic lines
    color -= darkColor * tex2.a * 0.3;    // Lower dark caustic shadows (subtractive)
    color *= (0.85 + waveShade * 0.15);
    
    if (u_use_lighting != 0) {
        vec2 light_uv = v_world_pos.xz * u_light_scale + u_light_offset;
        vec2 texel_size = vec2(1.0 / 256.0);
        vec3 dynamic_light = vec3(0.0);
        float total_weight = 0.0;
        for (int y = -2; y <= 2; y++) {
            for (int x = -2; x <= 2; x++) {
                vec2 offset = vec2(float(x), float(y)) * texel_size;
                float weight = 1.0 / (1.0 + float(abs(x) + abs(y)));
                dynamic_light += texture(sampler2D(u_water_light_texture, u_water_light_texture_smp), light_uv + offset).rgb * weight;
                total_weight += weight;
            }
        }
        dynamic_light /= total_weight;
        
        // Use luminance of dynamic light to brighten/darken water
        // This preserves the water's hue while still responding to lighting
        float lightIntensity = dot(dynamic_light, vec3(0.299, 0.587, 0.114));
        color *= (0.3 + lightIntensity * 0.9); // Base ambient + light contribution
    } else {
        // Day mode (no dynamic lighting) - apply bright ambient
        color *= 1.0;
    }
    
    frag_color = vec4(color, 1.0);
}
@end

@program water water_vs water_fs

@vs wall_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec2 a_texcoord;

layout(std140, binding=0) uniform wall_vs_params {
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
};

layout(location=0) in vec3 v_normal;
layout(location=1) in vec2 v_texcoord;
layout(location=2) in vec3 v_world_pos;
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

    frag_color = vec4(tex_color.rgb * lighting, tex_color.a);
}
@end

@program wall wall_vs wall_fs

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

    frag_color = vec4(u_color.rgb * lighting, u_color.a);
}
@end

@program entity entity_vs entity_fs
@program tank entity_vs entity_fs
@program projectile entity_vs entity_fs
@program powerup entity_vs entity_fs

@vs track_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_texcoord;

layout(location=0) out vec2 v_texcoord;

void main() {
    vec2 ndc = a_position * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_texcoord = a_texcoord;
}
@end

@fs track_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;

layout(std140, binding=0) uniform track_fs_params {
    vec4 u_color;
    int u_use_texture;
};

layout(location=0) in vec2 v_texcoord;
layout(location=0) out vec4 frag_color;

void main() {
    vec4 color = u_color;

    if (u_use_texture != 0) {
        vec4 tex = texture(sampler2D(u_texture, u_texture_smp), v_texcoord);
        color.a *= tex.a;
    }

    frag_color = color;
}
@end

@program track track_vs track_fs

@vs lightmap_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec3 a_color;
layout(location=2) in float a_intensity;

layout(location=0) out vec3 v_color;
layout(location=1) out float v_base_intensity;
layout(location=2) out vec2 v_uv_position;

void main() {
    vec2 ndc = a_position * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
    v_base_intensity = a_intensity;
    v_uv_position = a_position;
}
@end

@fs lightmap_fs
layout(std140, binding=0) uniform lightmap_fs_params {
    vec2 u_light_center_uv;
    float u_light_radius;
    vec2 u_world_size;
};

layout(location=0) in vec3 v_color;
layout(location=1) in float v_base_intensity;
layout(location=2) in vec2 v_uv_position;
layout(location=0) out vec4 frag_color;

void main() {
    vec2 uv_delta = v_uv_position - u_light_center_uv;
    vec2 world_delta = uv_delta * u_world_size;
    float dist = length(world_delta);

    float dist_factor = 1.0 - (dist / u_light_radius);
    dist_factor = clamp(dist_factor, 0.0, 1.0);
    dist_factor = dist_factor * dist_factor;

    float intensity = v_base_intensity * dist_factor;
    frag_color = vec4(v_color * intensity, 1.0);
}
@end

@program lightmap lightmap_vs lightmap_fs

@vs particle_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform particle_vs_params {
    mat4 u_mvp;
    float u_alpha;
    vec3 u_color;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out float v_alpha;
layout(location=2) out vec3 v_color;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_alpha = u_alpha;
    v_color = u_color;
}
@end

@fs particle_fs
layout(binding=0) uniform texture2D u_texture;
layout(binding=0) uniform sampler u_texture_smp;

layout(location=0) in vec2 v_texcoord;
layout(location=1) in float v_alpha;
layout(location=2) in vec3 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    vec4 tex = texture(sampler2D(u_texture, u_texture_smp), v_texcoord);

    if (tex.a < 0.01) {
        discard;
    }

    float brightness = tex.r;
    vec3 final_color;
    if (brightness > 0.7) {
        final_color = v_color;
    } else if (brightness > 0.35) {
        final_color = v_color * 0.75;
    } else {
        final_color = v_color * 0.5;
    }

    float final_alpha = tex.a * v_alpha;
    frag_color = vec4(final_color, final_alpha);
}
@end

@program particle particle_vs particle_fs

@vs laser_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform laser_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec2 v_texcoord;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
}
@end

@fs laser_fs
layout(std140, binding=1) uniform laser_fs_params {
    vec4 u_color;
};

layout(location=0) in vec2 v_texcoord;
layout(location=0) out vec4 frag_color;

void main() {
    float edge_fade = 1.0 - abs(v_texcoord.x * 2.0 - 1.0);
    edge_fade = pow(edge_fade, 0.5);

    float length_fade = 1.0 - v_texcoord.y * 0.3;

    float alpha = u_color.a * edge_fade * length_fade;
    frag_color = vec4(u_color.rgb, alpha);
}
@end

@program laser laser_vs laser_fs

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

@vs debug_line_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec4 a_color;

layout(std140, binding=0) uniform debug_line_vs_params {
    vec2 u_screen_size;
};

layout(location=0) out vec4 v_color;

void main() {
    vec2 pos = (a_position / u_screen_size) * 2.0 - 1.0;
    pos.y = -pos.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    v_color = a_color;
}
@end

@fs debug_line_fs
layout(location=0) in vec4 v_color;
layout(location=0) out vec4 frag_color;

void main() {
    frag_color = v_color;
}
@end

@program debug_line debug_line_vs debug_line_fs

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

// ============================================================================
// Background Shader
// Renders a fullscreen quad with solid color, vertical gradient, or radial gradient
// ============================================================================

@vs background_vs
layout(location=0) in vec2 a_position;

layout(location=0) out vec2 v_uv;

void main() {
    // a_position is in range [-1, 1]
    gl_Position = vec4(a_position, 0.9999, 1.0);  // Far back in depth
    // UV: (0,0) at bottom-left, (1,1) at top-right
    v_uv = a_position * 0.5 + 0.5;
}
@end

@fs background_fs
layout(std140, binding=0) uniform background_fs_params {
    vec3 u_color_start;     // Top color (vertical) or center color (radial)
    vec3 u_color_end;       // Bottom color (vertical) or edge color (radial)
    int u_gradient_type;    // 0 = solid, 1 = vertical, 2 = radial
    vec2 u_aspect;          // (1.0, height/width) for radial to be circular
};

layout(location=0) in vec2 v_uv;
layout(location=0) out vec4 frag_color;

void main() {
    vec3 color;
    
    if (u_gradient_type == 0) {
        // Solid color
        color = u_color_start;
    } else if (u_gradient_type == 1) {
        // Vertical gradient (top to bottom)
        // v_uv.y = 1 at top, 0 at bottom
        float t = v_uv.y;
        color = mix(u_color_end, u_color_start, t);
    } else {
        // Radial gradient (center to edge)
        vec2 centered = (v_uv - 0.5) * 2.0;  // Range [-1, 1]
        centered *= u_aspect;  // Correct for aspect ratio
        float dist = length(centered);
        float t = clamp(dist, 0.0, 1.0);
        color = mix(u_color_start, u_color_end, t);
    }
    
    frag_color = vec4(color, 1.0);
}
@end

@program background background_vs background_fs
