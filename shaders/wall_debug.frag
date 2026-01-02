#version 330 core

in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

void main()
{
    vec3 n = normalize(v_normal);
    
    // Color based on normal direction
    // +Y (top) = green
    // -Z (front/toward camera) = blue
    // +Z (back) = cyan
    // -X (left) = red
    // +X (right) = yellow
    
    vec3 color = vec3(0.5);
    
    if (n.y > 0.5) {
        color = vec3(0.0, 1.0, 0.0);  // Top = green
    } else if (n.z < -0.5) {
        color = vec3(0.0, 0.0, 1.0);  // -Z (toward camera) = blue
    } else if (n.z > 0.5) {
        color = vec3(0.0, 1.0, 1.0);  // +Z (away from camera) = cyan
    } else if (n.x < -0.5) {
        color = vec3(1.0, 0.0, 0.0);  // -X (left) = red
    } else if (n.x > 0.5) {
        color = vec3(1.0, 1.0, 0.0);  // +X (right) = yellow
    }
    
    frag_color = vec4(color, 1.0);
}
