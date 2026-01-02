#version 330 core

// Lightmap fragment shader
// Outputs light contribution for additive blending
// Calculates falloff based on actual fragment distance from light center

in vec3 v_color;
in float v_base_intensity;
in vec2 v_uv_position;

// Light parameters (set per-light before drawing)
uniform vec2 u_light_center_uv;  // Light center in UV space
uniform float u_light_radius;    // Light radius in world units
uniform vec2 u_world_size;       // World dimensions for UV to world conversion

out vec4 frag_color;

void main()
{
    // Convert UV delta to world-space distance for circular falloff
    // UV delta * world_size = world-space delta
    vec2 uv_delta = v_uv_position - u_light_center_uv;
    vec2 world_delta = uv_delta * u_world_size;
    float dist = length(world_delta);
    
    // Distance-based falloff (quadratic)
    float dist_factor = 1.0 - (dist / u_light_radius);
    dist_factor = clamp(dist_factor, 0.0, 1.0);
    dist_factor = dist_factor * dist_factor; // Quadratic falloff
    
    // Final intensity
    float intensity = v_base_intensity * dist_factor;
    
    // Output light color multiplied by intensity
    frag_color = vec4(v_color * intensity, 1.0);
}
