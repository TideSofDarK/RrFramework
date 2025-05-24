#version 450
#extension GL_ARB_shading_language_include : require

#include "GS.glsl"

layout(location = 0) in vec4 InColor;
layout(location = 1) in vec3 InConic;
layout(location = 2) in vec2 InCoords;
layout(location = 3) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

void main()
{
    float Power = -0.5f * (InConic.x * InCoords.x * InCoords.x + InConic.z * InCoords.y * InCoords.y) - InConic.y * InCoords.x * InCoords.y;
    if (Power > 0.0f) discard;
    float Alpha = min(0.99f, InColor.a * exp(Power));
    if (Alpha < 1.f / 255.f) discard;
    OutColor = vec4(InColor.rgb, Alpha);

    // OutColor = vec4(InColor);
    // vec2 NewUV = InUV * 2;
    // float D = dot(NewUV, NewUV);
    // float E = exp(-D);
    // OutColor.a = E * InColor.a;

    // float V = length(InUV);
    // float Vis = (step(V, 1.0));
    // Vis -= (step(V, 0.98));
    // if (Vis > 0.1)
    // {
    //     OutColor = vec4(InColor.rgb, 0.0);
    //     OutColor.a = 1.0;
    // }
}
