#version 330 core

// Lightmap vertex shader
// Renders light geometry to the light map texture

layout(location = 0) in vec2 a_position;   // UV-space position (0-1)
layout(location = 1) in vec3 a_color;      // Light color
layout(location = 2) in float a_intensity; // Light intensity at this vertex

out vec3 v_color;
out float v_intensity;

void main()
{
    // Convert UV (0-1) to NDC (-1 to 1)
    vec2 ndc = a_position * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    
    v_color = a_color;
    v_intensity = a_intensity;
}
