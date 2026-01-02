#version 330 core

in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

uniform vec4 u_color;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform sampler2D u_light_texture;
uniform int u_use_lighting;
uniform vec2 u_light_scale;
uniform vec2 u_light_offset;

void main()
{
    vec3 n = normalize(v_normal);
    
    vec3 lighting;
    
    if (u_use_lighting != 0) {
        // Sample light map at entity's XZ position
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
        
        // Apply some directional shading on top of dynamic light
        float ndotl = max(dot(n, normalize(u_light_dir)), 0.0);
        float shading = 0.6 + 0.4 * ndotl;  // Softer directional influence
        
        lighting = dynamic_light * shading;
    } else {
        // Fallback to simple directional lighting
        float ndotl = max(dot(n, normalize(u_light_dir)), 0.0);
        vec3 diffuse = u_light_color * ndotl;
        lighting = u_ambient + diffuse;
    }
    
    frag_color = vec4(u_color.rgb * lighting, u_color.a);
}
