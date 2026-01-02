#version 330 core

// Track rendering vertex shader
// Input coordinates are in UV space (0-1), converted to NDC

layout(location = 0) in vec2 a_position;  // Position in UV space
layout(location = 1) in vec2 a_texcoord;  // Texture coordinate

out vec2 v_texcoord;

void main()
{
    // Convert UV (0-1) to NDC (-1 to 1)
    vec2 ndc = a_position * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_texcoord = a_texcoord;
}
