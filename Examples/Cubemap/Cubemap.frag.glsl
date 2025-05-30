#version 450
#extension GL_ARB_shading_language_include : require

#include "Cubemap.glsl"

// layout(location = 0) in vec3 InNear;
// layout(location = 1) in vec3 InFar;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 1) uniform samplerCube UniformCube;

void main()
{
    OutColor = vec4(1.0);
}
