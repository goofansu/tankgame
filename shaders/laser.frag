#version 330 core

in vec2 v_texcoord;

out vec4 frag_color;

uniform vec4 u_color;

void main() {
    // Fade towards edges (v_texcoord.x is 0 at center, 1 at edges)
    float edge_fade = 1.0 - abs(v_texcoord.x * 2.0 - 1.0);
    edge_fade = pow(edge_fade, 0.5); // Softer falloff
    
    // Fade along length (v_texcoord.y is 0 at start, 1 at end)
    float length_fade = 1.0 - v_texcoord.y * 0.3; // Slight fade towards end
    
    float alpha = u_color.a * edge_fade * length_fade;
    frag_color = vec4(u_color.rgb, alpha);
}
