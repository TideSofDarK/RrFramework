#version 450
#extension GL_ARB_shading_language_include : require

#include "Quad.glsl"

layout(location = 0) in vec2 InUV;
layout(location = 1) flat in uint InIndex;

layout(location = 0) out vec4 OutColor;

const float CIRCLE_FEATHER_RATIO = 1.0f;

float linearstep(float A, float B, float X)
{
    float T = (X - A) / (B - A);
    return clamp(T, 0.0, 1.0);
}

void main()
{
    SGPUDraw Draw = Draws[InIndex];

    vec3 Color = vec3(float(Draw.Color & 0x000000ff) / 255.0f,
            float((Draw.Color & 0x0000ff00) >> 8) / 255.0f,
            float((Draw.Color & 0x00ff0000) >> 16) / 255.0f);

    if (Draw.Type == 0)
    {
        float Feather = CIRCLE_FEATHER_RATIO * abs(dFdx(InUV.x));
        float Distance = length(InUV - 0.5);
        float Radius = 0.5 - Feather;
        float Weight = 1.0 - linearstep(Radius - Feather, Radius + Feather, Distance);
        OutColor = vec4(Color, Weight * 0.75f);
    }
    else if (Draw.Type == 1)
    {
        float Hor = 1.0 - abs(InUV.x * 2.0 - 1.0);
        float Vert = 1.0 - abs(InUV.y * 2.0 - 1.0);
        vec2 Feather = vec2(dFdx(InUV.x), dFdy(InUV.y)) * 3.0;
        float Total = max(
                linearstep(1.0f - Feather.x, 1.0f, Hor),
                linearstep(1.0f - Feather.y, 1.0f, Vert));
        OutColor = vec4(0.0, 0.0, 0.0, Total);
    }
    else if (Draw.Type == 2)
    {
        float Hor = abs(InUV.x * 2.0 - 1.0);
        float Vert = abs(InUV.y * 2.0 - 1.0);
        vec2 Feather = vec2(dFdx(InUV.x), dFdy(InUV.y)) * 3.0;
        float Total = max(
                linearstep(1.0f - Feather.x * 1.5, 1.0f - Feather.x * 0.5, Hor),
                linearstep(1.0f - Feather.y * 1.5, 1.0f - Feather.y * 0.5, Vert));
        OutColor = vec4(Color, 0.5f + Total);
    }
}
