#version 450
#extension GL_ARB_shading_language_include : require

#include "Quad.glsl"

layout(location = 0) in vec2 InUV;
layout(location = 1) flat in uint InIndex;

layout(location = 0) out vec4 OutColor;

const float FEATHER_RATIO = 2.0f;

void main()
{
    SGPUDraw Draw = Draws[InIndex];

    vec3 Color = vec3(float(Draw.Color & 0x000000ff) / 255.0f,
            float((Draw.Color & 0x0000ff00) >> 8) / 255.0f,
            float((Draw.Color & 0x00ff0000) >> 16) / 255.0f);

    float Feather = FEATHER_RATIO * abs(dFdx(InUV.x));

    if (Draw.Type == 0)
    {
        float Distance = length(InUV - 0.5);
        float Radius = 0.5 - Feather;
        float Weight = 1.0 - smoothstep(Radius - Feather, Radius + Feather, Distance);
        OutColor = vec4(Color, Weight * 0.75f);
    }
    else if (Draw.Type == 1)
    {
        float Hor = abs(InUV.x * 2.0 - 1.0);
        float Vert = abs(InUV.y * 2.0 - 1.0);
        float Total = (Hor < Feather ? 1.0 : 0.0) + (Vert < Feather ? 1.0 : 0.0);
        OutColor = vec4(0.0, 0.0, 0.0, Total);
    }
}
