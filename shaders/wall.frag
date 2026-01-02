#version 330 core

in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

uniform sampler2D u_texture_top;
uniform sampler2D u_texture_side;
uniform sampler2D u_light_texture;
uniform vec3 u_light_dir;      // Legacy directional light (unused now)
uniform vec3 u_light_color;    // Legacy light color (unused now)
uniform vec3 u_ambient;
uniform int u_use_lighting;
uniform vec2 u_light_scale;
uniform vec2 u_light_offset;

// Sun lighting uniforms
uniform int u_has_sun;
uniform vec3 u_sun_direction;  // Direction FROM sun (normalized)
uniform vec3 u_sun_color;

void main()
{
    // Determine if this is a top or side face based on normal
    vec3 n = normalize(v_normal);
    float is_top = step(0.5, n.y);  // 1.0 if top face, 0.0 otherwise
    
    // Sample appropriate texture
    vec4 tex_top = texture(u_texture_top, v_texcoord);
    vec4 tex_side = texture(u_texture_side, v_texcoord);
    vec4 tex_color = mix(tex_side, tex_top, is_top);
    
    // Calculate lighting
    vec3 lighting = vec3(0.0);
    
    if (is_top > 0.5) {
        // TOP FACE: Use sun lighting or ambient
        if (u_has_sun != 0) {
            // Sun lighting based on normal dot product
            // sun_direction points FROM sun, so we negate it
            float sun_intensity = max(0.0, dot(n, -u_sun_direction));
            lighting = u_sun_color * sun_intensity;
            // Add some ambient so shadows aren't pitch black
            lighting += u_ambient * 0.3;
        } else {
            // Night mode: wall tops use ambient only (no dynamic light)
            lighting = u_ambient;
        }
    } else {
        // SIDE FACE: Use dynamic light map
        if (u_use_lighting != 0) {
            // Sample light map at base of wall (y=0 in world space)
            vec2 light_uv = v_world_pos.xz * u_light_scale + u_light_offset;
            
            // Sample with box blur to soften low-resolution light map edges
            vec2 texel_size = vec2(1.0 / 256.0);
            vec3 dynamic_light = vec3(0.0);
            float total_weight = 0.0;
            
            for (int y = -2; y <= 2; y++) {
                for (int x = -2; x <= 2; x++) {
                    vec2 offset = vec2(float(x), float(y)) * texel_size;
                    float weight = 1.0 / (1.0 + float(abs(x) + abs(y)));
                    dynamic_light += texture(u_light_texture, light_uv + offset).rgb * weight;
                    total_weight += weight;
                }
            }
            dynamic_light /= total_weight;
            
            // Apply normal-based shading to dynamic light
            // Sides facing the light get more illumination
            float side_factor = 0.7 + 0.3 * max(0.0, dot(n.xz, normalize(vec2(0.4, 0.3))));
            lighting = dynamic_light * side_factor;
            
            // In day mode, also add sun influence on sides
            if (u_has_sun != 0) {
                float sun_side = max(0.0, dot(n, -u_sun_direction));
                lighting += u_sun_color * sun_side * 0.3;
            }
        } else {
            // No light map, use ambient
            lighting = u_ambient;
        }
    }
    
    frag_color = vec4(tex_color.rgb * lighting, tex_color.a);
}
