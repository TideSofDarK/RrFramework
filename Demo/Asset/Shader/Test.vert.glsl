#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec3 out_color;

void main()
{
    gl_Position = in_position;
    out_color = in_color.xyz;
}
