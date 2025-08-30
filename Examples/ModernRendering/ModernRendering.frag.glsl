#version 450

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 View;
    mat4 Projection;
    float Time;
};

void main()
{
    OutColor = vec4(InUV, 0.0, 1.0);
}
