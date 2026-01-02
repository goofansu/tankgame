#version 330 core

// Lightmap fragment shader
// Outputs light contribution for additive blending

in vec3 v_color;
in float v_intensity;

out vec4 frag_color;

void main()
{
    // Output light color multiplied by intensity
    // Additive blending will accumulate multiple lights
    frag_color = vec4(v_color * v_intensity, 1.0);
}
