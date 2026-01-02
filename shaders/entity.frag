#version 330 core

in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

uniform vec4 u_color;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;

void main()
{
    vec3 n = normalize(v_normal);
    
    // Simple directional lighting
    float ndotl = max(dot(n, normalize(u_light_dir)), 0.0);
    vec3 diffuse = u_light_color * ndotl;
    vec3 lighting = u_ambient + diffuse;
    
    frag_color = vec4(u_color.rgb * lighting, u_color.a);
}
