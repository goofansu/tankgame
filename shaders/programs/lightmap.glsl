@vs lightmap_vs
layout(location=0) in vec2 a_position;
layout(location=1) in vec3 a_color;
layout(location=2) in float a_intensity;

layout(location=0) out vec3 v_color;
layout(location=1) out float v_base_intensity;
layout(location=2) out vec2 v_uv_position;

void main() {
    vec2 ndc = a_position * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
    v_base_intensity = a_intensity;
    v_uv_position = a_position;
}
@end

@fs lightmap_fs
layout(std140, binding=0) uniform lightmap_fs_params {
    vec2 u_light_center_uv;
    float u_light_radius;
    vec2 u_world_size;
};

layout(location=0) in vec3 v_color;
layout(location=1) in float v_base_intensity;
layout(location=2) in vec2 v_uv_position;
layout(location=0) out vec4 frag_color;

void main() {
    vec2 uv_delta = v_uv_position - u_light_center_uv;
    vec2 world_delta = uv_delta * u_world_size;
    float dist = length(world_delta);

    float dist_factor = 1.0 - (dist / u_light_radius);
    dist_factor = clamp(dist_factor, 0.0, 1.0);
    dist_factor = dist_factor * dist_factor;

    float intensity = v_base_intensity * dist_factor;
    frag_color = vec4(v_color * intensity, 1.0);
}
@end

@program lightmap lightmap_vs lightmap_fs

