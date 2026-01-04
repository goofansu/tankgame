// =============================================================================
// Blended Ground Shader - smooth terrain transitions using texture array
// =============================================================================

@vs ground_blend_vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;
layout(location=2) in float a_terrain_idx;     // Primary terrain index
layout(location=3) in vec4 a_neighbor_idx;     // N, E, S, W neighbor indices (-1 = same/wall)

layout(std140, binding=0) uniform ground_blend_vs_params {
    mat4 u_mvp;
};

layout(location=0) out vec2 v_texcoord;
layout(location=1) out vec3 v_world_pos;
layout(location=2) out float v_terrain_idx;
layout(location=3) out vec4 v_neighbor_idx;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_world_pos = a_position;
    v_terrain_idx = a_terrain_idx;
    v_neighbor_idx = a_neighbor_idx;
}
@end

@fs ground_blend_fs
layout(binding=0) uniform texture2DArray u_terrain_array;
layout(binding=0) uniform sampler u_terrain_array_smp;
layout(binding=1) uniform texture2D u_track_texture;
layout(binding=1) uniform sampler u_track_texture_smp;
layout(binding=2) uniform texture2D u_light_texture;
layout(binding=2) uniform sampler u_light_texture_smp;

layout(std140, binding=1) uniform ground_blend_fs_params {
    int u_use_tracks;
    int u_use_lighting;
    vec2 u_track_scale;
    vec2 u_track_offset;
    vec2 u_light_scale;
    vec2 u_light_offset;
    int u_has_sun;
    vec3 u_sun_direction;
    vec3 u_sun_color;
    float u_blend_sharpness;  // Controls how sharp/fuzzy the blend is (0.1 = very fuzzy, 0.5 = sharp)
    float u_noise_scale;       // Scale of the noise pattern
    float u_tile_size;         // Size of one tile in world units
    vec2 u_map_offset;         // Offset to align world pos to tile grid
};

layout(location=0) in vec2 v_texcoord;
layout(location=1) in vec3 v_world_pos;
layout(location=2) in float v_terrain_idx;
layout(location=3) in vec4 v_neighbor_idx;
layout(location=0) out vec4 frag_color;

