#version 450
#extension GL_ARB_shading_language_include : require

#include "UI.glsl"

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

float Median(vec3 Value)
{
    return max(min(Value.r, Value.g), min(max(Value.r, Value.g), Value.b));
}

float GetDistance(vec2 UV)
{
    return Median(texture(Atlas, UV).rgb);
}

vec4 MSDFGlyph(vec2 DXDY)
{
    float Weight = 0.0f;
    float Distance = GetDistance(InUV) + min(Weight, 0.5f - 1.0f / DistanceRange) - 0.5f;
    float Opacity = clamp(Distance * DistanceRange / length(DXDY) + 0.5f, 0.0f, 1.0f);

    return vec4(InColor.rgb, Opacity);
}

void main()
{
    vec2 DXDY = fwidth(InUV) * textureSize(Atlas, 0);
    OutColor = (DXDY.x + DXDY.y) != 0.0f ? MSDFGlyph(DXDY) : vec4(InColor);
}
