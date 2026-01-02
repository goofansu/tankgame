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
        vec3 dynamic_light = texture(u_light_texture, light_uv).rgb;
        
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
