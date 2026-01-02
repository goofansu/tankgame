#version 330 core

in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

uniform sampler2D u_texture_top;
uniform sampler2D u_texture_side;
uniform sampler2D u_light_texture;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform int u_use_lighting;
uniform vec2 u_light_scale;
uniform vec2 u_light_offset;

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
    vec3 lighting;
    
    if (u_use_lighting != 0 && is_top < 0.5) {
        // Side faces: sample light map at base of wall (y=0 in world space)
        vec2 light_uv = v_world_pos.xz * u_light_scale + u_light_offset;
        vec3 dynamic_light = texture(u_light_texture, light_uv).rgb;
        
        // Apply normal-based shading to dynamic light
        // Sides facing the light get more illumination
        float side_factor = 0.7 + 0.3 * max(0.0, dot(n.xz, normalize(vec2(0.4, 0.3))));
        lighting = dynamic_light * side_factor;
    } else {
        // Top faces: use simple directional lighting (no dynamic shadows)
        float ndotl = max(dot(n, normalize(u_light_dir)), 0.0);
        vec3 diffuse = u_light_color * ndotl;
        lighting = u_ambient + diffuse;
    }
    
    frag_color = vec4(tex_color.rgb * lighting, tex_color.a);
}
