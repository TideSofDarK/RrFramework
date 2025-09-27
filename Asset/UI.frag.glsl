#version 460
#extension GL_ARB_shading_language_include : require

#include "UI.glsl"

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

float Median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange() {
    vec2 UnitRange = vec2(DistanceRange) / vec2(textureSize(Atlas, 0));
    vec2 ScreenTexSize = vec2(1.0) / fwidth(InUV);
    return max(0.5 * dot(UnitRange, ScreenTexSize), 1.0);
}

void main() {
    vec3 Msd = texture(Atlas, InUV).rgb;
    float Distance = Median(Msd.r, Msd.g, Msd.b);
    float ScreenPxDistance = ScreenPxRange() * (Distance - 0.5);
    float Opacity = clamp(ScreenPxDistance + 0.5, 0.0, 1.0);
    OutColor = length(fwidth(InUV)) < 0.001f ? InColor : vec4(InColor.rgb, Opacity);
}
