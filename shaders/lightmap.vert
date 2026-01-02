#version 330 core

// Lightmap vertex shader
// Renders light geometry to the light map texture

layout(location = 0) in vec2 a_position;   // UV-space position (0-1)
layout(location = 1) in vec3 a_color;      // Light color
layout(location = 2) in float a_intensity; // Base intensity (at center)

out vec3 v_color;
out float v_base_intensity;
out vec2 v_uv_position;  // Pass UV position for distance calculation in frag

void main()
{
    // Convert UV (0-1) to NDC (-1 to 1)
    vec2 ndc = a_position * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    
    v_color = a_color;
    v_base_intensity = a_intensity;
    v_uv_position = a_position;
}
