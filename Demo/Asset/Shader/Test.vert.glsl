#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec4 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec4 InColor;

layout(location = 0) out vec3 OutColor;
layout(location = 1) out vec2 OutUV;

layout(set = 1, binding = 3) uniform VTest {
    mat4x4 Padding1;
    mat4x4 Padding2;
    mat4x4 Padding3;
    mat4x4 Padding4;
    float Offset;
} UniformTest;

void main()
{
    gl_Position = InPosition;
    gl_Position.x += UniformTest.Offset;
    gl_Position.y += UniformTest.Offset;
    OutColor = InColor.xyz;
    OutUV = InUV;
}
