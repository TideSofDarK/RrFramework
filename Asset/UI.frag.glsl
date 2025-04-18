#version 450
#extension GL_ARB_shading_language_include : require

#include "UI.glsl"

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float GetDistance(vec2 uv)
{
    vec3 msdf = texture(Atlas, uv).rgb;
    return Median(msdf.r, msdf.g, msdf.b);
}

vec4 MSDFGlyph()
{
    vec2 DXDY = fwidth(InUV) * textureSize(Atlas, 0);
    float Weight = 0.0f;
    float Distance = GetDistance(InUV) + min(Weight, 0.5f - 1.0f / DistanceRange) - 0.5f;
    float Opacity = clamp(Distance * DistanceRange / length(DXDY) + 0.5f, 0.0f, 1.0f);

    return vec4(InColor.rgb, Opacity);
}

void main()
{
    OutColor = InUV.x > 0.0f ? MSDFGlyph() : vec4(InColor);
}
