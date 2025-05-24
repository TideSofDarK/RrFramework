#version 450
#extension GL_ARB_shading_language_include : require

#include "SmoothGrid.glsl"

layout(location = 0) in vec3 InNear;
layout(location = 1) in vec3 InFar;

layout(location = 0) out vec4 OutColor;

void main()
{
    float Alpha = -InNear.y / (InFar.y - InNear.y);
    vec3 FragPos = InNear + (InFar - InNear) * Alpha;

    vec2 Coord = FragPos.xz * GridSize;
    vec2 Derivative = fwidth(Coord);
    vec2 Grid = abs(fract(Coord - 0.5) - 0.5) / Derivative;
    float Line = min(Grid.x, Grid.y);
    float MinZ = min(Derivative.y, 0.5);
    float MinX = min(Derivative.x, 0.5);
    vec4 Color = vec4(0.2, 0.2, 0.2, 1.0 - min(Line, 1.0));

    if(FragPos.x > -MinX && FragPos.x < MinX)
    {
        Color.y = 1.0;
    }

    if(FragPos.z > -MinZ && FragPos.z < MinZ)
    {
        Color.x = 1.0;
    }

    vec4 ClipSpacePos = Projection * View * vec4(FragPos.xyz, 1.0);
    gl_FragDepth = ClipSpacePos.z / ClipSpacePos.w;

    Color.a *= float(Alpha > 0.0);
    OutColor = Color;
}
