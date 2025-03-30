#version 450
#extension GL_ARB_shading_language_include : require

#include "UI.glsl"

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InColor;

layout(location = 0) out vec4 OutColor;

void main()
{
    OutColor = vec4(0.0);
}
