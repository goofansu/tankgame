#version 330 core

// Track rendering fragment shader
// Renders track marks that darken the accumulation texture

in vec2 v_texcoord;
out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec4 u_color;        // Track color (dark gray with alpha)
uniform int u_use_texture;   // Whether to use texture or just color

void main()
{
    vec4 color = u_color;
    
    if (u_use_texture != 0) {
        // Sample track texture and multiply with color
        vec4 tex = texture(u_texture, v_texcoord);
        color.a *= tex.a;  // Use texture alpha for shape
    }
    
    // Output: the darker the track, the lower the RGB
    // Alpha controls blend amount
    frag_color = color;
}
