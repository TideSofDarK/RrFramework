#version 450
#extension GL_ARB_shading_language_include : require

#include "BrushFade.glsl"

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 1) uniform sampler2D Masks[2];

void main()
{
    vec4 T = texture(Masks[0], InUV);
    vec4 C = texture(Masks[1], InUV);
    OutColor.rgb = C.rgb;
    OutColor.rgb *= 1.0f + (T.r * 2.0 - 1.0) * Mix;
    float Edge = 1.0 - T.r + 0.001;
    float Min = Edge - Smoothstep;
    Min = max(0.001, Min);
    float Max = Edge + Smoothstep;
    Max = min(Max, 1.0);
    OutColor.a = step(0.001, T.r) * smoothstep(Min, Max, Time);
    OutColor.a *= T.a;
}
