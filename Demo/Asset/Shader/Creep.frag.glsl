#version 450
#extension GL_ARB_shading_language_include : require

#include "Creep.glsl"

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec3 InViewDir;
layout(location = 3) flat in uint InIndex;

layout(location = 0) out vec4 OutColor;

void main()
{
    float Shade = pow((dot(InViewDir, InNormal) * -1.0f + 1.0f) / 2.0f, 3.0f) * 0.5f + 0.25f;
    OutColor = vec4(vec3(Creeps[InIndex].Color * Shade), 1.0f);
}
