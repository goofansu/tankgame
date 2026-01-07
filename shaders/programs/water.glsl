// =============================================================================
// Water shader - animated water with caustic lines
// =============================================================================

@vs water_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

layout(std140, binding=0) uniform water_vs_params {
    mat4 u_mvp;
    float u_wave_time;
    float u_wave_strength;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out vec3 v_world_pos;

void main() {
    float wave = 0.0;
    wave += sin(a_position.x * 0.7 + u_wave_time * 0.9) * 0.12;
    wave += sin(a_position.z * 1.1 + u_wave_time * 1.2) * 0.10;
    wave += sin((a_position.x + a_position.z) * 0.4 + u_wave_time * 0.6) * 0.07;
    wave *= u_wave_strength;

    vec3 displaced_pos = a_position;
    displaced_pos.y += wave;

    gl_Position = u_mvp * vec4(displaced_pos, 1.0);
    v_texcoord = a_texcoord;
    v_world_pos = displaced_pos;
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
    vec2 u_wind_dir;  // Normalized wind direction (x, z)
    float u_wind_strength;  // Wind strength multiplier
    float u_alpha;  // Water opacity (1.0 = opaque, 0.5 = translucent for editor)
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
    // Wind-driven UV offset - texture moves in wind direction
    float wind_speed = 0.05 * u_wind_strength;
    vec2 wind_offset = u_wind_dir * u_time * wind_speed;
    
    // Upper layer UV (bright caustics) - animated distortion like reference shader
    // Scale 0.23 (0.15 / 0.65) gives smaller caustic cells (65% of previous size)
    vec2 uv = v_world_pos.xz * 0.23 + wind_offset;
    
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
    
    frag_color = vec4(color, u_alpha);
}
@end

@program water water_vs water_fs

