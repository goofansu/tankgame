// ============================================================================
// Background Shader
// Renders a fullscreen quad with solid color, vertical gradient, or radial gradient
// ============================================================================

@vs background_vs
layout(location=0) in vec2 a_position;

layout(location=0) out vec2 v_uv;

void main() {
    // a_position is in range [-1, 1]
    gl_Position = vec4(a_position, 0.9999, 1.0);  // Far back in depth
    // UV: (0,0) at bottom-left, (1,1) at top-right
    v_uv = a_position * 0.5 + 0.5;
}
@end

@fs background_fs
layout(std140, binding=0) uniform background_fs_params {
    vec3 u_color_start;     // Top color (vertical) or center color (radial)
    vec3 u_color_end;       // Bottom color (vertical) or edge color (radial)
    int u_gradient_type;    // 0 = solid, 1 = vertical, 2 = radial
    vec2 u_aspect;          // (1.0, height/width) for radial to be circular
};

layout(location=0) in vec2 v_uv;
layout(location=0) out vec4 frag_color;

void main() {
    vec3 color;
    
    if (u_gradient_type == 0) {
        // Solid color
        color = u_color_start;
    } else if (u_gradient_type == 1) {
        // Vertical gradient (top to bottom)
        // v_uv.y = 1 at top, 0 at bottom
        float t = v_uv.y;
        color = mix(u_color_end, u_color_start, t);
    } else {
        // Radial gradient (center to edge)
        vec2 centered = (v_uv - 0.5) * 2.0;  // Range [-1, 1]
        centered *= u_aspect;  // Correct for aspect ratio
        float dist = length(centered);
        float t = clamp(dist, 0.0, 1.0);
        color = mix(u_color_start, u_color_end, t);
    }
    
    frag_color = vec4(color, 1.0);
}
@end

@program background background_vs background_fs
