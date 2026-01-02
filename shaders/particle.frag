#version 330 core

in vec2 v_texcoord;
in float v_alpha;
in vec3 v_color;

out vec4 frag_color;

uniform sampler2D u_texture;

void main()
{
    // Sample the smoke texture
    vec4 tex = texture(u_texture, v_texcoord);
    
    // The texture stores:
    // - RGB: brightness value indicating cel-shade band (1.0 = light, 0.15 = outline)
    // - A: cloud mask (1.0 = inside cloud, 0.0 = outside)
    
    // Discard fully transparent pixels
    if (tex.a < 0.01) {
        discard;
    }
    
    float brightness = tex.r;
    
    // Map brightness to cel-shaded color bands
    // Bright areas (>0.7) = full color (light band)
    // Mid areas (0.35-0.7) = 75% color (shadow band)  
    // Dark areas (<0.35) = 50% color (outline band)
    vec3 final_color;
    if (brightness > 0.7) {
        // Light band - bright color
        final_color = v_color;
    } else if (brightness > 0.35) {
        // Mid/shadow band
        final_color = v_color * 0.75;
    } else {
        // Dark outline band - but still colored, not black!
        final_color = v_color * 0.5;
    }
    
    // Apply alpha from texture and uniform
    float final_alpha = tex.a * v_alpha;
    
    // Standard alpha blending output (not premultiplied)
    frag_color = vec4(final_color, final_alpha);
}
