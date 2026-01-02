#version 330 core

// Ground rendering fragment shader
// Samples terrain texture and track accumulation texture

in vec2 v_texcoord;
in vec3 v_world_pos;

out vec4 frag_color;

uniform sampler2D u_texture;       // Terrain texture
uniform sampler2D u_track_texture; // Track accumulation texture
uniform int u_use_tracks;          // Whether track texture is bound
uniform vec2 u_track_scale;        // World-to-UV scale for tracks
uniform vec2 u_track_offset;       // World-to-UV offset for tracks

void main()
{
    // Sample terrain texture
    vec4 terrain = texture(u_texture, v_texcoord);
    
    // Sample track texture if available
    if (u_use_tracks != 0) {
        // Convert world XZ to track texture UV
        vec2 track_uv = v_world_pos.xz * u_track_scale + u_track_offset;
        vec4 tracks = texture(u_track_texture, track_uv);
        
        // Multiply terrain color by track color (tracks darken the ground)
        // The track texture is white where no tracks, darker where tracks are
        terrain.rgb *= tracks.rgb;
    }
    
    frag_color = terrain;
}
