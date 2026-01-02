#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;

out vec2 v_texcoord;
out float v_alpha;
out vec3 v_color;

uniform mat4 u_mvp;
uniform float u_alpha;
uniform vec3 u_color;

void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_alpha = u_alpha;
    v_color = u_color;
}
