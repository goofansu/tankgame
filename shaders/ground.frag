#version 330 core

// Ground rendering fragment shader
// Samples terrain texture, track accumulation texture, and light map
// Supports both sun lighting (day) and dynamic-only lighting (night)

in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

uniform sampler2D u_texture;       // Terrain texture
uniform sampler2D u_track_texture; // Track accumulation texture
uniform sampler2D u_light_texture; // Light map texture
uniform int u_use_tracks;          // Whether track texture is bound
uniform int u_use_lighting;        // Whether light map is bound
uniform vec2 u_track_scale;        // World-to-UV scale for tracks
uniform vec2 u_track_offset;       // World-to-UV offset for tracks
uniform vec2 u_light_scale;        // World-to-UV scale for light map
uniform vec2 u_light_offset;       // World-to-UV offset for light map

// Sun lighting uniforms
uniform int u_has_sun;             // Whether sun is enabled
uniform vec3 u_sun_direction;      // Direction FROM sun (normalized)
uniform vec3 u_sun_color;          // Sun light color

void main()
{
    // Sample terrain texture
    vec4 terrain = texture(u_texture, v_texcoord);
    
    // Sample track texture if available
    if (u_use_tracks != 0) {
        // Convert world XZ to track texture UV
        vec2 track_uv = v_world_pos.xz * u_track_scale + u_track_offset;
        vec4 tracks = texture(u_track_texture, track_uv);
        
        // Multiply terrain color by track color (tracks darken the ground)
        // The track texture is white where no tracks, darker where tracks are
        terrain.rgb *= tracks.rgb;
    }
    
    // Calculate lighting
    vec3 final_light = vec3(1.0);
    
    // Sun lighting for ground (ground normal is always up: 0, 1, 0)
    if (u_has_sun != 0) {
        // Ground is lit by sun based on how much the sun points down
        // sun_direction points FROM sun, so -sun_direction.y is how much it shines down
        float sun_intensity = max(0.0, -u_sun_direction.y);
        final_light = u_sun_color * sun_intensity;
    }
    
    // Apply dynamic lighting from light map (additive for lights, or base for night)
    if (u_use_lighting != 0) {
        // Convert world XZ to light map UV
        vec2 light_uv = v_world_pos.xz * u_light_scale + u_light_offset;
        
        // Sample with box blur to soften low-resolution light map edges
        vec2 texel_size = vec2(1.0 / 256.0);
        vec3 dynamic_light = vec3(0.0);
        float total_weight = 0.0;
        
        // 5x5 box blur with gaussian-ish weights
        for (int y = -2; y <= 2; y++) {
            for (int x = -2; x <= 2; x++) {
                vec2 offset = vec2(float(x), float(y)) * texel_size;
                float weight = 1.0 / (1.0 + float(abs(x) + abs(y)));
                dynamic_light += texture(u_light_texture, light_uv + offset).rgb * weight;
                total_weight += weight;
            }
        }
        dynamic_light /= total_weight;
        
        if (u_has_sun != 0) {
            // Day mode: add dynamic lights on top of sun
            final_light += dynamic_light * 0.5; // Reduce dynamic light contribution in day
        } else {
            // Night mode: dynamic lights are the primary source
            final_light = dynamic_light;
        }
    }
    
    // Apply lighting to terrain
    terrain.rgb *= final_light;
    
    frag_color = terrain;
}
