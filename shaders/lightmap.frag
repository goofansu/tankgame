#version 330 core

// Lightmap fragment shader
// Outputs light contribution for additive blending
// Calculates falloff based on actual fragment distance from light center

in vec3 v_color;
in float v_base_intensity;
in vec2 v_uv_position;

// Light parameters (set per-light before drawing)
uniform vec2 u_light_center_uv;  // Light center in UV space
uniform float u_light_radius_uv; // Light radius in UV space
uniform float u_spotlight_factor; // Pre-computed spotlight factor (1.0 for point lights)

out vec4 frag_color;

void main()
{
    // Calculate actual distance from light center
    float dist = length(v_uv_position - u_light_center_uv);
    
    // Distance-based falloff (quadratic)
    float dist_factor = 1.0 - (dist / u_light_radius_uv);
    dist_factor = clamp(dist_factor, 0.0, 1.0);
    dist_factor = dist_factor * dist_factor; // Quadratic falloff
    
    // Final intensity
    float intensity = v_base_intensity * dist_factor;
    
    // Output light color multiplied by intensity
    frag_color = vec4(v_color * intensity, 1.0);
}