// Simple noise function for organic transitions
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);  // Smoothstep
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    // Sample primary terrain
    int primary_idx = int(v_terrain_idx + 0.5);
    vec4 terrain = texture(sampler2DArray(u_terrain_array, u_terrain_array_smp), 
                           vec3(v_texcoord, float(primary_idx)));
    
    // Calculate position within tile using world coordinates
    // u_map_offset aligns world position to tile grid origin
    vec2 world_tile_pos = (v_world_pos.xz + u_map_offset) / u_tile_size;
    vec2 tile_pos = fract(world_tile_pos);  // 0-1 position within tile
    
    // Handle edge case: fract() at exact integers can give 0 or ~1 due to precision
    // Clamp to avoid artifacts at tile boundaries
    tile_pos = clamp(tile_pos, 0.001, 0.999);
    
    // Get noise value for organic blending
    float noise_val = fbm(v_world_pos.xz * u_noise_scale, 3);
    
    // Blend distance from edge (how far into the tile the blend extends)
    float blend_dist = 0.5;  // 50% of tile width - blend zone extends into tile
    
    // Calculate blend weights for each neighbor
    // The key insight: blend weight should be 0.5 AT the tile boundary (not 1.0)
    // This way both tiles show 50/50 at their shared edge and meet smoothly
    // Weight goes from 0.5 at edge to 0.0 at blend_dist into the tile
    
    vec4 neighbor_weights = vec4(0.0);
    
    // North blend (v_neighbor_idx.x) - edge at tile_pos.y = 0 (top of tile in UV space)
    if (v_neighbor_idx.x >= 0.0) {
        float dist_from_edge = tile_pos.y;  // 0 at north edge, 1 at south
        float noise_offset = (noise_val - 0.5) * blend_dist * 0.6;
        // Weight is 0.5 at edge (dist=0), 0 at blend_dist
        float raw_weight = 1.0 - smoothstep(0.0, blend_dist + noise_offset, dist_from_edge);
        neighbor_weights.x = raw_weight * 0.5;  // Max 0.5 at boundary
    }
    
    // South blend (v_neighbor_idx.z) - edge at tile_pos.y = 1 (bottom of tile)
    if (v_neighbor_idx.z >= 0.0) {
        float dist_from_edge = 1.0 - tile_pos.y;  // 0 at south edge
        float noise_offset = (noise_val - 0.5) * blend_dist * 0.6;
        float raw_weight = 1.0 - smoothstep(0.0, blend_dist + noise_offset, dist_from_edge);
        neighbor_weights.z = raw_weight * 0.5;
    }
    
    // West blend (v_neighbor_idx.w) - edge at tile_pos.x = 0 (left of tile)
    if (v_neighbor_idx.w >= 0.0) {
        float dist_from_edge = tile_pos.x;  // 0 at west edge
        float noise_offset = (noise_val - 0.5) * blend_dist * 0.6;
        float raw_weight = 1.0 - smoothstep(0.0, blend_dist + noise_offset, dist_from_edge);
        neighbor_weights.w = raw_weight * 0.5;
    }
    
    // East blend (v_neighbor_idx.y) - edge at tile_pos.x = 1 (right of tile)
    if (v_neighbor_idx.y >= 0.0) {
        float dist_from_edge = 1.0 - tile_pos.x;  // 0 at east edge
        float noise_offset = (noise_val - 0.5) * blend_dist * 0.6;
        float raw_weight = 1.0 - smoothstep(0.0, blend_dist + noise_offset, dist_from_edge);
        neighbor_weights.y = raw_weight * 0.5;
    }
    
    // Sample and blend neighbor textures using weighted average
    // This avoids compounding issues when multiple neighbors contribute
    float total_neighbor_weight = neighbor_weights.x + neighbor_weights.y + 
                                   neighbor_weights.z + neighbor_weights.w;
    
    if (total_neighbor_weight > 0.001) {
        vec4 blended_neighbors = vec4(0.0);
        
        if (neighbor_weights.x > 0.001) {
            int n_idx = int(v_neighbor_idx.x + 0.5);
            vec4 n_tex = texture(sampler2DArray(u_terrain_array, u_terrain_array_smp),
                                 vec3(v_texcoord, float(n_idx)));
            blended_neighbors += n_tex * neighbor_weights.x;
        }
        
        if (neighbor_weights.y > 0.001) {
            int n_idx = int(v_neighbor_idx.y + 0.5);
            vec4 n_tex = texture(sampler2DArray(u_terrain_array, u_terrain_array_smp),
                                 vec3(v_texcoord, float(n_idx)));
            blended_neighbors += n_tex * neighbor_weights.y;
        }
        
        if (neighbor_weights.z > 0.001) {
            int n_idx = int(v_neighbor_idx.z + 0.5);
            vec4 n_tex = texture(sampler2DArray(u_terrain_array, u_terrain_array_smp),
                                 vec3(v_texcoord, float(n_idx)));
            blended_neighbors += n_tex * neighbor_weights.z;
        }
        
        if (neighbor_weights.w > 0.001) {
            int n_idx = int(v_neighbor_idx.w + 0.5);
            vec4 n_tex = texture(sampler2DArray(u_terrain_array, u_terrain_array_smp),
                                 vec3(v_texcoord, float(n_idx)));
            blended_neighbors += n_tex * neighbor_weights.w;
        }
        
        // Normalize and blend: primary gets (1 - total_neighbor_weight), neighbors share the rest
        // Cap total_neighbor_weight to avoid over-blending
        total_neighbor_weight = min(total_neighbor_weight, 0.5);
        blended_neighbors /= max(neighbor_weights.x + neighbor_weights.y + 
                                  neighbor_weights.z + neighbor_weights.w, 0.001);
        terrain = mix(terrain, blended_neighbors, total_neighbor_weight);
    }
    
    // Apply tracks
    if (u_use_tracks != 0) {
        vec2 track_uv = v_world_pos.xz * u_track_scale + u_track_offset;
        vec4 tracks = texture(sampler2D(u_track_texture, u_track_texture_smp), track_uv);
        terrain.rgb *= tracks.rgb;
    }
    
    // Apply lighting (same as regular ground shader)
    vec3 final_light = vec3(1.0);
    
    if (u_has_sun != 0) {
        float sun_intensity = max(0.0, -u_sun_direction.y);
        final_light = u_sun_color * sun_intensity;
    }
    
    if (u_use_lighting != 0) {
        vec2 light_uv = v_world_pos.xz * u_light_scale + u_light_offset;
        vec2 texel_size = vec2(1.0 / 256.0);
        vec3 dynamic_light = vec3(0.0);
        float total_light_weight = 0.0;
        
        for (int y = -2; y <= 2; y++) {
            for (int x = -2; x <= 2; x++) {
                vec2 offset = vec2(float(x), float(y)) * texel_size;
                float weight = 1.0 / (1.0 + float(abs(x) + abs(y)));
                dynamic_light += texture(sampler2D(u_light_texture, u_light_texture_smp),
                                         light_uv + offset).rgb * weight;
                total_light_weight += weight;
            }
        }
        dynamic_light /= total_light_weight;
        
        if (u_has_sun != 0) {
            final_light += dynamic_light * 0.5;
        } else {
            final_light = dynamic_light;
        }
    }
    
    terrain.rgb *= final_light;
    frag_color = terrain;
}
@end

@program ground_blend ground_blend_vs ground_blend_fs

