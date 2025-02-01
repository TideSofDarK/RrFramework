#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec3 out_color;

layout(set = 1, binding = 3) uniform VTest {
    mat4x4 Padding1;
    mat4x4 Padding2;
    mat4x4 Padding3;
    mat4x4 Padding4;
    float Offset;
} u_vtest;

void main()
{
    gl_Position = in_position;
    gl_Position.x += u_vtest.Offset;
    gl_Position.y += u_vtest.Offset;
    out_color = in_color.xyz;
}
