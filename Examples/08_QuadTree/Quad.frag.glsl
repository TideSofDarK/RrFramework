#version 450
#extension GL_ARB_shading_language_include : require

#include "Quad.glsl"

layout(location = 0) in vec2 InUV;
layout(location = 1) flat in uint InIndex;

layout(location = 0) out vec4 OutColor;

const float THICKNESS = 1.0f;

void main()
{
    SGPUDraw Draw = Draws[InIndex];

    vec3 Color = vec3(float(Draw.Color & 0x000000ff) / 255.0f,
            float((Draw.Color & 0x0000ff00) >> 8) / 255.0f,
            float((Draw.Color & 0x00ff0000) >> 16) / 255.0f);

    float Feather = 1.1f * abs(dFdx(InUV.x));
    // float Feather = 0.01f;
    vec2 UV = InUV - 0.5;
    float Distance = length(UV);
    float Radius = 0.5 - Feather;
    float Weight = 1.0 - smoothstep(Radius - Feather, Radius + Feather, Distance);
    OutColor = vec4(Color, Weight * 0.75f);
}
