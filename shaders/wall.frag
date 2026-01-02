#version 330 core

in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

uniform sampler2D u_texture_top;
uniform sampler2D u_texture_side;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;

void main()
{
    // Determine if this is a top or side face based on normal
    vec3 n = normalize(v_normal);
    float is_top = step(0.5, n.y);  // 1.0 if top face, 0.0 otherwise
    
    // Sample appropriate texture
    vec4 tex_top = texture(u_texture_top, v_texcoord);
    vec4 tex_side = texture(u_texture_side, v_texcoord);
    vec4 tex_color = mix(tex_side, tex_top, is_top);
    
    // Simple directional lighting
    float ndotl = max(dot(n, normalize(u_light_dir)), 0.0);
    vec3 diffuse = u_light_color * ndotl;
    vec3 lighting = u_ambient + diffuse;
    
    frag_color = vec4(tex_color.rgb * lighting, tex_color.a);
}
