#version 460
#extension GL_ARB_shading_language_include : require

#include "Quad.glsl"

#define CIRCLE 0
#define CROSS 1
#define RECT_SELECTION 2
#define RECT_TREE_BORDER 3

layout(location = 0) in vec2 InUV;
layout(location = 1) flat in uint InIndex;

layout(location = 0) out vec4 OutColor;

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

    if (Draw.Type == CIRCLE)
    {
        float Feather = abs(dFdx(InUV.x));
        float Distance = length(InUV - 0.5);
        float Radius = 0.5 - Feather * 2.0;
        float Weight = 1.0 - linearstep(Radius - Feather, Radius + Feather, Distance);
        Weight *= 0.5;
        Weight += linearstep(1.0 - Feather * 1.5, 1.0, 1.0 - abs(Distance - Radius));
        OutColor = vec4(Color, Weight);
    }
    else if (Draw.Type == CROSS)
    {
        float Hor = 1.0 - abs(InUV.x * 2.0 - 1.0);
        float Vert = 1.0 - abs(InUV.y * 2.0 - 1.0);
        vec2 Feather = vec2(dFdx(InUV.x), dFdy(InUV.y)) * 3.0;
        float Total = max(
                linearstep(1.0f - Feather.x, 1.0f, Hor),
                linearstep(1.0f - Feather.y, 1.0f, Vert));
        OutColor = vec4(0.65, 0.65, 0.65, Total);
    }
    else if (Draw.Type == RECT_SELECTION)
    {
        float Hor = abs(InUV.x * 2.0 - 1.0);
        float Vert = abs(InUV.y * 2.0 - 1.0);
        vec2 Feather = vec2(dFdx(InUV.x), dFdy(InUV.y)) * 3.0;
        float Total = max(
                linearstep(1.0f - Feather.x * 1.5, 1.0f - Feather.x * 0.5, Hor),
                linearstep(1.0f - Feather.y * 1.5, 1.0f - Feather.y * 0.5, Vert));
        OutColor = vec4(Color, 0.5f + Total);
    }
    else if (Draw.Type == RECT_TREE_BORDER)
    {
        float Hor = abs(InUV.x * 2.0 - 1.0);
        float Vert = abs(InUV.y * 2.0 - 1.0);
        vec2 Feather = vec2(dFdx(InUV.x), dFdy(InUV.y)) * 3.0;
        float Total = max(
                linearstep(1.0f - Feather.x * 1.5, 1.0f - Feather.x * 0.5, Hor),
                linearstep(1.0f - Feather.y * 1.5, 1.0f - Feather.y * 0.5, Vert));
        OutColor = vec4(Color, Total);
    }
}
