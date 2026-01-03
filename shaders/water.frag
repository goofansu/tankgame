#version 330 core

// Water rendering fragment shader
// Creates animated water with caustic lines, similar to Wind Waker style

in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

uniform float u_time;
uniform vec3 u_water_color;      // Base water color
uniform vec3 u_water_dark;       // Darker shade for caustic lines
uniform vec3 u_water_highlight;  // Brighter shade for highlights
uniform sampler2D u_light_texture;
uniform int u_use_lighting;
uniform vec2 u_light_scale;
uniform vec2 u_light_offset;

#define SCALE 4.0

// Calculate animated wave pattern
float calculateSurface(float x, float z, float t)
{
    float y = 0.0;
    // Reduced amplitude (0.3 instead of 1.0) for subtler waves
    y += (sin(x * 1.0 / SCALE + t * 0.5) + sin(x * 2.3 / SCALE + t * 0.75) + sin(x * 3.3 / SCALE + t * 0.2)) / 3.0 * 0.3;
    y += (sin(z * 0.2 / SCALE + t * 0.9) + sin(z * 1.8 / SCALE + t * 0.9) + sin(z * 2.8 / SCALE + t * 0.4)) / 3.0 * 0.3;
    return y;
}

// Hash function for caustics
vec2 hash2(vec2 p)
{
    return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}

// Voronoi distance for caustic pattern
float voronoi(vec2 p)
{
    vec2 n = floor(p);
    vec2 f = fract(p);
    
    float md = 8.0;
    
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash2(n + g);
            vec2 r = g + o - f;
            float d = dot(r, r);
            
            if (d < md) {
                md = d;
            }
        }
    }
    return sqrt(md);
}

// Second closest for cell edges (caustic lines)
float voronoiEdge(vec2 p)
{
    vec2 n = floor(p);
    vec2 f = fract(p);
    
    float md = 8.0;
    float md2 = 8.0;
    
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash2(n + g);
            vec2 r = g + o - f;
            float d = dot(r, r);
            
            if (d < md) {
                md2 = md;
                md = d;
            } else if (d < md2) {
                md2 = d;
            }
        }
    }
    return sqrt(md2) - sqrt(md);
}

void main()
{
    // Animated UV for caustics
    vec2 uv = v_world_pos.xz * 0.5 + vec2(u_time * -0.03);
    
    // Distort UVs with wave motion
    uv.y += 0.01 * (sin(uv.x * 3.5 + u_time * 0.35) + sin(uv.x * 4.8 + u_time * 1.05) + sin(uv.x * 7.3 + u_time * 0.45)) / 3.0;
    uv.x += 0.08 * (sin(uv.y * 4.0 + u_time * 0.5) + sin(uv.y * 6.8 + u_time * 0.75) + sin(uv.y * 11.3 + u_time * 0.2)) / 3.0;
    uv.y += 0.08 * (sin(uv.x * 4.2 + u_time * 0.64) + sin(uv.x * 6.3 + u_time * 1.65) + sin(uv.x * 8.2 + u_time * 0.45)) / 3.0;
    
    // Calculate caustic patterns
    float voronoiVal = voronoi(uv * 2.0);
    float edge = voronoiEdge(uv * 2.0);
    
    // Create caustic highlight lines
    float causticLine = smoothstep(0.02, 0.08, edge);
    float highlight = 1.0 - causticLine;
    
    // Darker areas between caustics
    float darkArea = smoothstep(0.3, 0.5, voronoiVal);
    
    // Wave-based shading
    float wave = calculateSurface(v_world_pos.x, v_world_pos.z, u_time);
    float waveShade = 0.5 + wave * 0.5;
    
    // Combine colors
    vec3 color = u_water_color;
    
    // Add highlights (bright caustic lines)
    color = mix(color, u_water_highlight, highlight * 0.6);
    
    // Add dark lines 
    color = mix(color, u_water_dark, (1.0 - causticLine) * 0.3);
    
    // Subtle wave-based variation
    color *= (0.85 + waveShade * 0.15);
    
    // Apply dynamic lighting from light map if available
    if (u_use_lighting != 0) {
        vec2 light_uv = v_world_pos.xz * u_light_scale + u_light_offset;
        
        // Sample with box blur to soften edges
        vec2 texel_size = vec2(1.0 / 256.0);
        vec3 dynamic_light = vec3(0.0);
        float total_weight = 0.0;
        
        for (int y = -2; y <= 2; y++) {
            for (int x = -2; x <= 2; x++) {
                vec2 offset = vec2(float(x), float(y)) * texel_size;
                float weight = 1.0 / (1.0 + float(abs(x) + abs(y)));
                dynamic_light += texture(u_light_texture, light_uv + offset).rgb * weight;
                total_weight += weight;
            }
        }
        dynamic_light /= total_weight;
        
        // Water reflects more light, so boost it a bit
        color *= dynamic_light * 1.2;
    }
    
    frag_color = vec4(color, 1.0);
}
