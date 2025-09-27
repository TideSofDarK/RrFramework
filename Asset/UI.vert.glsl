#version 460
#extension GL_ARB_shading_language_include : require

#include "UI.glsl"

layout(location = 0) in vec2 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec4 InColor;

layout(location = 0) out vec2 OutUV;
layout(location = 1) out vec4 OutColor;

const vec2 UVs[4] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main()
{
    gl_Position = vec4((InPosition / ScreenSize) * 2.0 - 1.0, 0.0, 1.0);
    OutUV = InUV;
    OutColor = InColor;
}
